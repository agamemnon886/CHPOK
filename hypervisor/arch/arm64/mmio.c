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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file and
 * its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * hv_action_e (was hv_action_t); the macros, types and declarations are in
 * arch/internal.h, documented there.
 */

/**
 * @file
 * Data aborts of a guest at stage 2: an access to a guest-physical address
 * that stage 2 doesn't map (stage2.c). Accesses to the emulated device pages
 * (the distributor, the PL011s that aren't passed through, the RTC) are
 * emulated from the syndrome: QEMU gives the register, the size and the
 * direction of single loads and stores (ESR_EL2.ISV). The first access to
 * each register that isn't emulated is reported. Any other access, or one
 * without a syndrome, stops the VM.
 */

#include <arch/internal.h>

static const char *const device_names[HV_MMIO_DEVICES] = {
    "GIC distributor", "UART0", "UART1", "RTC"
};

/**
 * @funcname `log_unhandled`
 *
 * @description
 * Reports the first access of a VM to a register of an emulated device page
 * that isn't emulated (later ones are silent).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in] The VM. |
 * | `device` | `uint32_t` | [in] The emulated device. |
 * | `offset` | `uint32_t` | [in] The register offset in the page. |
 * | `write` | `bool` | [in] true for a write. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `device_names` | `const char *const[HV_MMIO_DEVICES]` | The names of the emulated devices |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `word` | `uint32_t` | The index of the register's word |
 * | `bit` | `uint8_t` | The word's bit in its byte of mmio_logged |
 * | `logged` | `uint8_t *` | The byte of mmio_logged |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `word` to `offset / 4U`.
 * 2. Set `bit` to `(uint8_t) (1U << (word % 8U))`.
 * 3. Set `logged` to the address of `vm->arch.mmio_logged[device][word / 8U]`.
 * 4. If `(*logged & bit)` is equal to `0U`:
 *    1. Set the bits `bit` in `*logged`.
 *    2. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: %%s of %%s register 0x%%x, which isn't emulated "`
 *       `"(reported once)\n"`, `vm->config->name`, `"write"` if `write` is
 *       true, otherwise `"read"`, `device_names[device]` and `offset`.
 *
 * @calls
 * - `hv_printf()`
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

/**
 * @funcname `device_of`
 *
 * @description
 * Returns the emulated device page of a guest-physical address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `const hv_vm_t *` | [in] The VM. |
 * | `ipa` | `uint64_t` | [in] The guest-physical address. |
 *
 * @return The device (HV_MMIO_GICD, _UART0, _UART1, _RTC), or HV_MMIO_DEVICES
 *         if the address isn't in an emulated page.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `device_pages` | `static const uint64_t[HV_MMIO_DEVICES]` | The guest-physical page of each device |
 * | `page` | `uint64_t` | The page of the address |
 * | `device` | `uint32_t` | The device found |
 * | `i` | `uint32_t` | The index of a device |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `page` to `ipa & ~PAGE_OFFSET_MASK`.
 * 2. Set `device` to `HV_MMIO_DEVICES`.
 * 3. Set `i` to `0U`; then while `i` is less than `HV_MMIO_DEVICES`, repeat
 *    (after each pass: increment `i`):
 *    1. If `page` is equal to `device_pages[i]`:
 *       1. Set `device` to `i`.
 * 4. If (`device` is equal to `HV_MMIO_UART0` and
 *    `vm->arch.uart[0].passthrough` is true) or (`device` is equal to
 *    `HV_MMIO_UART1` and `vm->arch.uart[1].passthrough` is true):
 *    1. Set `device` to `HV_MMIO_DEVICES`.
 * 5. Return `device`.
 *
 * @calls
 * None.
 */
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

