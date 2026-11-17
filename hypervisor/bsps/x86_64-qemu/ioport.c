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
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from hypervisor/src/ to
 * arch/x86_64/; the VM's devices and I/O permission map are in the arch
 * part of its structure. Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * hv_action_e (was hv_action_t); the macros, types and external declarations
 * are in arch/internal.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/x86_64/ioport.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps); memset() of the library
 * shared with the kernel replaces hv_memset().
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: the low level requirements
 * name the register and range helpers that the functions call; the high
 * level requirements trace to the system requirements split into
 * requirements of one "shall" each.
 */

/**
 * @file
 * I/O ports of the guests. Every port access exits, except to a guest
 * serial port forwarded to the host port at the same address, which the
 * guest uses directly. The guest sees the virtual 8259A PIC pair
 * (0x20-0x21, 0xA0-0xA1, vpic.c), the virtual 8254 PIT and port B
 * (0x40-0x43, 0x61, vpit.c), COM1 (0x3F8, forwarded to the VM's console
 * port), COM2 (0x2F8, forwarded to the VM's GDB port, if any), the keyboard
 * controller (0x60, 0x64: writing 0xFE resets) and the reset control
 * (0xCF9: bit 2 resets). Other ports read as all ones and ignore writes
 * (the first access to each one is reported), except the POST/delay ports
 * and the PCI configuration ports (no PCI devices), which are silent.
 */

#include <arch/internal.h>

/**
 * @funcname `iopm_allow`
 *
 * @description
 * Lets the guest access a range of ports directly: clears their bits in the
 * I/O permission map.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `iopm` | `uint8_t *` | [in,out] The I/O permission map. |
 * | `first` | `uint16_t` | [in] The first port. |
 * | `count` | `uint32_t` | [in] The number of ports. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint32_t` | The port whose bit is cleared |
 *
 * @hlr
 * - \hlrid{HLR_IOPORT_IOPM_ALLOW_01}: `iopm_allow()` shall let a guest access a
 *   range of ports directly.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 *
 * @llr
 * - \llrid{LLR_IOPORT_IOPM_ALLOW_01}: For each of the `count` ports from
 *   `first`, `iopm_allow()` shall clear bit `port % 8` of the byte `port / 8`
 *   of `iopm`, and change no other bit.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IOPM_ALLOW_01}
 *
 * @design
 * 1. Set `port` to `first`; then while `port` is less than
 *    `((uint32_t) first + count)`, repeat (after each pass: increment `port`):
 *    \llrref{LLR_IOPORT_IOPM_ALLOW_01}
 *    1. Keep only the bits `(uint8_t) ~(1U << (port % 8U))` of
 *       `iopm[port / 8U]`.
 *
 * @calls
 * None.
 */
static void iopm_allow(uint8_t *iopm, uint16_t first, uint32_t count)
{
    for (uint32_t port = first; port < ((uint32_t) first + count); port++) {
        iopm[port / 8U] &= (uint8_t) ~(1U << (port % 8U));
    }
}

/**
 * @funcname `io_init_iopm`
 *
 * @description
 * Builds the I/O permission map of a VM: every port access exits, except to
 * a serial port forwarded to the host port at the same address (COM1 as the
 * console, COM2 as the GDB port).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in] The VM. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_IOPORT_IO_INIT_IOPM_01}: `io_init_iopm()` shall make every port
 *   access of a guest exit to the hypervisor, except to its console and GDB
 *   ports when they are the same ports of the host (COM1 and COM2).
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_001}
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 *
 * @llr
 * - \llrid{LLR_IOPORT_IO_INIT_IOPM_01}: `io_init_iopm()` shall set the
 *   `SVM_IOPM_SIZE` bytes of `vm->arch.iopm` to `0xFF` with `memset()`, before
 *   it allows a port.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_INIT_IOPM_01}
 * - \llrid{LLR_IOPORT_IO_INIT_IOPM_02}: If and only if the console port of the
 *   VM's configuration is `HV_PORT_COM1`, `io_init_iopm()` shall call
 *   `iopm_allow()` once with `vm->arch.iopm`, `HV_PORT_COM1` and `UART_PORTS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_INIT_IOPM_01}
 * - \llrid{LLR_IOPORT_IO_INIT_IOPM_03}: If and only if the GDB port of the VM's
 *   configuration is `HV_PORT_COM2`, `io_init_iopm()` shall call `iopm_allow()`
 *   once with `vm->arch.iopm`, `HV_PORT_COM2` and `UART_PORTS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_INIT_IOPM_01}
 *
 * @design
 * 1. Call `memset()` with the arguments `vm->arch.iopm`, `0xFF` and
 *    `SVM_IOPM_SIZE`. \llrref{LLR_IOPORT_IO_INIT_IOPM_01}
 * 2. If `vm->config->console_port` is equal to `HV_PORT_COM1`:
 *    \llrref{LLR_IOPORT_IO_INIT_IOPM_02}
 *    1. Call `iopm_allow()` with the arguments `vm->arch.iopm`, `HV_PORT_COM1`
 *       and `UART_PORTS`.
 * 3. If `vm->config->gdb_port` is equal to `HV_PORT_COM2`:
 *    \llrref{LLR_IOPORT_IO_INIT_IOPM_03}
 *    1. Call `iopm_allow()` with the arguments `vm->arch.iopm`, `HV_PORT_COM2`
 *       and `UART_PORTS`.
 *
 * @calls
 * - `memset()`
 * - `iopm_allow()`
 */
