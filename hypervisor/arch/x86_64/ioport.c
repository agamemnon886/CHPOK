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
 *
 * @llr
 *
 * @design
 * 1. Set `port` to `first`; then while `port` is less than
 *    `((uint32_t) first + count)`, repeat (after each pass: increment `port`):
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
 *
 * @llr
 *
 * @design
 * 1. Call `hv_memset()` with the arguments `vm->arch.iopm`, `0xFFU` and
 *    `SVM_IOPM_SIZE`.
 * 2. If `vm->config->console_port` is equal to `HV_PORT_COM1`:
 *    1. Call `iopm_allow()` with the arguments `vm->arch.iopm`, `HV_PORT_COM1`
 *       and `UART_PORTS`.
 * 3. If `vm->config->gdb_port` is equal to `HV_PORT_COM2`:
 *    1. Call `iopm_allow()` with the arguments `vm->arch.iopm`, `HV_PORT_COM2`
 *       and `UART_PORTS`.
 *
 * @calls
 * - `hv_memset()`
 * - `iopm_allow()`
 */
void io_init_iopm(hv_vm_t *vm)
{
    // A set bit makes the access to that port exit.
    hv_memset(vm->arch.iopm, 0xFFU, SVM_IOPM_SIZE);
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
 *
 * @llr
 *
 * @design
 * 1. Return
 *    `(port >= first) && ((uint32_t) port < ((uint32_t) first + count))`.
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
 *
 * @llr
 *
 * @design
 * 1. If `size` is equal to `1U`:
 *    1. Set `value` to `0xFFU`.
 * 2. Otherwise, if `size` is equal to `2U`:
 *    1. Set `value` to `0xFFFFU`.
 * 3. Otherwise:
 *    1. Set `value` to `0xFFFFFFFFU`.
 * 4. Return `value`.
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
 *
 * @llr
 *
 * @design
 * 1. If `host_base` is not equal to `HV_PORT_NONE`:
 *    1. Call `vuart_access()` with the arguments `host_base`, `offset`, `size`,
 *       `is_in` and `value`.
 * 2. Otherwise, if `is_in` is true:
 *    1. Call `all_ones()` with the argument `size`, and store the result in
 *       `*value`.
 * 3. Otherwise:
 *    1. Do nothing.
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
 *
 * @llr
 *
 * @design
 * 1. Set `byte` to `(uint32_t) port / 8U`.
 * 2. Set `bit` to `(uint8_t) (1U << ((uint32_t) port % 8U))`.
 * 3. If `(vm->arch.io_logged[byte] & bit)` is equal to `0U`:
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
 *
 * @llr
 *
 * @design
 * 1. Set `action` to `HV_CONTINUE`.
 * 2. If `is_in` is true:
 *    1. Call `all_ones()` with the argument `size`, and store the result in
 *       `*value`.
 * 3. If `port` is equal to `PORT_KBD_STATUS` or `port` is equal to
 *    `PORT_KBD_DATA`:
 *    1. If `is_in` is true:
 *       1. Set `*value` to `0U`.
 *    2. Otherwise, if `port` is equal to `PORT_KBD_STATUS` and
 *       `(*value & 0xFFU)` is equal to `KBD_CMD_RESET`:
 *       1. Set `action` to `HV_RESET`.
 *    3. Otherwise:
 *       1. Do nothing.
 * 4. Otherwise, if `port` is equal to `PORT_RESET_CONTROL`:
 *    1. If `is_in` is true:
 *       1. Set `*value` to `0U`.
 *    2. Otherwise, if `(*value & RESET_CONTROL_CPU)` is not equal to `0U`:
 *       1. Set `action` to `HV_RESET`.
 *    3. Otherwise:
 *       1. Do nothing.
 * 5. Otherwise, if `port` is equal to `PORT_POST` or `port` is equal to
 *    `PORT_DELAY` or
 *    `in_range(port, PORT_PCI_FIRST, (PORT_PCI_LAST - PORT_PCI_FIRST) + 1U)` is
 *    true:
 *    1. Do nothing.
 * 6. Otherwise:
 *    1. Call `report_port()` with the arguments `vm`, `port` and `is_in`.
 * 7. Return `action`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `action` to `HV_CONTINUE`.
 * 2. Set `byte` to `(uint8_t) *value`.
 * 3. If `in_range(port, HV_PORT_COM1, UART_PORTS)` is true:
 *    1. Call `serial_access()` with the arguments `vm->config->console_port`,
 *       `(uint16_t) (port - HV_PORT_COM1)`, `size`, `is_in` and `value`.
 * 4. Otherwise, if `in_range(port, HV_PORT_COM2, UART_PORTS)` is true:
 *    1. Call `serial_access()` with the arguments `vm->config->gdb_port`,
 *       `(uint16_t) (port - HV_PORT_COM2)`, `size`, `is_in` and `value`.
 * 5. Otherwise, if
 *    `in_range(port, PORT_PIC_MASTER, 2U) || in_range(port, PORT_PIC_SLAVE,`
 *    `2U)` holds:
 *    1. If `is_in` is true:
 *       1. Call `vpic_read()` with the arguments the address of `vm->arch.pic`
 *          and `port`, and store the result in `*value`.
 *    2. Otherwise:
 *       1. Call `vpic_write()` with the arguments the address of
 *          `vm->arch.pic`, `port` and `byte`.
 * 6. Otherwise, if
 *    `in_range(port, PORT_PIT_FIRST, (PORT_PIT_LAST - PORT_PIT_FIRST) + 1U)` is
 *    true:
 *    1. If `is_in` is true:
 *       1. Call `vpit_read()` with the arguments the address of `vm->arch.pit`
 *          and `port`, and store the result in `*value`.
 *    2. Otherwise:
 *       1. Call `vpit_write()` with the arguments the address of
 *          `vm->arch.pit`, `port` and `byte`.
 * 7. Otherwise, if `port` is equal to `PORT_PORT_B`:
 *    1. If `is_in` is true:
 *       1. Call `vpit_read_port_b()` with the address of `vm->arch.pit`, and
 *          store the result in `*value`.
 *    2. Otherwise:
 *       1. Call `vpit_write_port_b()` with the arguments the address of
 *          `vm->arch.pit` and `byte`.
 * 8. Otherwise:
 *    1. Call `other_port()` with the arguments `vm`, `port`, `size`, `is_in`
 *       and `value`, and store the result in `action`.
 * 9. Return `action`.
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