/**
 * @funcname `emulate`
 *
 * @description
 * Emulates an access to an emulated device page.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 * | `device` | `uint32_t` | [in] The emulated device. |
 * | `offset` | `uint32_t` | [in] The register offset. |
 * | `size` | `uint32_t` | [in] The access size in bytes. |
 * | `write` | `bool` | [in] true for a write. |
 * | `value` | `uint64_t *` | [out] The value written, or where the value read is stored. |
 *
 * @return Whether the register is emulated.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `known` | `bool` | Whether the register is emulated |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `device` is equal to `HV_MMIO_GICD`:
 *    1. Call `vgicd_access()` with the arguments `vm`, `offset`, `size`,
 *       `write` and `value`, and store the result in `known`.
 * 2. Otherwise, if `size` is not equal to `4U` and not ((`size` is equal to
 *    `1U` and `offset` is equal to `0U`)):
 *    1. Set `*value` to `0U`.
 *    2. Set `known` to `false`.
 * 3. Otherwise, if `device` is equal to `HV_MMIO_RTC`:
 *    1. Call `vrtc_access()` with the arguments `offset`, `write` and `value`,
 *       and store the result in `known`.
 * 4. Otherwise:
 *    1. Call `vuart_access()` with the arguments the address of
 *       `vm->arch.uart[device - HV_MMIO_UART0]`, `offset`, `write` and `value`,
 *       and store the result in `known`.
 * 5. Return `known`.
 *
 * @calls
 * - `vgicd_access()`
 * - `vrtc_access()`
 * - `vuart_access()`
 */
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

/**
 * @funcname `load_value`
 *
 * @description
 * Keeps the low bytes of a value, sign-extended or not, as a W or X register
 * value (the result of a load, or the data of a store).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | [in] The value. |
 * | `size` | `uint32_t` | [in] The access size in bytes. |
 * | `sign` | `bool` | [in] true to sign-extend. |
 * | `sixty_four` | `bool` | [in] true for an X register, false for a W one. |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bits` | `uint32_t` | The access size in bits |
 * | `result` | `uint64_t` | The value |
 * | `top` | `uint32_t` | The top bit's number |
 * | `sign_bit` | `uint64_t` | The top bit |
 * | `mask` | `uint64_t` | The bits of the access |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `bits` to `8U * size`.
 * 2. Set `result` to `value`.
 * 3. If `bits` is less than `64U`:
 *    1. Set `top` to `bits - 1U`.
 *    2. Set `sign_bit` to `1UL << top`.
 *    3. Set `mask` to `(sign_bit << 1) - 1U`.
 *    4. Keep only the bits `mask` of `result`.
 *    5. If `sign` is true and `(result & sign_bit)` is not equal to `0U`:
 *       1. Set the bits `~mask` in `result`.
 * 4. If `sixty_four` is false:
 *    1. Keep only the bits `0xFFFFFFFFUL` of `result`.
 * 5. Return `result`.
 *
 * @calls
 * None.
 */
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