void io_init_iopm(hv_vm_t *vm)
{
    // A set bit makes the access to that port exit.
    (void) memset(vm->arch.iopm, 0xFF, SVM_IOPM_SIZE);
    if (vm->config->console_port == HV_PORT_COM1) {
        iopm_allow(vm->arch.iopm, HV_PORT_COM1, UART_PORTS);
    }
    if (vm->config->gdb_port == HV_PORT_COM2) {
        iopm_allow(vm->arch.iopm, HV_PORT_COM2, UART_PORTS);
    }
}

/**
 * @funcname `in_range`
 *
 * @description
 * Tells whether a port is in a range of ports.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port. |
 * | `first` | `uint16_t` | [in] The first port of the range. |
 * | `count` | `uint32_t` | [in] The number of ports of the range. |
 *
 * @return true if port is one of the count ports from first.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_IOPORT_IN_RANGE_01}: `in_range()` shall tell whether a port is
 *   in a range of ports.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_001}
 *
 * @llr
 * - \llrid{LLR_IOPORT_IN_RANGE_01}: If `port` is not less than `first` and is
 *   less than `first` plus `count`, `in_range()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IN_RANGE_01}
 * - \llrid{LLR_IOPORT_IN_RANGE_02}: If `port` is less than `first`, or is not
 *   less than `first` plus `count`, `in_range()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IN_RANGE_01}
 *
 * @design
 * 1. Return
 *    `(port >= first) && ((uint32_t) port < ((uint32_t) first + count))`.
 *    \llrref{LLR_IOPORT_IN_RANGE_01}, \llrref{LLR_IOPORT_IN_RANGE_02}
 *
 * @calls
 * None.
 */
static bool in_range(uint16_t port, uint16_t first, uint32_t count)
{
    return (port >= first) && ((uint32_t) port < ((uint32_t) first + count));
}

/**
 * @funcname `all_ones`
 *
 * @description
 * Returns the value read from an absent port: all ones, of the access size.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `uint32_t` | [in] The access size: 1, 2 or 4 bytes. |
 *
 * @return 0xFF, 0xFFFF or 0xFFFFFFFF.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint32_t` | The value of all ones |
 *
 * @hlr
 * - \hlrid{HLR_IOPORT_ALL_ONES_01}: `all_ones()` shall give the value of a read
 *   from an absent port: all ones of the size.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_006}
 *
 * @llr
 * - \llrid{LLR_IOPORT_ALL_ONES_01}: If `size` is 1, `all_ones()` shall return
 *   `0xFF`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_ALL_ONES_01}
 * - \llrid{LLR_IOPORT_ALL_ONES_02}: If `size` is 2, `all_ones()` shall return
 *   `0xFFFF`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_ALL_ONES_01}
 * - \llrid{LLR_IOPORT_ALL_ONES_03}: If `size` is neither 1 nor 2, `all_ones()`
 *   shall return `0xFFFFFFFF`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_ALL_ONES_01}
 *
 * @design
 * 1. If `size` is equal to `1U`: \llrref{LLR_IOPORT_ALL_ONES_01}
 *    1. Set `value` to `0xFFU`.
 * 2. Otherwise, if `size` is equal to `2U`: \llrref{LLR_IOPORT_ALL_ONES_02}
 *    1. Set `value` to `0xFFFFU`.
 * 3. Otherwise: \llrref{LLR_IOPORT_ALL_ONES_03}
 *    1. Set `value` to `0xFFFFFFFFU`.
 * 4. Return `value`. \llrref{LLR_IOPORT_ALL_ONES_01},
 *    \llrref{LLR_IOPORT_ALL_ONES_02}, \llrref{LLR_IOPORT_ALL_ONES_03}
 *
 * @calls
 * None.
 */
