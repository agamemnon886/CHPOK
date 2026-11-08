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
 * arch/x86_64/; hv_port_name() (from vm.c) names the serial ports.
 */

/*
 * Polled 16550 UART output for the hypervisor's own console. The guests'
 * serial ports are forwarded by vuart.c and programmed by the guests.
 */

#include <arch/internal.h>

#define UART_THR    0U      // Transmit holding register (DLAB = 0)
#define UART_IER    1U      // Interrupt enable (DLAB = 0)
#define UART_DLL    0U      // Divisor latch, low byte (DLAB = 1)
#define UART_DLM    1U      // Divisor latch, high byte (DLAB = 1)
#define UART_FCR    2U
#define UART_LCR    3U
#define UART_MCR    4U
#define UART_LSR    5U

#define UART_LSR_THRE   0x20U   // Transmit holding register empty

// Write a register of the UART at port.
static void uart_out(uint16_t port, uint16_t reg, uint8_t value)
{
    x86_outb((uint16_t) (port + reg), value);
}

// Set up the UART at port: 38400 baud, 8N1, FIFOs on, no interrupts.
void uart_init(uint16_t port)
{
    uart_out(port, UART_IER, 0x00U);    // No interrupts
    uart_out(port, UART_LCR, 0x80U);    // DLAB: set the divisor
    uart_out(port, UART_DLL, 0x03U);    // 38400 baud
    uart_out(port, UART_DLM, 0x00U);
    uart_out(port, UART_LCR, 0x03U);    // 8 bits, no parity, 1 stop
    uart_out(port, UART_FCR, 0xC7U);    // FIFOs on and cleared
    uart_out(port, UART_MCR, 0x03U);    // DTR, RTS; no interrupt line
}

// Write a character to the UART at port, waiting for room in it.
void uart_putc(uint16_t port, char c)
{
    while ((x86_inb((uint16_t) (port + UART_LSR)) & UART_LSR_THRE) == 0U) {
        x86_pause();
    }
    uart_out(port, UART_THR, (uint8_t) c);
}

// Name of a serial port (COM1-COM4, or "none").
const char *hv_port_name(uint16_t port)
{
    const char *name;

    switch (port) {
        case HV_PORT_COM1:
            name = "COM1";
            break;
        case HV_PORT_COM2:
            name = "COM2";
            break;
        case HV_PORT_COM3:
            name = "COM3";
            break;
        case HV_PORT_COM4:
            name = "COM4";
            break;
        default:
            name = "none";
            break;
    }
    return name;
}