/**
 * @funcname `mmio_abort`
 *
 * @description
 * Handles a data abort of the guest at stage 2: emulates the access to an
 * emulated device page and goes on after the instruction, or stops the VM.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in,out] The vCPU. |
 * | `esr` | `uint64_t` | [in] ESR_EL2 of the abort. |
 *
 * @return HV_CONTINUE, or HV_STOP.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `device_names` | `const char *const[HV_MMIO_DEVICES]` | The names of the emulated devices |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | The vCPU's VM |
 * | `regs` | `hv_guest_regs_t *` | The guest's registers |
 * | `hpfar` | `uint64_t` | HPFAR_EL2: the faulting page |
 * | `far` | `uint64_t` | FAR_EL2: the faulting address |
 * | `ipa` | `uint64_t` | The guest-physical address |
 * | `device` | `uint32_t` | The emulated device |
 * | `action` | `hv_action_e` | What the run loop does next |
 * | `sas` | `uint32_t` | The access size field (log2) |
 * | `size` | `uint32_t` | The access size in bytes |
 * | `reg` | `uint32_t` | The register of the access (31: XZR) |
 * | `write` | `bool` | Whether it is a write |
 * | `offset` | `uint32_t` | The offset in the page |
 * | `value` | `uint64_t` | The value written or read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `vm` to `vcpu->vm`.
 * 2. Set `regs` to the address of `vcpu->arch.regs`.
 * 3. Set `hpfar` to `HV_READ_SYSREG(hpfar_el2)`.
 * 4. Set `far` to `HV_READ_SYSREG(far_el2)`.
 * 5. Set `ipa` to
 *    `((hpfar >> HPFAR_FIPA_SHIFT) << 12) | (far & PAGE_OFFSET_MASK)`.
 * 6. Call `device_of()` with the arguments `vm` and `ipa`, and store the result
 *    in `device`.
 * 7. Set `action` to `HV_CONTINUE`.
 * 8. If `device` is equal to `HV_MMIO_DEVICES` or `(esr & ESR_S1PTW)` is not
 *    equal to `0U`:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: access to guest-physical address 0x%%lx "`
 *       `"outside its RAM (ESR 0x%%lx)\n"`, `vm->config->name`, `ipa` and
 *       `esr`.
 *    2. Call `hv_report_state()` with the argument `vcpu`.
 *    3. Set `action` to `HV_STOP`.
 * 9. Otherwise, if `(esr & ESR_ISV)` is equal to `0U`:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: access to %%s at 0x%%lx by an instruction "`
 *       `"that can't be emulated (ESR 0x%%lx)\n"`, `vm->config->name`,
 *       `device_names[device]`, `ipa` and `esr`.
 *    2. Call `hv_report_state()` with the argument `vcpu`.
 *    3. Set `action` to `HV_STOP`.
 * 10. Otherwise:
 *     1. Set `sas` to `(uint32_t) ((esr >> ESR_SAS_SHIFT) & ESR_SAS_MASK)`.
 *     2. Set `size` to `(uint32_t) 1U << sas`.
 *     3. Set `reg` to `(uint32_t) ((esr >> ESR_SRT_SHIFT) & ESR_SRT_MASK)`.
 *     4. Set `write` to `(esr & ESR_WNR) != 0U`.
 *     5. Set `offset` to `(uint32_t) (ipa & PAGE_OFFSET_MASK)`.
 *     6. Set `value` to `0U`.
 *     7. If `write` is true and `reg` is not equal to `REG_XZR`:
 *        1. Call `load_value()` with the arguments `regs->x[reg]`, `size`,
 *           `false` and `true`, and store the result in `value`.
 *     8. If `emulate(vm, device, offset, size, write, &value)` is false:
 *        1. Call `log_unhandled()` with the arguments `vm`, `device`, `offset`
 *           and `write`.
 *     9. If `write` is false and `reg` is not equal to `REG_XZR`:
 *        1. Call `load_value()` with the arguments `value`, `size`,
 *           `(esr & ESR_SSE) != 0U` and `(esr & ESR_SF) != 0U`, and store the
 *           result in `regs->x[reg]`.
 *     10. Add `4U` to `regs->elr`.
 * 11. Return `action`.
 *
 * @calls
 * - `device_of()`
 * - `hv_printf()`
 * - `hv_report_state()`
 * - `load_value()`
 * - `emulate()`
 * - `log_unhandled()`
 */
hv_action_e mmio_abort(hv_vcpu_t *vcpu, uint64_t esr)
{
    hv_vm_t *vm = vcpu->vm;
    hv_guest_regs_t *regs = &vcpu->arch.regs;
    uint64_t hpfar = HV_READ_SYSREG(hpfar_el2);
    uint64_t far = HV_READ_SYSREG(far_el2);
    uint64_t ipa = ((hpfar >> HPFAR_FIPA_SHIFT) << 12)
                   | (far & PAGE_OFFSET_MASK);
    uint32_t device = device_of(vm, ipa);
    hv_action_e action = HV_CONTINUE;

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
