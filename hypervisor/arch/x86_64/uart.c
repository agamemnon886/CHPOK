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
 * arch/x86_64/; hv_port_name() (from vm.c) names the serial ports. Doxygen
 * headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and external
 * declarations are in arch/internal.h, documented there.
 */

/**
 * @file
 * Polled 16550 UART output for the hypervisor's own console. The guests'
 * serial ports are forwarded by vuart.c and programmed by the guests.
 */

#include <arch/internal.h>

/**
 * @funcname `uart_out`
 *
 * @description
 * Writes a register of a 16550 UART.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The UART's I/O base address. |
 * | `reg` | `uint16_t` | [in] The register offset. |
 * | `value` | `uint8_t` | [in] The value. |
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
 * 1. Call `x86_outb()` with the arguments `(uint16_t) (port + reg)` and
 *    `value`.
 *
 * @calls
 * - `x86_outb()`
 */
static void uart_out(uint16_t port, uint16_t reg, uint8_t value)
{
    x86_outb((uint16_t) (port + reg), value);
}

/**
 * @funcname `uart_init`
 *
 * @description
 * Sets up a 16550 UART for the hypervisor console: 38400 baud, 8 data bits,
 * no parity, one stop bit, FIFOs on, no interrupts.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The UART's I/O base address. |
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
 * 1. Call `uart_out()` with the arguments `port`, `UART_IER` and `0x00U`.
 * 2. Call `uart_out()` with the arguments `port`, `UART_LCR` and `0x80U`.
 * 3. Call `uart_out()` with the arguments `port`, `UART_DLL` and `0x03U`.
 * 4. Call `uart_out()` with the arguments `port`, `UART_DLM` and `0x00U`.
 * 5. Call `uart_out()` with the arguments `port`, `UART_LCR` and `0x03U`.
 * 6. Call `uart_out()` with the arguments `port`, `UART_FCR` and `0xC7U`.
 * 7. Call `uart_out()` with the arguments `port`, `UART_MCR` and `0x03U`.
 *
 * @calls
 * - `uart_out()`
 */
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

/**
 * @funcname `uart_putc`
 *
 * @description
 * Writes a character to a 16550 UART, waiting for room in it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The UART's I/O base address. |
 * | `c` | `char` | [in] The character. |
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
 * 1. While `(x86_inb((uint16_t) (port + UART_LSR)) & UART_LSR_THRE)` is equal
 *    to `0U`:
 *    1. Call `x86_pause()`.
 * 2. Call `uart_out()` with the arguments `port`, `UART_THR` and `(uint8_t) c`.
 *
 * @calls
 * - `x86_inb()`
 * - `x86_pause()`
 * - `uart_out()`
 */
void uart_putc(uint16_t port, char c)
{
    while ((x86_inb((uint16_t) (port + UART_LSR)) & UART_LSR_THRE) == 0U) {
        x86_pause();
    }
    uart_out(port, UART_THR, (uint8_t) c);
}

/**
 * @funcname `hv_port_name`
 *
 * @description
 * Returns the name of a host serial port, for the messages.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port's I/O base address, or HV_PORT_NONE. |
 *
 * @return "COM1" to "COM4", or "none".
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char *` | The name |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Depending on `port`:
 *    1. Case `HV_PORT_COM1`:
 *       1. Set `name` to `"COM1"`.
 *       2. Leave the switch.
 *    2. Case `HV_PORT_COM2`:
 *       1. Set `name` to `"COM2"`.
 *       2. Leave the switch.
 *    3. Case `HV_PORT_COM3`:
 *       1. Set `name` to `"COM3"`.
 *       2. Leave the switch.
 *    4. Case `HV_PORT_COM4`:
 *       1. Set `name` to `"COM4"`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Set `name` to `"none"`.
 *       2. Leave the switch.
 * 2. Return `name`.
 *
 * @calls
 * None.
 */
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
