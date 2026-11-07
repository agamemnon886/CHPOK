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
 * I/O ports of the guests. Every port access exits, except to a guest
 * serial port forwarded to the host port at the same address, which the
 * guest uses directly. The guest sees:
 *
 *   0x20-0x21, 0xA0-0xA1   virtual 8259A PIC pair (vpic.c)
 *   0x40-0x43, 0x61        virtual 8254 PIT and port B (vpit.c)
 *   0x3F8-0x3FF            COM1, forwarded to the VM's console port
 *   0x2F8-0x2FF            COM2, forwarded to the VM's GDB port, if any
 *   0x60, 0x64             keyboard controller: writing 0xFE resets
 *   0xCF9                  reset control: bit 2 resets
 *
 * Other ports read as all ones and ignore writes (the first access to
 * each one is reported), except the POST/delay ports and the PCI
 * configuration ports (no PCI devices), which are silent.
 */

#include <hv/hv.h>

#define PORT_PIC_MASTER     0x20U
#define PORT_PIC_SLAVE      0xA0U
#define PORT_PIT_FIRST      0x40U
#define PORT_PIT_LAST       0x43U
#define PORT_KBD_DATA       0x60U
#define PORT_PORT_B         0x61U
#define PORT_KBD_STATUS     0x64U
#define PORT_POST           0x80U
#define PORT_DELAY          0xEDU
#define PORT_PCI_FIRST      0xCF8U
#define PORT_PCI_LAST       0xCFFU
#define PORT_RESET_CONTROL  0xCF9U
#define UART_PORTS          8U

#define KBD_CMD_RESET       0xFEU
#define RESET_CONTROL_CPU   0x04U

/*
 * Let the guest access the count ports from first directly (clear their bits in
 * the IOPM).
 */
static void iopm_allow(uint8_t *iopm, uint16_t first, uint32_t count)
{
    for (uint32_t port = first; port < ((uint32_t) first + count); port++) {
        iopm[port / 8U] &= (uint8_t) ~(1U << (port % 8U));
    }
}

/*
 * Build the IOPM of a VM: every port access exits, except to a serial port
 * forwarded to the host port at the same address.
 */
void io_init_iopm(hv_vm_t *vm)
{
    // A set bit makes the access to that port exit.
    hv_memset(vm->iopm, 0xFFU, SVM_IOPM_SIZE);
    if (vm->config->console_port == HV_PORT_COM1) {
        iopm_allow(vm->iopm, HV_PORT_COM1, UART_PORTS);
    }
    if (vm->config->gdb_port == HV_PORT_COM2) {
        iopm_allow(vm->iopm, HV_PORT_COM2, UART_PORTS);
    }
}

// Check whether port is one of the count ports from first.
static bool in_range(uint16_t port, uint16_t first, uint32_t count)
{
    return (port >= first) && ((uint32_t) port < ((uint32_t) first + count));
}

// Value read from an absent port of the given size (1, 2 or 4 bytes).
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

// A guest serial port forwarded to host_base (or absent if none).
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

// Report the first access to an unhandled port of a VM.
static void report_port(hv_vm_t *vm, uint16_t port, bool is_in)
{
    // One bit per port in io_logged.
    uint32_t byte = (uint32_t) port / 8U;
    uint8_t bit = (uint8_t) (1U << ((uint32_t) port % 8U));

    if ((vm->io_logged[byte] & bit) == 0U) {
        vm->io_logged[byte] |= bit;
        hv_printf("[hv] %s: %s of I/O port 0x%x ignored\n",
                  vm->config->name, is_in ? "read" : "write",
                  (uint32_t) port);
    }
}

/*
 * Access of a VM to a port that is not a device: the keyboard controller and
 * reset control (reset the VM), silent ports and the others (reported once).
 */
static hv_action_t other_port(hv_vm_t *vm, uint16_t port,
                              uint32_t size, bool is_in, uint32_t *value)
{
    hv_action_t action = HV_CONTINUE;

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

/*
 * Perform an I/O access of a VM: port, size in bytes, direction; *value is the
 * value written or receives the value read. Returns what the vCPU does next.
 */
hv_action_t io_access(hv_vm_t *vm, uint16_t port, uint32_t size,
                      bool is_in, uint32_t *value)
{
    hv_action_t action = HV_CONTINUE;
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
            *value = vpic_read(&vm->pic, port);
        } else {
            vpic_write(&vm->pic, port, byte);
        }
    } else if (in_range(port, PORT_PIT_FIRST,
                        (PORT_PIT_LAST - PORT_PIT_FIRST) + 1U)) {
        if (is_in) {
            *value = vpit_read(&vm->pit, port);
        } else {
            vpit_write(&vm->pit, port, byte);
        }
    } else if (port == PORT_PORT_B) {
        if (is_in) {
            *value = vpit_read_port_b(&vm->pit);
        } else {
            vpit_write_port_b(&vm->pit, byte);
        }
    } else {
        action = other_port(vm, port, size, is_in, value);
    }
    return action;
}
