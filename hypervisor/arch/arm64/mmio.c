/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Data aborts of a guest at stage 2: an access to a guest-physical
 * address that stage 2 doesn't map (stage2.c). Accesses to the emulated
 * device pages (the distributor, the PL011s that aren't passed through,
 * the RTC) are emulated from the syndrome: QEMU gives the register, the
 * size and the direction of single loads and stores (ESR_EL2.ISV). The
 * first access to each register that isn't emulated is reported. Any
 * other access, or one without a syndrome, stops the VM.
 */

#include <arch/internal.h>

// ESR_EL2 of a data abort.
#define ESR_ISV             (1UL << 24)
#define ESR_SAS_SHIFT       22U
#define ESR_SAS_MASK        3UL
#define ESR_SSE             (1UL << 21)
#define ESR_SRT_SHIFT       16U
#define ESR_SRT_MASK        0x1FUL
#define ESR_SF              (1UL << 15)
#define ESR_S1PTW           (1UL << 7)
#define ESR_WNR             (1UL << 6)

#define HPFAR_FIPA_SHIFT    4U
#define PAGE_OFFSET_MASK    0xFFFUL
#define REG_XZR             31U

static const char *const device_names[HV_MMIO_DEVICES] = {
    "GIC distributor", "UART0", "UART1", "RTC"
};

/*
 * Report the first unhandled access to a register of an emulated device
 * page.
 */
static void log_unhandled(hv_vm_t *vm, uint32_t device, uint32_t offset,
                          bool write)
{
    uint32_t word = offset / 4U;
    uint8_t bit = (uint8_t) (1U << (word % 8U));
    uint8_t *logged = &vm->arch.mmio_logged[device][word / 8U];

    if ((*logged & bit) == 0U) {
        *logged |= bit;
        hv_printf("[hv] %s: %s of %s register 0x%x, which isn't emulated "
                  "(reported once)\n", vm->config->name,
                  write ? "write" : "read", device_names[device], offset);
    }
}

// The emulated device page of guest-physical address ipa, or HV_MMIO_DEVICES.
static uint32_t device_of(const hv_vm_t *vm, uint64_t ipa)
{
    // Emulated device pages, by guest-physical address.
    static const uint64_t device_pages[HV_MMIO_DEVICES] = {
        HV_GUEST_GICD, HV_GUEST_UART0, HV_GUEST_UART1, HV_GUEST_RTC
    };
    uint64_t page = ipa & ~PAGE_OFFSET_MASK;
    uint32_t device = HV_MMIO_DEVICES;

    for (uint32_t i = 0U; i < HV_MMIO_DEVICES; i++) {
        if (page == device_pages[i]) {
            device = i;
        }
    }
    // A PL011 that is passed through is mapped and never comes here.
    if (((device == HV_MMIO_UART0) && vm->arch.uart[0].passthrough)
        || ((device == HV_MMIO_UART1) && vm->arch.uart[1].passthrough)) {
        device = HV_MMIO_DEVICES;
    }
    return device;
}

// Emulate the access; returns whether the register is emulated.
static bool emulate(hv_vm_t *vm, uint32_t device, uint32_t offset,
                    uint32_t size, bool write, uint64_t *value)
{
    bool known;

    if (device == HV_MMIO_GICD) {
        known = vgicd_access(vm, offset, size, write, value);
    } else if ((size != 4U) && !((size == 1U) && (offset == 0U))) {
        // The PrimeCells take word accesses (and byte ones to the data).
        *value = 0U;
        known = false;
    } else if (device == HV_MMIO_RTC) {
        known = vrtc_access(offset, write, value);
    } else {
        known = vuart_access(&vm->arch.uart[device - HV_MMIO_UART0], offset,
                             write, value);
    }
    return known;
}

// Keep the low size bytes of value, sign-extended if sign, as a W or X value.
static uint64_t load_value(uint64_t value, uint32_t size, bool sign,
                           bool sixty_four)
{
    uint32_t bits = 8U * size;
    uint64_t result = value;

    if (bits < 64U) {
        uint32_t top = bits - 1U;
        uint64_t sign_bit = 1UL << top;
        uint64_t mask = (sign_bit << 1) - 1U;

        result &= mask;
        if (sign && ((result & sign_bit) != 0U)) {
            result |= ~mask;
        }
    }
    if (!sixty_four) {
        result &= 0xFFFFFFFFUL;
    }
    return result;
}

/*
 * A data abort of the guest (ESR_EL2 esr): emulate the access and go on
 * after the instruction, or stop the VM.
 */
hv_action_t mmio_abort(hv_vcpu_t *vcpu, uint64_t esr)
{
    hv_vm_t *vm = vcpu->vm;
    hv_guest_regs_t *regs = &vcpu->arch.regs;
    uint64_t hpfar = HV_READ_SYSREG(hpfar_el2);
    uint64_t far = HV_READ_SYSREG(far_el2);
    uint64_t ipa = ((hpfar >> HPFAR_FIPA_SHIFT) << 12)
                   | (far & PAGE_OFFSET_MASK);
    uint32_t device = device_of(vm, ipa);
    hv_action_t action = HV_CONTINUE;

    if ((device == HV_MMIO_DEVICES) || ((esr & ESR_S1PTW) != 0U)) {
        hv_printf("[hv] %s: access to guest-physical address 0x%lx "
                  "outside its RAM (ESR 0x%lx)\n", vm->config->name, ipa,
                  esr);
        hv_report_state(vcpu);
        action = HV_STOP;
    } else if ((esr & ESR_ISV) == 0U) {
        hv_printf("[hv] %s: access to %s at 0x%lx by an instruction "
                  "that can't be emulated (ESR 0x%lx)\n", vm->config->name,
                  device_names[device], ipa, esr);
        hv_report_state(vcpu);
        action = HV_STOP;
    } else {
        uint32_t sas = (uint32_t) ((esr >> ESR_SAS_SHIFT) & ESR_SAS_MASK);
        uint32_t size = (uint32_t) 1U << sas;
        uint32_t reg = (uint32_t) ((esr >> ESR_SRT_SHIFT) & ESR_SRT_MASK);
        bool write = (esr & ESR_WNR) != 0U;
        uint32_t offset = (uint32_t) (ipa & PAGE_OFFSET_MASK);
        uint64_t value = 0U;

        if (write && (reg != REG_XZR)) {
            value = load_value(regs->x[reg], size, false, true);
        }
        if (!emulate(vm, device, offset, size, write, &value)) {
            log_unhandled(vm, device, offset, write);
        }
        if (!write && (reg != REG_XZR)) {
            regs->x[reg] = load_value(value, size, (esr & ESR_SSE) != 0U,
                                      (esr & ESR_SF) != 0U);
        }
        regs->elr += 4U;        // Past the load or store
    }
    return action;
}