static uint32_t all_ones(uint32_t size)
{
    uint32_t value;

    if (size == 1U) {
        value = 0xFFU;
    } else if (size == 2U) {
        value = 0xFFFFU;
    } else {
        value = 0xFFFFFFFFU;
    }
    return value;
}

/**
 * @funcname `serial_access`
 *
 * @description
 * Performs a guest access to a serial port that is forwarded to a host
 * port, or absent if the VM has no host port for it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `host_base` | `uint16_t` | [in] The host serial port, or HV_PORT_NONE. |
 * | `offset` | `uint16_t` | [in] The register offset in the serial port, 0-7. |
 * | `size` | `uint32_t` | [in] The access size in bytes. |
 * | `is_in` | `bool` | [in] true for a read. |
 * | `value` | `uint32_t *` | [in,out] The value written, or where the value read is stored. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_IOPORT_SERIAL_ACCESS_01}: `serial_access()` shall repeat a guest
 *   access to a serial port on the host port that the configuration gives it,
 *   and make it absent if there is none.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 *
 * @llr
 * - \llrid{LLR_IOPORT_SERIAL_ACCESS_01}: If `host_base` is not `HV_PORT_NONE`,
 *   `serial_access()` shall call `vuart_access()` once with `host_base`,
 *   `offset`, `size`, `is_in` and `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_SERIAL_ACCESS_01}
 * - \llrid{LLR_IOPORT_SERIAL_ACCESS_02}: If `host_base` is `HV_PORT_NONE` and
 *   `is_in` is true, `serial_access()` shall store the result of `all_ones()`
 *   for `size` at `value`, without calling `vuart_access()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_SERIAL_ACCESS_01}
 * - \llrid{LLR_IOPORT_SERIAL_ACCESS_03}: If `host_base` is `HV_PORT_NONE` and
 *   `is_in` is false, `serial_access()` shall leave the value at `value` as it
 *   is, without calling `vuart_access()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_SERIAL_ACCESS_01}
 *
 * @design
 * 1. If `host_base` is not equal to `HV_PORT_NONE`:
 *    \llrref{LLR_IOPORT_SERIAL_ACCESS_01}
 *    1. Call `vuart_access()` with the arguments `host_base`, `offset`, `size`,
 *       `is_in` and `value`.
 * 2. Otherwise, if `is_in` is true: \llrref{LLR_IOPORT_SERIAL_ACCESS_02},
 *    \llrref{LLR_IOPORT_SERIAL_ACCESS_03}
 *    1. Call `all_ones()` with the argument `size`, and store the result in
 *       `*value`.
 *
 * @calls
 * - `vuart_access()`
 * - `all_ones()`
 */
static void serial_access(uint16_t host_base, uint16_t offset, uint32_t size,
                          bool is_in, uint32_t *value)
{
    if (host_base != HV_PORT_NONE) {
        vuart_access(host_base, offset, size, is_in, value);
    } else if (is_in) {
        *value = all_ones(size);
    } else {
        // Writes to an absent port are dropped.
    }
}

