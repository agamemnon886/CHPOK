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
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and declarations
 * are in arch/internal.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/arm64/load.c (the code of the processor architecture is in
 * hypervisor/csps); system registers are accessed with read_sysreg(),
 * write_sysreg() and isb(), shared with the kernel; memcpy() and memset() of
 * the library shared with the kernel replace hv_memcpy() and hv_memset().
 */

/**
 * @file
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

/**
 * @funcname `print_port_hint`
 *
 * @description
 * Prints the QEMU option that makes a missing host port: COM1 and COM2 are the
 * PL011s (-serial), the others PCI 16550s (qemu.sh of hvconfig.py).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `port` is equal to `HV_PORT_COM1`:
 *    1. Call `hv_printf()` with the argument
 *       `"[hv] (QEMU: a -serial option)\n"`.
 * 2. Otherwise, if `port` is equal to `HV_PORT_COM2`:
 *    1. Call `hv_printf()` with the argument
 *       `"[hv] (QEMU: a second -serial option)\n"`.
 * 3. Otherwise:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] (QEMU: -device pci-serial,addr=0x%%x)\n"` and
 *       `0x10U + ((uint32_t) port - HV_PORT_COM3)`.
 *
 * @calls
 * - `hv_printf()`
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

/**
 * @funcname `setup_uart`
 *
 * @description
 * Gives a guest UART its host port (or none); stops the hypervisor if the port
 * doesn't exist.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `const hv_vm_t *` | [in] The VM. |
 * | `uart` | `hv_vuart_t *` | [out] The guest UART. |
 * | `port` | `uint16_t` | [in] The host port number, or HV_PORT_NONE. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `port` is not equal to `HV_PORT_NONE` and `hv_uart_present(port)` is
 *    false:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: its serial port %%s doesn't exist\n"`, `vm->config->name`
 *       and `hv_port_name(port)`.
 *    2. Call `print_port_hint()` with the argument `port`.
 *    3. Call `hv_fatal()` with the argument
 *       `"a serial port of the configuration is missing"`.
 * 2. Set `uart->host_port` to `port`.
 * 3. Call `hv_uart_is_pl011()` with the argument `port`, and store the result
 *    in `uart->passthrough`.
 * 4. Call `vuart_reset()` with the argument `uart`.
 *
 * @calls
 * - `hv_uart_present()`
 * - `hv_printf()`
 * - `hv_port_name()`
 * - `print_port_hint()`
 * - `hv_fatal()`
 * - `hv_uart_is_pl011()`
 * - `vuart_reset()`
 */
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

/**
 * @funcname `hv_arch_vm_init`
 *
 * @description
 * Sets up the arm64 part of a VM, once its RAM is placed: its serial ports and
 * its stage-2 tables.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `setup_uart()` with the arguments `vm`, the address of
 *    `vm->arch.uart[0]` and `vm->config->console_port`.
 * 2. Call `setup_uart()` with the arguments `vm`, the address of
 *    `vm->arch.uart[1]` and `vm->config->gdb_port`.
 * 3. Call `stage2_build()` with the arguments `vm` and `gic_gicv_base()`, and
 *    store the result in `vm->arch.vttbr`.
 *
 * @calls
 * - `setup_uart()`
 * - `stage2_build()`
 * - `gic_gicv_base()`
 */
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

/**
 * @funcname `zero_ram`
 *
 * @description
 * Clears guest RAM at a host-physical address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The host-physical address. |
 * | `size` | `uint64_t` | [in] The size, a multiple of 8. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `volatile uint64_t *` | The RAM |
 * | `i` | `uint64_t` | The index of the 64-bit word |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `(volatile uint64_t *) (uintptr_t) pa`.
 * 2. Set `i` to `0U`; then while `i` is less than `(size / 8U)`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `p[i]` to `0U`.
 *
 * @calls
 * None.
 */
static void zero_ram(uint64_t pa, uint64_t size)
{
    volatile uint64_t *p = (volatile uint64_t *) (uintptr_t) pa;

    for (uint64_t i = 0U; i < (size / 8U); i++) {
        p[i] = 0U;
    }
}

/**
 * @funcname `clean_caches`
 *
 * @description
 * Writes the data cache back to memory over guest RAM and drops the instruction
 * cache, because the guest starts with its MMU (and its caches) off.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The host-physical address. |
 * | `size` | `uint64_t` | [in] The size. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `line` | `uint64_t` | The smallest data cache line size (CTR_EL0) |
 * | `a` | `uint64_t` | The address of a line |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `line` to `4UL << ((read_sysreg(ctr_el0) >> CTR_DMINLINE_SHIFT) &`
 *    `CTR_DMINLINE_MASK)`.
 * 2. Set `a` to `pa & ~(line - 1U)`; then while `a` is less than `(pa + size)`,
 *    repeat (after each pass: add `line` to `a`):
 *    1. Call `arm64_dc_cvac()` with the argument `a`.
 * 3. Call `arm64_dsb_ish()`.
 * 4. Call `arm64_ic_iallu()`.
 * 5. Call `arm64_dsb_ish()`.
 * 6. Call `isb()`.
 *
 * @calls
 * - `arm64_dc_cvac()`
 * - `arm64_dsb_ish()`
 * - `arm64_ic_iallu()`
 * - `isb()`
 */
