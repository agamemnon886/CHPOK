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
 * hypervisor/arch/arm64/pci.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps).
 */

/**
 * @file
 * The PCI serial ports of the host: qemu.sh (hvconfig.py) puts COMn, for n =
 * 3 to 16, as a QEMU pci-serial device (a 16550, 1b36:0002) in slot
 * 0x10 + n - 3 of bus 0. Nothing assigns their BARs when QEMU loads the
 * hypervisor directly, so each one gets I/O space 0x1000 + 0x10 * (n - 3)
 * here, and its I/O decoding is turned on.
 */

#include <arch/internal.h>

/**
 * @funcname `config_read`
 *
 * @description
 * Reads a 32-bit register of the PCI configuration space of a device of bus 0
 * (ECAM).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ecam` | `uint64_t` | [in] The physical address of the configuration space. |
 * | `slot` | `uint32_t` | [in] The device number. |
 * | `reg` | `uint32_t` | [in] The register offset. |
 *
 * @return The register value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `address` | `uint64_t` | The register's physical address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `address` to `ecam + ((uint64_t) slot << SLOT_SHIFT) + reg`.
 * 2. Return the value at `(const volatile uint32_t *) (uintptr_t) address`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (config_read(), config_write()): the
 * PCI configuration space is used by physical address (identity-mapped),
 * converted to pointers.
 */
static uint32_t config_read(uint64_t ecam, uint32_t slot, uint32_t reg)
{
    uint64_t address = ecam + ((uint64_t) slot << SLOT_SHIFT) + reg;

    return *(const volatile uint32_t *) (uintptr_t) address;
}

/**
 * @funcname `config_write`
 *
 * @description
 * Writes a 32-bit register of the PCI configuration space of a device of bus 0
 * (ECAM).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ecam` | `uint64_t` | [in] The physical address of the configuration space. |
 * | `slot` | `uint32_t` | [in] The device number. |
 * | `reg` | `uint32_t` | [in] The register offset. |
 * | `value` | `uint32_t` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `address` | `uint64_t` | The register's physical address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `address` to `ecam + ((uint64_t) slot << SLOT_SHIFT) + reg`.
 * 2. Set `*(volatile uint32_t *) (uintptr_t) address` to `value`.
 *
 * @calls
 * None.
 */
static void config_write(uint64_t ecam, uint32_t slot, uint32_t reg,
                         uint32_t value)
{
    uint64_t address = ecam + ((uint64_t) slot << SLOT_SHIFT) + reg;

    *(volatile uint32_t *) (uintptr_t) address = value;
}

/**
 * @funcname `pci_find_serial_ports`
 *
 * @description
 * Finds the PCI serial ports in the slots qemu.sh gives them (bus 0), gives
 * each one I/O space and records it as a host port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ecam_base` | `uint64_t` | [in] The physical address of the configuration space (0: no PCI). |
 * | `ecam_size` | `uint64_t` | [in] Its size. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint32_t` | The port number n of COMn |
 * | `slot` | `uint32_t` | The slot of COMn |
 * | `id` | `uint32_t` | The device and vendor ID in the slot |
 * | `io` | `uint32_t` | The I/O address given to the port |
 * | `command` | `uint32_t` | The command register |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `ecam_base` is equal to `0U` or `ecam_size` is less than
 *    `((uint64_t) SLOTS << SLOT_SHIFT)`:
 *    1. Return.
 * 2. Set `port` to `FIRST_PORT`; then while `port` is less than or equal to
 *    `HV_NB_PORTS`, repeat (after each pass: increment `port`):
 *    1. Set `slot` to `FIRST_SLOT + (port - FIRST_PORT)`.
 *    2. Call `config_read()` with the arguments `ecam_base`, `slot` and
 *       `PCI_ID`, and store the result in `id`.
 *    3. If `id` is equal to
 *       `(((uint32_t) PCI_DEVICE_SERIAL << 16) | PCI_VENDOR_REDHAT)`:
 *       1. Set `io` to `IO_BASE + (IO_STEP * (port - FIRST_PORT))`.
 *       2. Call `config_read()` with the arguments `ecam_base`, `slot` and
 *          `PCI_COMMAND`, and store the result in `command`.
 *       3. Call `config_write()` with the arguments `ecam_base`, `slot`,
 *          `PCI_BAR0` and `io`.
 *       4. Call `config_write()` with the arguments `ecam_base`, `slot`,
 *          `PCI_COMMAND` and `(command & 0xFFFFU) | PCI_COMMAND_IO`.
 *       5. Call `hv_uart_add()` with the arguments `(uint16_t) port`,
 *          `HV_HOST_PCI_IO + io` and `false`.
 *
 * @calls
 * - `config_read()`
 * - `config_write()`
 * - `hv_uart_add()`
 */
void pci_find_serial_ports(uint64_t ecam_base, uint64_t ecam_size)
{
    if ((ecam_base == 0U) || (ecam_size < ((uint64_t) SLOTS << SLOT_SHIFT))) {
        return;             // No PCI bus (or no room for bus 0)
    }
    for (uint32_t port = FIRST_PORT; port <= HV_NB_PORTS; port++) {
        uint32_t slot = FIRST_SLOT + (port - FIRST_PORT);
        uint32_t id = config_read(ecam_base, slot, PCI_ID);

        if (id == (((uint32_t) PCI_DEVICE_SERIAL << 16)
                   | PCI_VENDOR_REDHAT)) {
            uint32_t io = IO_BASE + (IO_STEP * (port - FIRST_PORT));
            uint32_t command = config_read(ecam_base, slot, PCI_COMMAND);

            config_write(ecam_base, slot, PCI_BAR0, io);
            config_write(ecam_base, slot, PCI_COMMAND,
                         (command & 0xFFFFU) | PCI_COMMAND_IO);
            hv_uart_add((uint16_t) port, HV_HOST_PCI_IO + io, false);
        }
    }
}