/**
 * @funcname `report_port`
 *
 * @description
 * Reports the first access of a VM to an unhandled port (later accesses to
 * the same port are silent).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 * | `port` | `uint16_t` | [in] The port. |
 * | `is_in` | `bool` | [in] true for a read. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `byte` | `uint32_t` | The byte of the port's bit in io_logged |
 * | `bit` | `uint8_t` | The port's bit in that byte |
 *
 * @hlr
 * - \hlrid{HLR_IOPORT_REPORT_PORT_01}: `report_port()` shall report the first
 *   access of a VM to a port that it does not have.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_006}
 *   - \srqref{SRQ_HV_CON_003}
 *
 * @llr
 * - \llrid{LLR_IOPORT_REPORT_PORT_01}: If bit `port % 8` of the byte `port / 8`
 *   of `vm->arch.io_logged` is 0 and `is_in` is true, `report_port()` shall set
 *   that bit and call `hv_printf()` once with `"[hv] %%s: %%s of I/O port 0x%%x
 *   ignored\n"`, the name of the VM, `"read"` and `port`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_REPORT_PORT_01}
 * - \llrid{LLR_IOPORT_REPORT_PORT_02}: If that bit is 0 and `is_in` is false,
 *   `report_port()` shall set the bit and call `hv_printf()` once with `"[hv]
 *   %%s: %%s of I/O port 0x%%x ignored\n"`, the name of the VM, `"write"` and
 *   `port`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_REPORT_PORT_01}
 * - \llrid{LLR_IOPORT_REPORT_PORT_03}: If bit `port % 8` of the byte `port / 8`
 *   of `vm->arch.io_logged` is 1, `report_port()` shall leave
 *   `vm->arch.io_logged` as it is and not call `hv_printf()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_REPORT_PORT_01}
 *
 * @design
 * 1. Set `byte` to `(uint32_t) port / 8U`. \llrref{LLR_IOPORT_REPORT_PORT_01},
 *    \llrref{LLR_IOPORT_REPORT_PORT_02}, \llrref{LLR_IOPORT_REPORT_PORT_03}
 * 2. Set `bit` to `(uint8_t) (1U << ((uint32_t) port % 8U))`.
 *    \llrref{LLR_IOPORT_REPORT_PORT_01}, \llrref{LLR_IOPORT_REPORT_PORT_02},
 *    \llrref{LLR_IOPORT_REPORT_PORT_03}
 * 3. If `(vm->arch.io_logged[byte] & bit)` is equal to `0U`:
 *    \llrref{LLR_IOPORT_REPORT_PORT_01}, \llrref{LLR_IOPORT_REPORT_PORT_02},
 *    \llrref{LLR_IOPORT_REPORT_PORT_03}
 *    1. Set the bits `bit` in `vm->arch.io_logged[byte]`.
 *    2. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: %%s of I/O port 0x%%x ignored\n"`, `vm->config->name`,
 *       `"read"` if `is_in` is true, otherwise `"write"` and `(uint32_t) port`.
 *
 * @calls
 * - `hv_printf()`
 */
static void report_port(hv_vm_t *vm, uint16_t port, bool is_in)
{
    // One bit per port in io_logged.
    uint32_t byte = (uint32_t) port / 8U;
    uint8_t bit = (uint8_t) (1U << ((uint32_t) port % 8U));

    if ((vm->arch.io_logged[byte] & bit) == 0U) {
        vm->arch.io_logged[byte] |= bit;
        hv_printf("[hv] %s: %s of I/O port 0x%x ignored\n",
                  vm->config->name, is_in ? "read" : "write",
                  (uint32_t) port);
    }
}

