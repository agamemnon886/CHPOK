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
 * The host's serial ports, polled: COM1 and COM2 are QEMU's PL011s, COM3
 * and up 16550s on the PCI bus (pci.c finds them), whose registers are in
 * the PCI I/O space window, one byte each. They carry the hypervisor's
 * console and the guest serial ports that are not passed through
 * (vuart.c).
 */

#include <arch/internal.h>

// PL011 registers.
#define PL011_DR            0x000U
#define PL011_FR            0x018U
#define PL011_FR_RXFE       (1U << 4)
#define PL011_FR_TXFF       (1U << 5)
#define PL011_FR_TXFE       (1U << 7)

// 16550 registers.
#define UART_RBR            0U      // Receive buffer (DLAB = 0)
#define UART_THR            0U      // Transmit holding register (DLAB = 0)
#define UART_IER            1U      // Interrupt enable (DLAB = 0)
#define UART_DLL            0U      // Divisor latch, low byte (DLAB = 1)
#define UART_DLM            1U      // Divisor latch, high byte (DLAB = 1)
#define UART_FCR            2U
#define UART_LCR            3U
#define UART_MCR            4U
#define UART_LSR            5U
#define UART_LSR_DR         0x01U   // Data ready
#define UART_LSR_THRE       0x20U   // Transmit holding register empty
#define UART_LSR_TEMT       0x40U   // Transmitter empty

typedef struct {
    uint64_t base;              // Physical address of the registers (0: none)
    bool pl011;
} host_port_t;

// COM1 to COM16, by number (index 0 unused).
static host_port_t ports[HV_NB_PORTS + 1U];

/*
 * MISRA C:2025 Rule 11.4 deviation (the register accessors below): the
 * UARTs' registers are used by physical address (identity-mapped),
 * converted to pointers.
 */
static uint32_t reg_read32(uint64_t address)
{
    return *(const volatile uint32_t *) (uintptr_t) address;
}

static void reg_write32(uint64_t address, uint32_t value)
{
    *(volatile uint32_t *) (uintptr_t) address = value;
}

static uint8_t reg_read8(uint64_t address)
{
    return *(const volatile uint8_t *) (uintptr_t) address;
}

static void reg_write8(uint64_t address, uint8_t value)
{
    *(volatile uint8_t *) (uintptr_t) address = value;
}

// The port with this number, or NULL if there is none.
static const host_port_t *port_of(uint16_t port)
{
    const host_port_t *p = NULL;

    if ((port >= HV_PORT_COM1) && (port <= HV_NB_PORTS)
        && (ports[port].base != 0U)) {
        p = &ports[port];
    }
    return p;
}

// Record that COM port number port exists, with its registers at base.
void hv_uart_add(uint16_t port, uint64_t base, bool pl011)
{
    if ((port >= HV_PORT_COM1) && (port <= HV_NB_PORTS)) {
        ports[port].base = base;
        ports[port].pl011 = pl011;
    }
}

bool hv_uart_present(uint16_t port)
{
    return port_of(port) != NULL;
}

bool hv_uart_is_pl011(uint16_t port)
{
    const host_port_t *p = port_of(port);

    return (p != NULL) && p->pl011;
}

// Physical address of the registers of a port (0: none).
uint64_t hv_uart_base(uint16_t port)
{
    const host_port_t *p = port_of(port);

    return (p != NULL) ? p->base : 0U;
}

// Name of a serial port (COM1-COM16, or "none").
const char *hv_port_name(uint16_t port)
{
    static const char *const port_names[HV_NB_PORTS + 1U] = {
        "none", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
        "COM8", "COM9", "COM10", "COM11", "COM12", "COM13", "COM14",
        "COM15", "COM16"
    };

    return (port <= HV_NB_PORTS) ? port_names[port] : port_names[0];
}

/*
 * Set up a port: a 16550 gets 38400 baud, 8N1, FIFOs on, no interrupts;
 * QEMU's PL011 needs nothing.
 */
void uart_init(uint16_t port)
{
    const host_port_t *p = port_of(port);

    if ((p != NULL) && !p->pl011) {
        reg_write8(p->base + UART_IER, 0x00U);     // No interrupts
        reg_write8(p->base + UART_LCR, 0x80U);     // DLAB: set the divisor
        reg_write8(p->base + UART_DLL, 0x03U);     // 38400 baud
        reg_write8(p->base + UART_DLM, 0x00U);
        reg_write8(p->base + UART_LCR, 0x03U);     // 8 bits, no parity, 1 stop
        reg_write8(p->base + UART_FCR, 0xC7U);     // FIFOs on and cleared
        reg_write8(p->base + UART_MCR, 0x03U);     // DTR, RTS
    }
}

// Whether a port can take a byte now.
static bool tx_ready(const host_port_t *p)
{
    bool ready;

    if (p->pl011) {
        ready = (reg_read32(p->base + PL011_FR) & PL011_FR_TXFF) == 0U;
    } else {
        ready = (reg_read8(p->base + UART_LSR) & UART_LSR_THRE) != 0U;
    }
    return ready;
}

// Write a byte to a port, waiting for room in it (no port: dropped).
void hv_uart_write(uint16_t port, uint8_t byte)
{
    const host_port_t *p = port_of(port);

    if (p != NULL) {
        while (!tx_ready(p)) {
            arm64_yield();
        }
        if (p->pl011) {
            reg_write32(p->base + PL011_DR, byte);
        } else {
            reg_write8(p->base + UART_THR, byte);
        }
    }
}

// Write a character of the hypervisor's console.
void uart_putc(uint16_t port, char c)
{
    hv_uart_write(port, (uint8_t) c);
}

// Whether a port has received a byte.
bool hv_uart_rx_ready(uint16_t port)
{
    const host_port_t *p = port_of(port);
    bool ready = false;

    if (p == NULL) {
        // Nothing comes.
    } else if (p->pl011) {
        ready = (reg_read32(p->base + PL011_FR) & PL011_FR_RXFE) == 0U;
    } else {
        ready = (reg_read8(p->base + UART_LSR) & UART_LSR_DR) != 0U;
    }
    return ready;
}

// The next received byte of a port (0 if there is none).
uint8_t hv_uart_read(uint16_t port)
{
    const host_port_t *p = port_of(port);
    uint8_t byte = 0U;

    if ((p != NULL) && hv_uart_rx_ready(port)) {
        if (p->pl011) {
            byte = (uint8_t) reg_read32(p->base + PL011_DR);
        } else {
            byte = reg_read8(p->base + UART_RBR);
        }
    }
    return byte;
}

// Whether everything written to a port has been sent (no port: true).
bool hv_uart_tx_empty(uint16_t port)
{
    const host_port_t *p = port_of(port);
    bool empty = true;

    if (p == NULL) {
        // Nothing to send.
    } else if (p->pl011) {
        empty = (reg_read32(p->base + PL011_FR) & PL011_FR_TXFE) != 0U;
    } else {
        empty = (reg_read8(p->base + UART_LSR) & UART_LSR_TEMT) != 0U;
    }
    return empty;
}
