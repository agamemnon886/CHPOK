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
 * Virtual machines on arm64: their serial ports and stage-2 translation,
 * and loading them as QEMU's ELF loader does: the image at its
 * guest-physical addresses (hvconfig.py made it flat and checked it), the
 * VM's device tree (made by hvconfig.py) at the start of its RAM.
 *
 * A guest PL011 whose host port is a PL011 too (COM1, COM2) is passed
 * through: stage 2 maps it to the host's, at any address. Otherwise it is
 * emulated (vuart.c) and forwarded to the host port, a PCI 16550.
 */

#include <arch/internal.h>

#define CTR_DMINLINE_SHIFT  16U
#define CTR_DMINLINE_MASK   0xFUL

/*
 * The QEMU option that makes a host port: COM1 and COM2 are the PL011s
 * (-serial), the others PCI 16550s (qemu.sh of hvconfig.py).
 */
static void print_port_hint(uint16_t port)
{
    if (port == HV_PORT_COM1) {
        hv_printf("[hv] (QEMU: a -serial option)\n");
    } else if (port == HV_PORT_COM2) {
        hv_printf("[hv] (QEMU: a second -serial option)\n");
    } else {
        hv_printf("[hv] (QEMU: -device pci-serial,addr=0x%x)\n",
                  0x10U + ((uint32_t) port - HV_PORT_COM3));
    }
}

// Give a guest UART its host port (or none); stops if the port is missing.
static void setup_uart(const hv_vm_t *vm, hv_vuart_t *uart, uint16_t port)
{
    if ((port != HV_PORT_NONE) && !hv_uart_present(port)) {
        hv_printf("[hv] %s: its serial port %s doesn't exist\n",
                  vm->config->name, hv_port_name(port));
        print_port_hint(port);
        hv_fatal("a serial port of the configuration is missing");
    }
    uart->host_port = port;
    uart->passthrough = hv_uart_is_pl011(port);
    vuart_reset(uart);
}

// Serial ports and stage 2 of a VM (its RAM already placed).
void hv_arch_vm_init(hv_vm_t *vm)
{
    setup_uart(vm, &vm->arch.uart[0], vm->config->console_port);
    setup_uart(vm, &vm->arch.uart[1], vm->config->gdb_port);
    vm->arch.vttbr = stage2_build(vm, gic_gicv_base());
}

/*
 * MISRA C:2025 Rule 11.4 deviation (the rest of this file): the guest RAM
 * is used by host-physical address (identity-mapped), converted to
 * pointers.
 */

// Clear size bytes (a multiple of 8) of guest RAM at a host-physical address.
static void zero_ram(uint64_t pa, uint64_t size)
{
    volatile uint64_t *p = (volatile uint64_t *) (uintptr_t) pa;

    for (uint64_t i = 0U; i < (size / 8U); i++) {
        p[i] = 0U;
    }
}

/*
 * Write the data cache back to memory for size bytes at host-physical pa,
 * and drop the instruction cache: the guest starts with its MMU (and its
 * caches) off.
 */
static void clean_caches(uint64_t pa, uint64_t size)
{
    uint64_t line = 4UL << ((HV_READ_SYSREG(ctr_el0) >> CTR_DMINLINE_SHIFT)
                            & CTR_DMINLINE_MASK);

    for (uint64_t a = pa & ~(line - 1U); a < (pa + size); a += line) {
        arm64_dc_cvac(a);
    }
    arm64_dsb_ish();
    arm64_ic_iallu();
    arm64_dsb_ish();
    arm64_isb();
}

/*
 * Load a VM (first start or reset): clear its RAM, copy its image and its
 * device tree, reset its devices. Runs on the VM's CPU.
 */
void vm_load(hv_vm_t *vm)
{
    const hv_vm_config_t *cfg = vm->config;
    uint64_t image_size = *cfg->image_size;
    uint64_t dtb_size = (cfg->boot_data_size != NULL) ? *cfg->boot_data_size
                                                      : 0U;
    uint64_t load = cfg->load_addr;
    uint64_t load_end = load + image_size;
    uint64_t ram_end = HV_GUEST_RAM + cfg->memory_size;

    // hvconfig.py checks this too; the RAM is never written outside.
    if ((cfg->boot_data == NULL) || (dtb_size > HV_GUEST_DTB_SPACE)
        || (load < (HV_GUEST_RAM + HV_GUEST_DTB_SPACE))
        || (load_end > cfg->bss_end) || (cfg->bss_end > ram_end)
        || (cfg->entry < load) || (cfg->entry >= load_end)) {
        vm_print(vm, "image doesn't fit into the VM's RAM");
        hv_fatal("bad VM image");
    }

    // Cleared RAM, then the device tree and the image (its BSS stays 0).
    zero_ram(vm->ram_base, cfg->memory_size);
    hv_memcpy((uint8_t *) (uintptr_t) vm->ram_base, cfg->boot_data,
              (size_t) dtb_size);
    hv_memcpy((uint8_t *) (uintptr_t) (vm->ram_base + (load - HV_GUEST_RAM)),
              cfg->image, (size_t) image_size);
    clean_caches(vm->ram_base, cfg->memory_size);

    // Devices as after power-on; unhandled registers are reported again.
    vgicd_reset(&vm->arch.gicd);
    vuart_reset(&vm->arch.uart[0]);
    vuart_reset(&vm->arch.uart[1]);
    vm->arch.timer_pending = false;
    vm->arch.timer_irqs = 0U;
    hv_memset(vm->arch.mmio_logged, 0U, sizeof(vm->arch.mmio_logged));
}