/**
 * @funcname `other_port`
 *
 * @description
 * Handles an access of a VM to a port that is not an emulated device: the
 * keyboard controller and the reset control (which reset the VM), the
 * silent ports, and the others (reported once).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 * | `port` | `uint16_t` | [in] The port. |
 * | `size` | `uint32_t` | [in] The access size in bytes. |
 * | `is_in` | `bool` | [in] true for a read. |
 * | `value` | `uint32_t *` | [out] The value written, or where the value read is stored. |
 *
 * @return HV_RESET if the guest asked for a reset, HV_CONTINUE otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `action` | `hv_action_e` | What the vCPU does next |
 *
 * @hlr
 * - \hlrid{HLR_IOPORT_OTHER_PORT_01}: `other_port()` shall handle an access to
 *   a port that is not an emulated device: all ones and no effect, the silent
 *   ports not reported and the others reported once.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_006}
 * - \hlrid{HLR_IOPORT_OTHER_PORT_02}: `other_port()` shall reset the VM when
 *   the guest writes `0xFE` to port `0x64` or a CPU reset to port `0xCF9`.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_007}
 *
 * @llr
 * - \llrid{LLR_IOPORT_OTHER_PORT_01}: If `is_in` is true and `port` is
 *   `PORT_KBD_STATUS`, `PORT_KBD_DATA` or `PORT_RESET_CONTROL`, `other_port()`
 *   shall store 0 at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_OTHER_PORT_01}
 * - \llrid{LLR_IOPORT_OTHER_PORT_02}: If `is_in` is true and `port` is none of
 *   `PORT_KBD_STATUS`, `PORT_KBD_DATA` and `PORT_RESET_CONTROL`, `other_port()`
 *   shall store the result of `all_ones()` for `size` at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_OTHER_PORT_01}
 * - \llrid{LLR_IOPORT_OTHER_PORT_03}: If `is_in` is false, `other_port()` shall
 *   leave the value at `value` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_OTHER_PORT_01}
 * - \llrid{LLR_IOPORT_OTHER_PORT_04}: If `is_in` is false, `port` is
 *   `PORT_KBD_STATUS` and the low 8 bits of the value at `value` are
 *   `KBD_CMD_RESET`, `other_port()` shall return `HV_RESET`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_OTHER_PORT_02}
 * - \llrid{LLR_IOPORT_OTHER_PORT_05}: If `is_in` is false, `port` is
 *   `PORT_RESET_CONTROL` and a bit of `RESET_CONTROL_CPU` is set in the value
 *   at `value`, `other_port()` shall return `HV_RESET`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_OTHER_PORT_02}
 * - \llrid{LLR_IOPORT_OTHER_PORT_06}: If `is_in` is true, or `port` is
 *   `PORT_KBD_STATUS` and the low 8 bits of the value at `value` are not
 *   `KBD_CMD_RESET`, or `port` is `PORT_RESET_CONTROL` and no bit of
 *   `RESET_CONTROL_CPU` is set in the value at `value`, or `port` is neither
 *   `PORT_KBD_STATUS` nor `PORT_RESET_CONTROL`, `other_port()` shall return
 *   `HV_CONTINUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_OTHER_PORT_01}
 *   - \hlrref{HLR_IOPORT_OTHER_PORT_02}
 * - \llrid{LLR_IOPORT_OTHER_PORT_07}: If `port` is `PORT_KBD_STATUS`,
 *   `PORT_KBD_DATA`, `PORT_RESET_CONTROL`, `PORT_POST`, `PORT_DELAY` or a port
 *   from `PORT_PCI_FIRST` to `PORT_PCI_LAST` (`in_range()`), `other_port()`
 *   shall not call `report_port()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_OTHER_PORT_01}
 * - \llrid{LLR_IOPORT_OTHER_PORT_08}: If `port` is none of `PORT_KBD_STATUS`,
 *   `PORT_KBD_DATA`, `PORT_RESET_CONTROL`, `PORT_POST`, `PORT_DELAY` and the
 *   ports from `PORT_PCI_FIRST` to `PORT_PCI_LAST`, `other_port()` shall call
 *   `report_port()` once with `vm`, `port` and `is_in`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_OTHER_PORT_01}
 *
 * @design
 * 1. Set `action` to `HV_CONTINUE`. \llrref{LLR_IOPORT_OTHER_PORT_06}
 * 2. If `is_in` is true: \llrref{LLR_IOPORT_OTHER_PORT_01},
 *    \llrref{LLR_IOPORT_OTHER_PORT_02}, \llrref{LLR_IOPORT_OTHER_PORT_03}
 *    1. Call `all_ones()` with the argument `size`, and store the result in
 *       `*value`. \llrref{LLR_IOPORT_OTHER_PORT_02}
 * 3. If `port` is equal to `PORT_KBD_STATUS` or `port` is equal to
 *    `PORT_KBD_DATA`: \llrref{LLR_IOPORT_OTHER_PORT_01},
 *    \llrref{LLR_IOPORT_OTHER_PORT_04}, \llrref{LLR_IOPORT_OTHER_PORT_06},
 *    \llrref{LLR_IOPORT_OTHER_PORT_07}
 *    1. If `is_in` is true: \llrref{LLR_IOPORT_OTHER_PORT_01}
 *       1. Set `*value` to `0U`.
 *    2. Otherwise, if `port` is equal to `PORT_KBD_STATUS` and
 *       `(*value & 0xFFU)` is equal to `KBD_CMD_RESET`:
 *       \llrref{LLR_IOPORT_OTHER_PORT_04}, \llrref{LLR_IOPORT_OTHER_PORT_06}
 *       1. Set `action` to `HV_RESET`.
 * 4. Otherwise, if `port` is equal to `PORT_RESET_CONTROL`:
 *    \llrref{LLR_IOPORT_OTHER_PORT_01}, \llrref{LLR_IOPORT_OTHER_PORT_05},
 *    \llrref{LLR_IOPORT_OTHER_PORT_06}, \llrref{LLR_IOPORT_OTHER_PORT_07}
 *    1. If `is_in` is true: \llrref{LLR_IOPORT_OTHER_PORT_01}
 *       1. Set `*value` to `0U`.
 *    2. Otherwise, if `(*value & RESET_CONTROL_CPU)` is not equal to `0U`:
 *       \llrref{LLR_IOPORT_OTHER_PORT_05}, \llrref{LLR_IOPORT_OTHER_PORT_06}
 *       1. Set `action` to `HV_RESET`.
 * 5. Otherwise, if `port` is equal to `PORT_POST` or `port` is equal to
 *    `PORT_DELAY` or
 *    `in_range(port, PORT_PCI_FIRST, (PORT_PCI_LAST - PORT_PCI_FIRST) + 1U)` is
 *    true: \llrref{LLR_IOPORT_OTHER_PORT_07}
 * 6. Otherwise: \llrref{LLR_IOPORT_OTHER_PORT_08}
 *    1. Call `report_port()` with the arguments `vm`, `port` and `is_in`.
 * 7. Return `action`. \llrref{LLR_IOPORT_OTHER_PORT_04},
 *    \llrref{LLR_IOPORT_OTHER_PORT_05}, \llrref{LLR_IOPORT_OTHER_PORT_06}
 *
 * @calls
 * - `all_ones()`
 * - `in_range()`
 * - `report_port()`
 */
