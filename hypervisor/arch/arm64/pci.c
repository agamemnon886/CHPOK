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
 * The PCI serial ports of the host: qemu.sh (hvconfig.py) puts COMn, for
 * n = 3 to 16, as a QEMU pci-serial device (a 16550, 1b36:0002) in slot
 * 0x10 + n - 3 of bus 0. Nothing assigns their BARs when QEMU loads the
 * hypervisor directly, so each one gets I/O space 0x1000 + 0x10 * (n - 3)
 * here, and its I/O decoding is turned on.
 */

#include <arch/internal.h>

#define PCI_VENDOR_REDHAT       0x1B36U
#define PCI_DEVICE_SERIAL       0x0002U

#define PCI_ID                  0x00U
#define PCI_COMMAND             0x04U
#define PCI_COMMAND_IO          0x0001U
#define PCI_BAR0                0x10U

#define SLOT_SHIFT              15U         // ECAM: bus 0, function 0
#define FIRST_SLOT              0x10U       // COM3
#define FIRST_PORT              3U
#define IO_BASE                 0x1000U
#define IO_STEP                 0x10U
#define SLOTS                   32U

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

static void config_write(uint64_t ecam, uint32_t slot, uint32_t reg,
                         uint32_t value)
{
    uint64_t address = ecam + ((uint64_t) slot << SLOT_SHIFT) + reg;

    *(volatile uint32_t *) (uintptr_t) address = value;
}

/*
 * Find the PCI serial ports in the configuration space at ecam_base
 * (bus 0 only), give them I/O space and record them (hv_uart_add()).
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
