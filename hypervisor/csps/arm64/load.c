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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
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
 * - \hlrid{HLR_LOAD_PRINT_PORT_HINT_01}: `print_port_hint()` shall print the
 *   option of QEMU that creates a missing serial port: a `-serial` option for
 *   COM1 and COM2, a PCI serial device in its slot for the others.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_004}
 *
 * @llr
 * - \llrid{LLR_LOAD_PRINT_PORT_HINT_01}: If `port` is `HV_PORT_COM1`,
 *   `print_port_hint()` shall call `hv_printf()` once with `"[hv] (QEMU: a
 *   -serial option)\n"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_PRINT_PORT_HINT_01}
 * - \llrid{LLR_LOAD_PRINT_PORT_HINT_02}: If `port` is `HV_PORT_COM2`,
 *   `print_port_hint()` shall call `hv_printf()` once with `"[hv] (QEMU: a
 *   second -serial option)\n"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_PRINT_PORT_HINT_01}
 * - \llrid{LLR_LOAD_PRINT_PORT_HINT_03}: If `port` is neither `HV_PORT_COM1`
 *   nor `HV_PORT_COM2`, `print_port_hint()` shall call `hv_printf()` once with
 *   `"[hv] (QEMU: -device pci-serial,addr=0x%%x)\n"` and `0x10` plus `port`
 *   minus `HV_PORT_COM3`.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_PRINT_PORT_HINT_01}
 *
 * @design
 * 1. If `port` is equal to `HV_PORT_COM1`: \llrref{LLR_LOAD_PRINT_PORT_HINT_01}
 *    1. Call `hv_printf()` with the argument
 *       `"[hv] (QEMU: a -serial option)\n"`.
 * 2. Otherwise, if `port` is equal to `HV_PORT_COM2`:
 *    \llrref{LLR_LOAD_PRINT_PORT_HINT_02}
 *    1. Call `hv_printf()` with the argument
 *       `"[hv] (QEMU: a second -serial option)\n"`.
 * 3. Otherwise: \llrref{LLR_LOAD_PRINT_PORT_HINT_03}
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
 * - \hlrid{HLR_LOAD_SETUP_UART_01}: `setup_uart()` shall give a UART of a guest
 *   the serial port of the host that the configuration names, used directly if
 *   it is a PL011, and reset it.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 * - \hlrid{HLR_LOAD_SETUP_UART_02}: `setup_uart()` shall stop the hypervisor,
 *   with a message and the QEMU option, if that port does not exist.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_004}
 *
 * @llr
 * - \llrid{LLR_LOAD_SETUP_UART_01}: If `port` is not `HV_PORT_NONE` and
 *   `hv_uart_present()` returns false for it, `setup_uart()` shall call, in
 *   this order, `hv_printf()` with `"[hv] %%s: its serial port %%s doesn't
 *   exist\n"`, the name of the VM and the result of `hv_port_name()` for
 *   `port`, `print_port_hint()` with `port`, and `hv_fatal()` with `"a serial
 *   port of the configuration is missing"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_SETUP_UART_02}
 * - \llrid{LLR_LOAD_SETUP_UART_02}: If `port` is `HV_PORT_NONE`, or
 *   `hv_uart_present()` returns true for it, `setup_uart()` shall set
 *   `host_port` of the UART to `port` and `passthrough` to the result of
 *   `hv_uart_is_pl011()` for `port`, and call `vuart_reset()` once with the
 *   UART.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_SETUP_UART_01}
 *
 * @design
 * 1. If `port` is not equal to `HV_PORT_NONE` and `hv_uart_present(port)` is
 *    false: \llrref{LLR_LOAD_SETUP_UART_01}
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: its serial port %%s doesn't exist\n"`, `vm->config->name`
 *       and `hv_port_name(port)`.
 *    2. Call `print_port_hint()` with the argument `port`.
 *    3. Call `hv_fatal()` with the argument
 *       `"a serial port of the configuration is missing"`.
 * 2. Set `uart->host_port` to `port`. \llrref{LLR_LOAD_SETUP_UART_02}
 * 3. Call `hv_uart_is_pl011()` with the argument `port`, and store the result
 *    in `uart->passthrough`. \llrref{LLR_LOAD_SETUP_UART_02}
 * 4. Call `vuart_reset()` with the argument `uart`.
 *    \llrref{LLR_LOAD_SETUP_UART_02}
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
 * - \hlrid{HLR_LOAD_HV_ARCH_VM_INIT_01}: `hv_arch_vm_init()` shall set up the
 *   arm64 part of a VM once its RAM is placed: its two serial ports and its
 *   stage 2 tables.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_MEM_001}
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 *
 * @llr
 * - \llrid{LLR_LOAD_HV_ARCH_VM_INIT_01}: `hv_arch_vm_init()` shall call
 *   `setup_uart()` with the VM, its UART 0 and the console port of its
 *   configuration, then with the VM, its UART 1 and the GDB port of its
 *   configuration, and then set `vttbr` of the VM to the result of
 *   `stage2_build()` for the VM and the result of `gic_gicv_base()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_HV_ARCH_VM_INIT_01}
 *
 * @design
 * 1. Call `setup_uart()` with the arguments `vm`, the address of
 *    `vm->arch.uart[0]` and `vm->config->console_port`.
 *    \llrref{LLR_LOAD_HV_ARCH_VM_INIT_01}
 * 2. Call `setup_uart()` with the arguments `vm`, the address of
 *    `vm->arch.uart[1]` and `vm->config->gdb_port`.
 *    \llrref{LLR_LOAD_HV_ARCH_VM_INIT_01}
 * 3. Call `stage2_build()` with the arguments `vm` and `gic_gicv_base()`, and
 *    store the result in `vm->arch.vttbr`. \llrref{LLR_LOAD_HV_ARCH_VM_INIT_01}
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
 * - \hlrid{HLR_LOAD_ZERO_RAM_01}: `zero_ram()` shall clear guest RAM at a host
 *   physical address.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_004}
 *
 * @llr
 * - \llrid{LLR_LOAD_ZERO_RAM_01}: `zero_ram()` shall write 0 to each of the
 *   `size` divided by 8 64-bit words from the address `pa`.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_ZERO_RAM_01}
 *
 * @design
 * 1. Set `p` to `(volatile uint64_t *) (uintptr_t) pa`.
 *    \llrref{LLR_LOAD_ZERO_RAM_01}
 * 2. Set `i` to `0U`; then while `i` is less than `(size / 8U)`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_LOAD_ZERO_RAM_01}
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
 * - \hlrid{HLR_LOAD_CLEAN_CACHES_01}: `clean_caches()` shall write the data
 *   cache back to memory over the RAM of a VM and drop the instruction cache.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_MEM_004}
 *
 * @llr
 * - \llrid{LLR_LOAD_CLEAN_CACHES_01}: For each address from `pa` rounded down
 *   to a multiple of the line size (4 shifted left by the field
 *   `CTR_DMINLINE_MASK` at `CTR_DMINLINE_SHIFT` of CTR_EL0) to below `pa` plus
 *   `size`, in steps of the line size, `clean_caches()` shall call
 *   `arm64_dc_cvac()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_CLEAN_CACHES_01}
 * - \llrid{LLR_LOAD_CLEAN_CACHES_02}: `clean_caches()` shall then call, in this
 *   order, `arm64_dsb_ish()`, `arm64_ic_iallu()`, `arm64_dsb_ish()` and
 *   `isb()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_CLEAN_CACHES_01}
 *
 * @design
 * 1. Set `line` to `4UL << ((read_sysreg(ctr_el0) >> CTR_DMINLINE_SHIFT) &`
 *    `CTR_DMINLINE_MASK)`. \llrref{LLR_LOAD_CLEAN_CACHES_01}
 * 2. Set `a` to `pa & ~(line - 1U)`; then while `a` is less than `(pa + size)`,
 *    repeat (after each pass: add `line` to `a`):
 *    \llrref{LLR_LOAD_CLEAN_CACHES_01}
 *    1. Call `arm64_dc_cvac()` with the argument `a`.
 * 3. Call `arm64_dsb_ish()`. \llrref{LLR_LOAD_CLEAN_CACHES_02}
 * 4. Call `arm64_ic_iallu()`. \llrref{LLR_LOAD_CLEAN_CACHES_02}
 * 5. Call `arm64_dsb_ish()`. \llrref{LLR_LOAD_CLEAN_CACHES_02}
 * 6. Call `isb()`. \llrref{LLR_LOAD_CLEAN_CACHES_02}
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
 * - \hlrid{HLR_LOAD_VM_LOAD_01}: `vm_load()` shall load a VM at its first start
 *   and at each reset: clear its RAM, copy its device tree to the start of the
 *   RAM and its image to its addresses, clean the caches, and reset its
 *   devices.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_004}
 *   - \srqref{SRQ_HV_VM_006}
 * - \hlrid{HLR_LOAD_VM_LOAD_02}: `vm_load()` shall stop, with a message, if the
 *   device tree or the image does not fit into the RAM of the VM or the entry
 *   is outside the image.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_003}
 *
 * @llr
 * - \llrid{LLR_LOAD_VM_LOAD_01}: If `boot_data` of the configuration is NULL,
 *   or the size of the device tree (0 if `boot_data_size` is NULL) is more than
 *   `HV_GUEST_DTB_SPACE`, or the load address is less than `HV_GUEST_RAM` plus
 *   `HV_GUEST_DTB_SPACE`, or the load address plus the size of the image is
 *   more than `bss_end`, or `bss_end` is more than `HV_GUEST_RAM` plus
 *   `memory_size`, or `entry` is less than the load address, or `entry` is not
 *   less than the load address plus the size of the image, `vm_load()` shall
 *   call `vm_print()` with the VM and `"image doesn't fit into the VM's RAM"`,
 *   and then `hv_fatal()` with `"bad VM image"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_VM_LOAD_02}
 * - \llrid{LLR_LOAD_VM_LOAD_02}: If `boot_data` of the configuration is not
 *   NULL, the size of the device tree (0 if `boot_data_size` is NULL) is not
 *   more than `HV_GUEST_DTB_SPACE`, the load address is not less than
 *   `HV_GUEST_RAM` plus `HV_GUEST_DTB_SPACE`, the load address plus the size of
 *   the image is not more than `bss_end`, `bss_end` is not more than
 *   `HV_GUEST_RAM` plus `memory_size`, and `entry` is from the load address to
 *   below the load address plus the size of the image, `vm_load()` shall, in
 *   this order, call `zero_ram()` with the RAM base of the VM and
 *   `memory_size`, copy with `memcpy()` the device tree to the RAM base and the
 *   image to the RAM base plus the load address minus `HV_GUEST_RAM`, call
 *   `clean_caches()` with the RAM base and `memory_size`, call `vgicd_reset()`
 *   for the distributor of the VM and `vuart_reset()` for its UARTs 0 and 1,
 *   set `timer_pending` to false and `timer_irqs` to 0, and set every byte of
 *   `mmio_logged` of the VM to 0 with `memset()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_LOAD_VM_LOAD_01}
 *
 * @design
 * 1. Set `cfg` to `vm->config`. \llrref{LLR_LOAD_VM_LOAD_01},
 *    \llrref{LLR_LOAD_VM_LOAD_02}
 * 2. Set `image_size` to the value at `cfg->image_size`.
 *    \llrref{LLR_LOAD_VM_LOAD_01}, \llrref{LLR_LOAD_VM_LOAD_02}
 * 3. Set `dtb_size` to the value at `cfg->boot_data_size` if
 *    `cfg->boot_data_size` is not NULL, otherwise `0U`.
 *    \llrref{LLR_LOAD_VM_LOAD_01}, \llrref{LLR_LOAD_VM_LOAD_02}
 * 4. Set `load` to `cfg->load_addr`. \llrref{LLR_LOAD_VM_LOAD_01},
 *    \llrref{LLR_LOAD_VM_LOAD_02}
 * 5. Set `load_end` to `load + image_size`. \llrref{LLR_LOAD_VM_LOAD_01},
 *    \llrref{LLR_LOAD_VM_LOAD_02}
 * 6. Set `ram_end` to `HV_GUEST_RAM + cfg->memory_size`.
 *    \llrref{LLR_LOAD_VM_LOAD_01}, \llrref{LLR_LOAD_VM_LOAD_02}
 * 7. If `cfg->boot_data` is NULL or `dtb_size` is greater than
 *    `HV_GUEST_DTB_SPACE` or `load` is less than
 *    `(HV_GUEST_RAM + HV_GUEST_DTB_SPACE)` or `load_end` is greater than
 *    `cfg->bss_end` or `cfg->bss_end` is greater than `ram_end` or `cfg->entry`
 *    is less than `load` or `cfg->entry` is greater than or equal to
 *    `load_end`: \llrref{LLR_LOAD_VM_LOAD_01}
 *    1. Call `vm_print()` with the arguments `vm` and
 *       `"image doesn't fit into the VM's RAM"`.
 *    2. Call `hv_fatal()` with the argument `"bad VM image"`.
 * 8. Call `zero_ram()` with the arguments `vm->ram_base` and
 *    `cfg->memory_size`. \llrref{LLR_LOAD_VM_LOAD_02}
 * 9. Call `memcpy()` with the arguments
 *    `(uint8_t *) (uintptr_t) vm->ram_base`, `cfg->boot_data` and
 *    `(size_t) dtb_size`. \llrref{LLR_LOAD_VM_LOAD_02}
 * 10. Call `memcpy()` with the arguments
 *     `(uint8_t *) (uintptr_t) (vm->ram_base + (load - HV_GUEST_RAM))`,
 *     `cfg->image` and `(size_t) image_size`. \llrref{LLR_LOAD_VM_LOAD_02}
 * 11. Call `clean_caches()` with the arguments `vm->ram_base` and
 *     `cfg->memory_size`. \llrref{LLR_LOAD_VM_LOAD_02}
 * 12. Call `vgicd_reset()` with the address of `vm->arch.gicd`.
 *     \llrref{LLR_LOAD_VM_LOAD_02}
 * 13. Call `vuart_reset()` with the address of `vm->arch.uart[0]`.
 *     \llrref{LLR_LOAD_VM_LOAD_02}
 * 14. Call `vuart_reset()` with the address of `vm->arch.uart[1]`.
 *     \llrref{LLR_LOAD_VM_LOAD_02}
 * 15. Set `vm->arch.timer_pending` to `false`. \llrref{LLR_LOAD_VM_LOAD_02}
 * 16. Set `vm->arch.timer_irqs` to `0U`. \llrref{LLR_LOAD_VM_LOAD_02}
 * 17. Call `memset()` with the arguments `vm->arch.mmio_logged`, `0` and
 *     `sizeof(vm->arch.mmio_logged)`. \llrref{LLR_LOAD_VM_LOAD_02}
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