static hv_action_e other_port(hv_vm_t *vm, uint16_t port,
                              uint32_t size, bool is_in, uint32_t *value)
{
    hv_action_e action = HV_CONTINUE;

    // By default a read gets all ones, as from a bus without a device.
    if (is_in) {
        *value = all_ones(size);
    }
    // Writing 0xFE to the keyboard controller is the classic CPU reset.
    if ((port == PORT_KBD_STATUS) || (port == PORT_KBD_DATA)) {
        if (is_in) {
            *value = 0U;    // No data, input buffer empty
        } else if ((port == PORT_KBD_STATUS)
                   && ((*value & 0xFFU) == KBD_CMD_RESET)) {
            action = HV_RESET;
        } else {
            // Other keyboard controller commands are ignored.
        }
    } else if (port == PORT_RESET_CONTROL) {
        if (is_in) {
            *value = 0U;
        } else if ((*value & RESET_CONTROL_CPU) != 0U) {
            action = HV_RESET;
        } else {
            // Reset not requested.
        }
    } else if ((port == PORT_POST) || (port == PORT_DELAY)
               || in_range(port, PORT_PCI_FIRST,
                           (PORT_PCI_LAST - PORT_PCI_FIRST) + 1U)) {
        // Silent.
    } else {
        report_port(vm, port, is_in);
    }
    return action;
}