static void clean_caches(uint64_t pa, uint64_t size)
{
    uint64_t line = 4UL << ((read_sysreg(ctr_el0) >> CTR_DMINLINE_SHIFT)
                            & CTR_DMINLINE_MASK);

    for (uint64_t a = pa & ~(line - 1U); a < (pa + size); a += line) {
        arm64_dc_cvac(a);
    }
    arm64_dsb_ish();
    arm64_ic_iallu();
    arm64_dsb_ish();
    isb();
}

/**
 * @funcname `vm_load`
 *
 * @description
 * Loads a VM, at its first start and at each reset: clears its RAM, copies its
 * device tree and image, resets its devices. Runs on the VM's CPU.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cfg` | `const hv_vm_config_t *` | The VM's configuration |
 * | `image_size` | `uint64_t` | The size of the image |
 * | `dtb_size` | `uint64_t` | The size of the device tree |
 * | `load` | `uint64_t` | The guest-physical load address |
 * | `load_end` | `uint64_t` | The end of the loaded bytes |
 * | `ram_end` | `uint64_t` | The guest-physical end of the RAM |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `cfg` to `vm->config`.
 * 2. Set `image_size` to the value at `cfg->image_size`.
 * 3. Set `dtb_size` to the value at `cfg->boot_data_size` if
 *    `cfg->boot_data_size` is not NULL, otherwise `0U`.
 * 4. Set `load` to `cfg->load_addr`.
 * 5. Set `load_end` to `load + image_size`.
 * 6. Set `ram_end` to `HV_GUEST_RAM + cfg->memory_size`.
 * 7. If `cfg->boot_data` is NULL or `dtb_size` is greater than
 *    `HV_GUEST_DTB_SPACE` or `load` is less than
 *    `(HV_GUEST_RAM + HV_GUEST_DTB_SPACE)` or `load_end` is greater than
 *    `cfg->bss_end` or `cfg->bss_end` is greater than `ram_end` or `cfg->entry`
 *    is less than `load` or `cfg->entry` is greater than or equal to
 *    `load_end`:
 *    1. Call `vm_print()` with the arguments `vm` and
 *       `"image doesn't fit into the VM's RAM"`.
 *    2. Call `hv_fatal()` with the argument `"bad VM image"`.
 * 8. Call `zero_ram()` with the arguments `vm->ram_base` and
 *    `cfg->memory_size`.
 * 9. Call `memcpy()` with the arguments
 *    `(uint8_t *) (uintptr_t) vm->ram_base`, `cfg->boot_data` and
 *    `(size_t) dtb_size`.
 * 10. Call `memcpy()` with the arguments
 *     `(uint8_t *) (uintptr_t) (vm->ram_base + (load - HV_GUEST_RAM))`,
 *     `cfg->image` and `(size_t) image_size`.
 * 11. Call `clean_caches()` with the arguments `vm->ram_base` and
 *     `cfg->memory_size`.
 * 12. Call `vgicd_reset()` with the address of `vm->arch.gicd`.
 * 13. Call `vuart_reset()` with the address of `vm->arch.uart[0]`.
 * 14. Call `vuart_reset()` with the address of `vm->arch.uart[1]`.
 * 15. Set `vm->arch.timer_pending` to `false`.
 * 16. Set `vm->arch.timer_irqs` to `0U`.
 * 17. Call `memset()` with the arguments `vm->arch.mmio_logged`, `0` and
 *     `sizeof(vm->arch.mmio_logged)`.
 *
 * @calls
 * - `vm_print()`
 * - `hv_fatal()`
 * - `zero_ram()`
 * - `memcpy()`
 * - `clean_caches()`
 * - `vgicd_reset()`
 * - `vuart_reset()`
 * - `memset()`
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
    (void) memcpy((uint8_t *) (uintptr_t) vm->ram_base, cfg->boot_data,
                  (size_t) dtb_size);
    (void) memcpy(
        (uint8_t *) (uintptr_t) (vm->ram_base + (load - HV_GUEST_RAM)),
        cfg->image, (size_t) image_size);
    clean_caches(vm->ram_base, cfg->memory_size);

    // Devices as after power-on; unhandled registers are reported again.
    vgicd_reset(&vm->arch.gicd);
    vuart_reset(&vm->arch.uart[0]);
    vuart_reset(&vm->arch.uart[1]);
    vm->arch.timer_pending = false;
    vm->arch.timer_irqs = 0U;
    (void) memset(vm->arch.mmio_logged, 0, sizeof(vm->arch.mmio_logged));
}