/**
 * @funcname `io_access`
 *
 * @description
 * Performs an I/O access of a VM after an IOIO exit, dispatching it to the
 * emulated device of the port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 * | `port` | `uint16_t` | [in] The port. |
 * | `size` | `uint32_t` | [in] The access size in bytes. |
 * | `is_in` | `bool` | [in] true for a read. |
 * | `value` | `uint32_t *` | [in,out] The value written, or where the value read is stored. |
 *
 * @return What the vCPU does next: HV_CONTINUE or HV_RESET.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `action` | `hv_action_e` | What the vCPU does next |
 * | `byte` | `uint8_t` | The low byte of the value (the emulated devices are 8-bit) |
 *
 * @hlr
 * - \hlrid{HLR_IOPORT_IO_ACCESS_01}: `io_access()` shall perform a port access
 *   of a guest with the device of the port: the console and GDB ports, the
 *   8259A pair, the 8254 timer, port B and the other ports.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_001}
 *   - \srqref{SRQ_HV_DEV_006}
 *
 * @llr
 * - \llrid{LLR_IOPORT_IO_ACCESS_01}: If `port` is one of the `UART_PORTS` ports
 *   from `HV_PORT_COM1` (`in_range()`), `io_access()` shall call
 *   `serial_access()` once with the console port of the VM's configuration,
 *   `port` minus `HV_PORT_COM1`, `size`, `is_in` and `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 * - \llrid{LLR_IOPORT_IO_ACCESS_02}: If `port` is one of the `UART_PORTS` ports
 *   from `HV_PORT_COM2`, `io_access()` shall call `serial_access()` once with
 *   the GDB port of the VM's configuration, `port` minus `HV_PORT_COM2`,
 *   `size`, `is_in` and `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 * - \llrid{LLR_IOPORT_IO_ACCESS_03}: If `port` is one of the 2 ports from
 *   `PORT_PIC_MASTER` or of the 2 ports from `PORT_PIC_SLAVE`, and `is_in` is
 *   true, `io_access()` shall store the result of `vpic_read()` for
 *   `vm->arch.pic` and `port` at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 * - \llrid{LLR_IOPORT_IO_ACCESS_04}: If `port` is one of the 2 ports from
 *   `PORT_PIC_MASTER` or of the 2 ports from `PORT_PIC_SLAVE`, and `is_in` is
 *   false, `io_access()` shall call `vpic_write()` once with `vm->arch.pic`,
 *   `port` and the low 8 bits of the value at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 * - \llrid{LLR_IOPORT_IO_ACCESS_05}: If `port` is a port from `PORT_PIT_FIRST`
 *   to `PORT_PIT_LAST` and `is_in` is true, `io_access()` shall store the
 *   result of `vpit_read()` for `vm->arch.pit` and `port` at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 * - \llrid{LLR_IOPORT_IO_ACCESS_06}: If `port` is a port from `PORT_PIT_FIRST`
 *   to `PORT_PIT_LAST` and `is_in` is false, `io_access()` shall call
 *   `vpit_write()` once with `vm->arch.pit`, `port` and the low 8 bits of the
 *   value at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 * - \llrid{LLR_IOPORT_IO_ACCESS_07}: If `port` is `PORT_PORT_B` and `is_in` is
 *   true, `io_access()` shall store the result of `vpit_read_port_b()` for
 *   `vm->arch.pit` at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 * - \llrid{LLR_IOPORT_IO_ACCESS_08}: If `port` is `PORT_PORT_B` and `is_in` is
 *   false, `io_access()` shall call `vpit_write_port_b()` once with
 *   `vm->arch.pit` and the low 8 bits of the value at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 * - \llrid{LLR_IOPORT_IO_ACCESS_09}: If `port` is a port of the `UART_PORTS`
 *   from `HV_PORT_COM1` or from `HV_PORT_COM2`, of the 2 from `PORT_PIC_MASTER`
 *   or from `PORT_PIC_SLAVE`, from `PORT_PIT_FIRST` to `PORT_PIT_LAST`, or
 *   `PORT_PORT_B`, `io_access()` shall return `HV_CONTINUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 * - \llrid{LLR_IOPORT_IO_ACCESS_10}: If `port` is none of the `UART_PORTS`
 *   ports from `HV_PORT_COM1` and from `HV_PORT_COM2`, the 2 ports from
 *   `PORT_PIC_MASTER` and from `PORT_PIC_SLAVE`, the ports from
 *   `PORT_PIT_FIRST` to `PORT_PIT_LAST` and `PORT_PORT_B`, `io_access()` shall
 *   call `other_port()` once with `vm`, `port`, `size`, `is_in` and `value`,
 *   and return its result.
 *   <br>Traces to:
 *   - \hlrref{HLR_IOPORT_IO_ACCESS_01}
 *
 * @design
 * 1. Set `action` to `HV_CONTINUE`. \llrref{LLR_IOPORT_IO_ACCESS_09}
 * 2. Set `byte` to `(uint8_t) *value`. \llrref{LLR_IOPORT_IO_ACCESS_04},
 *    \llrref{LLR_IOPORT_IO_ACCESS_06}, \llrref{LLR_IOPORT_IO_ACCESS_08}
 * 3. If `in_range(port, HV_PORT_COM1, UART_PORTS)` is true:
 *    \llrref{LLR_IOPORT_IO_ACCESS_01}
 *    1. Call `serial_access()` with the arguments `vm->config->console_port`,
 *       `(uint16_t) (port - HV_PORT_COM1)`, `size`, `is_in` and `value`.
 * 4. Otherwise, if `in_range(port, HV_PORT_COM2, UART_PORTS)` is true:
 *    \llrref{LLR_IOPORT_IO_ACCESS_02}
 *    1. Call `serial_access()` with the arguments `vm->config->gdb_port`,
 *       `(uint16_t) (port - HV_PORT_COM2)`, `size`, `is_in` and `value`.
 * 5. Otherwise, if
 *    `in_range(port, PORT_PIC_MASTER, 2U) || in_range(port, PORT_PIC_SLAVE,`
 *    `2U)` holds: \llrref{LLR_IOPORT_IO_ACCESS_03},
 *    \llrref{LLR_IOPORT_IO_ACCESS_04}
 *    1. If `is_in` is true: \llrref{LLR_IOPORT_IO_ACCESS_03}
 *       1. Call `vpic_read()` with the arguments the address of `vm->arch.pic`
 *          and `port`, and store the result in `*value`.
 *    2. Otherwise: \llrref{LLR_IOPORT_IO_ACCESS_04}
 *       1. Call `vpic_write()` with the arguments the address of
 *          `vm->arch.pic`, `port` and `byte`.
 * 6. Otherwise, if
 *    `in_range(port, PORT_PIT_FIRST, (PORT_PIT_LAST - PORT_PIT_FIRST) + 1U)` is
 *    true: \llrref{LLR_IOPORT_IO_ACCESS_05}, \llrref{LLR_IOPORT_IO_ACCESS_06}
 *    1. If `is_in` is true: \llrref{LLR_IOPORT_IO_ACCESS_05}
 *       1. Call `vpit_read()` with the arguments the address of `vm->arch.pit`
 *          and `port`, and store the result in `*value`.
 *    2. Otherwise: \llrref{LLR_IOPORT_IO_ACCESS_06}
 *       1. Call `vpit_write()` with the arguments the address of
 *          `vm->arch.pit`, `port` and `byte`.
 * 7. Otherwise, if `port` is equal to `PORT_PORT_B`:
 *    \llrref{LLR_IOPORT_IO_ACCESS_07}, \llrref{LLR_IOPORT_IO_ACCESS_08}
 *    1. If `is_in` is true: \llrref{LLR_IOPORT_IO_ACCESS_07}
 *       1. Call `vpit_read_port_b()` with the address of `vm->arch.pit`, and
 *          store the result in `*value`.
 *    2. Otherwise: \llrref{LLR_IOPORT_IO_ACCESS_08}
 *       1. Call `vpit_write_port_b()` with the arguments the address of
 *          `vm->arch.pit` and `byte`.
 * 8. Otherwise: \llrref{LLR_IOPORT_IO_ACCESS_10}
 *    1. Call `other_port()` with the arguments `vm`, `port`, `size`, `is_in`
 *       and `value`, and store the result in `action`.
 * 9. Return `action`. \llrref{LLR_IOPORT_IO_ACCESS_09},
 *    \llrref{LLR_IOPORT_IO_ACCESS_10}
 *
 * @calls
 * - `in_range()`
 * - `serial_access()`
 * - `vpic_read()`
 * - `vpic_write()`
 * - `vpit_read()`
 * - `vpit_write()`
 * - `vpit_read_port_b()`
 * - `vpit_write_port_b()`
 * - `other_port()`
 */
hv_action_e io_access(hv_vm_t *vm, uint16_t port, uint32_t size,
                      bool is_in, uint32_t *value)
{
    hv_action_e action = HV_CONTINUE;
    uint8_t byte = (uint8_t) *value;    // The emulated devices are 8-bit

    // The guest's COM1 and COM2 go to the host ports of the VM.
    if (in_range(port, HV_PORT_COM1, UART_PORTS)) {
        serial_access(vm->config->console_port,
                      (uint16_t) (port - HV_PORT_COM1), size, is_in, value);
    } else if (in_range(port, HV_PORT_COM2, UART_PORTS)) {
        serial_access(vm->config->gdb_port,
                      (uint16_t) (port - HV_PORT_COM2), size, is_in, value);
    } else if (in_range(port, PORT_PIC_MASTER, 2U)
               || in_range(port, PORT_PIC_SLAVE, 2U)) {
        if (is_in) {
            *value = vpic_read(&vm->arch.pic, port);
        } else {
            vpic_write(&vm->arch.pic, port, byte);
        }
    } else if (in_range(port, PORT_PIT_FIRST,
                        (PORT_PIT_LAST - PORT_PIT_FIRST) + 1U)) {
        if (is_in) {
            *value = vpit_read(&vm->arch.pit, port);
        } else {
            vpit_write(&vm->arch.pit, port, byte);
        }
    } else if (port == PORT_PORT_B) {
        if (is_in) {
            *value = vpit_read_port_b(&vm->arch.pit);
        } else {
            vpit_write_port_b(&vm->arch.pit, byte);
        }
    } else {
        action = other_port(vm, port, size, is_in, value);
    }
    return action;
}
