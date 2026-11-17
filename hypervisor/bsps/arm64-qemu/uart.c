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
 * hypervisor/arch/arm64/uart.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps).
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
 * The host's serial ports, polled: COM1 and COM2 are QEMU's PL011s, COM3
 * and up 16550s on the PCI bus (pci.c finds them), whose registers are in
 * the PCI I/O space window, one byte each. They carry the hypervisor's
 * console and the guest serial ports that are not passed through
 * (vuart.c).
 */

#include <arch/internal.h>

// COM1 to COM16, by number (index 0 unused).
static host_port_t ports[HV_NB_PORTS + 1U];

/**
 * @funcname `reg_read32`
 *
 * @description
 * Reads a 32-bit UART register (PL011).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `address` | `uint64_t` | [in] The register's physical address. |
 *
 * @return The register value.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UART_REG_READ32_01}: `reg_read32()` shall read a 32-bit register
 *   of a PL011 UART.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *
 * @llr
 * - \llrid{LLR_UART_REG_READ32_01}: `reg_read32()` shall return the 32-bit
 *   value at the address `address`, read once.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_REG_READ32_01}
 *
 * @design
 * 1. Return the value at `(const volatile uint32_t *) (uintptr_t) address`.
 *    \llrref{LLR_UART_REG_READ32_01}
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (the register accessors below): the
 * UARTs' registers are used by physical address (identity-mapped),
 * converted to pointers.
 */
static uint32_t reg_read32(uint64_t address)
{
    return *(const volatile uint32_t *) (uintptr_t) address;
}

/**
 * @funcname `reg_write32`
 *
 * @description
 * Writes a 32-bit UART register (PL011).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `address` | `uint64_t` | [in] The register's physical address. |
 * | `value` | `uint32_t` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UART_REG_WRITE32_01}: `reg_write32()` shall write a 32-bit
 *   register of a PL011 UART.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *
 * @llr
 * - \llrid{LLR_UART_REG_WRITE32_01}: `reg_write32()` shall write the 32-bit
 *   value `value` once at the address `address`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_REG_WRITE32_01}
 *
 * @design
 * 1. Set `*(volatile uint32_t *) (uintptr_t) address` to `value`.
 *    \llrref{LLR_UART_REG_WRITE32_01}
 *
 * @calls
 * None.
 */
static void reg_write32(uint64_t address, uint32_t value)
{
    *(volatile uint32_t *) (uintptr_t) address = value;
}

/**
 * @funcname `reg_read8`
 *
 * @description
 * Reads an 8-bit UART register (16550).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `address` | `uint64_t` | [in] The register's physical address. |
 *
 * @return The register value.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UART_REG_READ8_01}: `reg_read8()` shall read an 8-bit register
 *   of a 16550 UART.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *
 * @llr
 * - \llrid{LLR_UART_REG_READ8_01}: `reg_read8()` shall return the byte at the
 *   address `address`, read once.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_REG_READ8_01}
 *
 * @design
 * 1. Return the value at `(const volatile uint8_t *) (uintptr_t) address`.
 *    \llrref{LLR_UART_REG_READ8_01}
 *
 * @calls
 * None.
 */
static uint8_t reg_read8(uint64_t address)
{
    return *(const volatile uint8_t *) (uintptr_t) address;
}

/**
 * @funcname `reg_write8`
 *
 * @description
 * Writes an 8-bit UART register (16550).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `address` | `uint64_t` | [in] The register's physical address. |
 * | `value` | `uint8_t` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UART_REG_WRITE8_01}: `reg_write8()` shall write an 8-bit
 *   register of a 16550 UART.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *
 * @llr
 * - \llrid{LLR_UART_REG_WRITE8_01}: `reg_write8()` shall write the byte `value`
 *   once at the address `address`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_REG_WRITE8_01}
 *
 * @design
 * 1. Set `*(volatile uint8_t *) (uintptr_t) address` to `value`.
 *    \llrref{LLR_UART_REG_WRITE8_01}
 *
 * @calls
 * None.
 */
static void reg_write8(uint64_t address, uint8_t value)
{
    *(volatile uint8_t *) (uintptr_t) address = value;
}

/**
 * @funcname `port_of`
 *
 * @description
 * Returns the host port with a number, if it exists.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number (1 for COM1). |
 *
 * @return The port, or NULL if there is none.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ports` | `host_port_t[HV_NB_PORTS + 1U]` | COM1 to COM16, by number: registers and kind |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const host_port_t *` | The port, or NULL |
 *
 * @hlr
 * - \hlrid{HLR_UART_PORT_OF_01}: `port_of()` shall give a serial port of the
 *   host by its number, if it exists.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CFG_004}
 *
 * @llr
 * - \llrid{LLR_UART_PORT_OF_01}: If `port` is from `HV_PORT_COM1` to
 *   `HV_NB_PORTS` and `base` of `ports[port]` is not 0, `port_of()` shall
 *   return the address of `ports[port]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_PORT_OF_01}
 * - \llrid{LLR_UART_PORT_OF_02}: If `port` is less than `HV_PORT_COM1`, or more
 *   than `HV_NB_PORTS`, or `base` of `ports[port]` is 0, `port_of()` shall
 *   return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_PORT_OF_01}
 *
 * @design
 * 1. Set `p` to `NULL`. \llrref{LLR_UART_PORT_OF_02}
 * 2. If `port` is greater than or equal to `HV_PORT_COM1` and `port` is less
 *    than or equal to `HV_NB_PORTS` and `ports[port].base` is not equal to
 *    `0U`: \llrref{LLR_UART_PORT_OF_01}
 *    1. Set `p` to the address of `ports[port]`.
 * 3. Return `p`. \llrref{LLR_UART_PORT_OF_01}, \llrref{LLR_UART_PORT_OF_02}
 *
 * @calls
 * None.
 */
static const host_port_t *port_of(uint16_t port)
{
    const host_port_t *p = NULL;

    if ((port >= HV_PORT_COM1) && (port <= HV_NB_PORTS)
        && (ports[port].base != 0U)) {
        p = &ports[port];
    }
    return p;
}

/**
 * @funcname `hv_uart_add`
 *
 * @description
 * Records that a host COM port exists (from the device tree or the PCI scan).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 * | `base` | `uint64_t` | [in] The physical address of its registers. |
 * | `pl011` | `bool` | [in] true for a PL011, false for a 16550. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ports` | `host_port_t[HV_NB_PORTS + 1U]` | COM1 to COM16, by number: registers and kind |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UART_HV_UART_ADD_01}: `hv_uart_add()` shall record that a serial
 *   port of the host exists, with its address and kind.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_006}
 *   - \srqref{SRQ_HV_CFG_004}
 *
 * @llr
 * - \llrid{LLR_UART_HV_UART_ADD_01}: If and only if `port` is from
 *   `HV_PORT_COM1` to `HV_NB_PORTS`, `hv_uart_add()` shall set `base` of
 *   `ports[port]` to `base` and `pl011` of `ports[port]` to `pl011`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_ADD_01}
 *
 * @design
 * 1. If `port` is greater than or equal to `HV_PORT_COM1` and `port` is less
 *    than or equal to `HV_NB_PORTS`: \llrref{LLR_UART_HV_UART_ADD_01}
 *    1. Set `ports[port].base` to `base`.
 *    2. Set `ports[port].pl011` to `pl011`.
 *
 * @calls
 * None.
 */
void hv_uart_add(uint16_t port, uint64_t base, bool pl011)
{
    if ((port >= HV_PORT_COM1) && (port <= HV_NB_PORTS)) {
        ports[port].base = base;
        ports[port].pl011 = pl011;
    }
}

/**
 * @funcname `hv_uart_present`
 *
 * @description
 * Tells whether a host COM port exists.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 *
 * @return true if it exists.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UART_HV_UART_PRESENT_01}: `hv_uart_present()` shall tell whether
 *   a serial port of the host exists.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_004}
 *
 * @llr
 * - \llrid{LLR_UART_HV_UART_PRESENT_01}: `hv_uart_present()` shall return true
 *   if `port_of()` returns a port for `port`, and false if it returns NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_PRESENT_01}
 *
 * @design
 * 1. Return `port_of(port) != NULL`. \llrref{LLR_UART_HV_UART_PRESENT_01}
 *
 * @calls
 * - `port_of()`
 */
bool hv_uart_present(uint16_t port)
{
    return port_of(port) != NULL;
}

/**
 * @funcname `hv_uart_is_pl011`
 *
 * @description
 * Tells whether a host COM port is a PL011 (one that can be passed through to a
 * guest).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 *
 * @return true for an existing PL011.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const host_port_t *` | The port, or NULL |
 *
 * @hlr
 * - \hlrid{HLR_UART_HV_UART_IS_PL011_01}: `hv_uart_is_pl011()` shall tell
 *   whether a serial port of the host is a PL011, which a guest can use
 *   directly.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 *
 * @llr
 * - \llrid{LLR_UART_HV_UART_IS_PL011_01}: If `port_of()` returns a port for
 *   `port` and `pl011` of that port is true, `hv_uart_is_pl011()` shall return
 *   true.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_IS_PL011_01}
 * - \llrid{LLR_UART_HV_UART_IS_PL011_02}: If `port_of()` returns NULL for
 *   `port`, or `pl011` of the port is false, `hv_uart_is_pl011()` shall return
 *   false.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_IS_PL011_01}
 *
 * @design
 * 1. Call `port_of()` with the argument `port`, and store the result in `p`.
 *    \llrref{LLR_UART_HV_UART_IS_PL011_01},
 *    \llrref{LLR_UART_HV_UART_IS_PL011_02}
 * 2. Return `(p != NULL) && p->pl011`. \llrref{LLR_UART_HV_UART_IS_PL011_01},
 *    \llrref{LLR_UART_HV_UART_IS_PL011_02}
 *
 * @calls
 * - `port_of()`
 */
bool hv_uart_is_pl011(uint16_t port)
{
    const host_port_t *p = port_of(port);

    return (p != NULL) && p->pl011;
}

/**
 * @funcname `hv_uart_base`
 *
 * @description
 * Returns the physical address of the registers of a host COM port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 *
 * @return The address, or 0 if there is no such port.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const host_port_t *` | The port, or NULL |
 *
 * @hlr
 * - \hlrid{HLR_UART_HV_UART_BASE_01}: `hv_uart_base()` shall give the address
 *   of the registers of a serial port of the host, or 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_MEM_003}
 *
 * @llr
 * - \llrid{LLR_UART_HV_UART_BASE_01}: If `port_of()` returns a port for `port`,
 *   `hv_uart_base()` shall return `base` of that port.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_BASE_01}
 * - \llrid{LLR_UART_HV_UART_BASE_02}: If `port_of()` returns NULL for `port`,
 *   `hv_uart_base()` shall return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_BASE_01}
 *
 * @design
 * 1. Call `port_of()` with the argument `port`, and store the result in `p`.
 *    \llrref{LLR_UART_HV_UART_BASE_01}, \llrref{LLR_UART_HV_UART_BASE_02}
 * 2. Return `p->base` if `p` is not NULL, otherwise `0U`.
 *    \llrref{LLR_UART_HV_UART_BASE_01}, \llrref{LLR_UART_HV_UART_BASE_02}
 *
 * @calls
 * - `port_of()`
 */
uint64_t hv_uart_base(uint16_t port)
{
    const host_port_t *p = port_of(port);

    return (p != NULL) ? p->base : 0U;
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
 * | `port` | `uint16_t` | [in] The port number, or HV_PORT_NONE. |
 *
 * @return "COM1" to "COM16", or "none".
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port_names` | `static const char *const[HV_NB_PORTS + 1U]` | The names, by number |
 *
 * @hlr
 * - \hlrid{HLR_UART_ARM64_HV_PORT_NAME_01}: `hv_port_name()` shall give the
 *   name of a serial port of the host for the messages: COM1 to COM16, or none.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_006}
 *   - \srqref{SRQ_HV_CFG_004}
 *
 * @llr
 * - \llrid{LLR_UART_ARM64_HV_PORT_NAME_01}: If `port` is `HV_NB_PORTS` or less,
 *   `hv_port_name()` shall return the name of the port: `"none"` for 0, and
 *   `"COM1"` to `"COM16"` for 1 to 16.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_ARM64_HV_PORT_NAME_01}
 * - \llrid{LLR_UART_ARM64_HV_PORT_NAME_02}: If `port` is more than
 *   `HV_NB_PORTS`, `hv_port_name()` shall return `"none"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_ARM64_HV_PORT_NAME_01}
 *
 * @design
 * 1. Return `port_names[port]` if `port` is less than or equal to
 *    `HV_NB_PORTS`, otherwise `port_names[0]`.
 *    \llrref{LLR_UART_ARM64_HV_PORT_NAME_01},
 *    \llrref{LLR_UART_ARM64_HV_PORT_NAME_02}
 *
 * @calls
 * None.
 */
const char *hv_port_name(uint16_t port)
{
    static const char *const port_names[HV_NB_PORTS + 1U] = {
        "none", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
        "COM8", "COM9", "COM10", "COM11", "COM12", "COM13", "COM14",
        "COM15", "COM16"
    };

    return (port <= HV_NB_PORTS) ? port_names[port] : port_names[0];
}

/**
 * @funcname `uart_init`
 *
 * @description
 * Sets up a host port for the hypervisor console: a 16550 gets 38400 baud, 8
 * data bits, no parity, one stop bit, FIFOs on, no interrupts; QEMU's PL011
 * needs nothing.
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
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const host_port_t *` | The port, or NULL |
 *
 * @hlr
 * - \hlrid{HLR_UART_ARM64_UART_INIT_01}: `uart_init()` shall set up a port of
 *   the host for the console of the hypervisor: a 16550 at 38400 baud, 8 data
 *   bits, no parity, one stop bit, FIFOs on and no interrupts; a PL011 as it
 *   is.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *
 * @llr
 * - \llrid{LLR_UART_ARM64_UART_INIT_01}: If `port_of()` returns a port for
 *   `port` and `pl011` of that port is false, `uart_init()` shall write with
 *   `reg_write8()`, at the base of the port plus the register, in this order:
 *   `0x00` to `UART_IER`, `0x80` to `UART_LCR`, `0x03` to `UART_DLL`, `0x00` to
 *   `UART_DLM`, `0x03` to `UART_LCR`, `0xC7` to `UART_FCR` and `0x03` to
 *   `UART_MCR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_ARM64_UART_INIT_01}
 * - \llrid{LLR_UART_ARM64_UART_INIT_02}: If `port_of()` returns NULL for
 *   `port`, or `pl011` of the port is true, `uart_init()` shall write no
 *   register.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_ARM64_UART_INIT_01}
 *
 * @design
 * 1. Call `port_of()` with the argument `port`, and store the result in `p`.
 *    \llrref{LLR_UART_ARM64_UART_INIT_01}, \llrref{LLR_UART_ARM64_UART_INIT_02}
 * 2. If `p` is not NULL and `p->pl011` is false:
 *    \llrref{LLR_UART_ARM64_UART_INIT_01}, \llrref{LLR_UART_ARM64_UART_INIT_02}
 *    1. Call `reg_write8()` with the arguments `p->base + UART_IER` and
 *       `0x00U`.
 *    2. Call `reg_write8()` with the arguments `p->base + UART_LCR` and
 *       `0x80U`.
 *    3. Call `reg_write8()` with the arguments `p->base + UART_DLL` and
 *       `0x03U`.
 *    4. Call `reg_write8()` with the arguments `p->base + UART_DLM` and
 *       `0x00U`.
 *    5. Call `reg_write8()` with the arguments `p->base + UART_LCR` and
 *       `0x03U`.
 *    6. Call `reg_write8()` with the arguments `p->base + UART_FCR` and
 *       `0xC7U`.
 *    7. Call `reg_write8()` with the arguments `p->base + UART_MCR` and
 *       `0x03U`.
 *
 * @calls
 * - `port_of()`
 * - `reg_write8()`
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

/**
 * @funcname `tx_ready`
 *
 * @description
 * Tells whether a host port can take a byte now.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const host_port_t *` | [in] The port. |
 *
 * @return true if there is room.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ready` | `bool` | Whether there is room |
 *
 * @hlr
 * - \hlrid{HLR_UART_TX_READY_01}: `tx_ready()` shall tell whether a port of the
 *   host can take a byte.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *
 * @llr
 * - \llrid{LLR_UART_TX_READY_01}: If `pl011` of the port is true, `tx_ready()`
 *   shall return true if `PL011_FR_TXFF` is not set in the 32-bit register
 *   (`reg_read32()`) at the base of the port plus `PL011_FR`, and false if it
 *   is set.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_TX_READY_01}
 * - \llrid{LLR_UART_TX_READY_02}: If `pl011` of the port is false, `tx_ready()`
 *   shall return true if `UART_LSR_THRE` is set in the 8-bit register
 *   (`reg_read8()`) at the base of the port plus `UART_LSR`, and false if it is
 *   not.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_TX_READY_01}
 *
 * @design
 * 1. If `p->pl011` is true: \llrref{LLR_UART_TX_READY_01}
 *    1. Set `ready` to
 *       `(reg_read32(p->base + PL011_FR) & PL011_FR_TXFF) == 0U`.
 * 2. Otherwise: \llrref{LLR_UART_TX_READY_02}
 *    1. Set `ready` to `(reg_read8(p->base + UART_LSR) & UART_LSR_THRE) != 0U`.
 * 3. Return `ready`. \llrref{LLR_UART_TX_READY_01},
 *    \llrref{LLR_UART_TX_READY_02}
 *
 * @calls
 * - `reg_read32()`
 * - `reg_read8()`
 */
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

/**
 * @funcname `hv_uart_write`
 *
 * @description
 * Writes a byte to a host port, waiting for room in it; without a port the byte
 * is dropped.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 * | `byte` | `uint8_t` | [in] The byte. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const host_port_t *` | The port, or NULL |
 *
 * @hlr
 * - \hlrid{HLR_UART_HV_UART_WRITE_01}: `hv_uart_write()` shall write a byte to
 *   a port of the host, waiting for room, and drop it without a port.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *   - \srqref{SRQ_HV_CON_007}
 *   - \srqref{SRQ_HV_CON_004}
 *
 * @llr
 * - \llrid{LLR_UART_HV_UART_WRITE_01}: If `port_of()` returns a port for
 *   `port`, `hv_uart_write()` shall call `arm64_yield()` while `tx_ready()`
 *   returns false for that port.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_WRITE_01}
 * - \llrid{LLR_UART_HV_UART_WRITE_02}: If `port_of()` returns a port whose
 *   `pl011` is true, `hv_uart_write()` shall, when `tx_ready()` returns true,
 *   write `byte` once with `reg_write32()` at the base of the port plus
 *   `PL011_DR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_WRITE_01}
 * - \llrid{LLR_UART_HV_UART_WRITE_03}: If `port_of()` returns a port whose
 *   `pl011` is false, `hv_uart_write()` shall, when `tx_ready()` returns true,
 *   write `byte` once with `reg_write8()` at the base of the port plus
 *   `UART_THR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_WRITE_01}
 * - \llrid{LLR_UART_HV_UART_WRITE_04}: If `port_of()` returns NULL for `port`,
 *   `hv_uart_write()` shall write no register.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_WRITE_01}
 *
 * @design
 * 1. Call `port_of()` with the argument `port`, and store the result in `p`.
 *    \llrref{LLR_UART_HV_UART_WRITE_01}, \llrref{LLR_UART_HV_UART_WRITE_02},
 *    \llrref{LLR_UART_HV_UART_WRITE_03}, \llrref{LLR_UART_HV_UART_WRITE_04}
 * 2. If `p` is not NULL: \llrref{LLR_UART_HV_UART_WRITE_01},
 *    \llrref{LLR_UART_HV_UART_WRITE_02}, \llrref{LLR_UART_HV_UART_WRITE_03},
 *    \llrref{LLR_UART_HV_UART_WRITE_04}
 *    1. While `tx_ready(p)` is false: \llrref{LLR_UART_HV_UART_WRITE_01}
 *       1. Call `arm64_yield()`.
 *    2. If `p->pl011` is true: \llrref{LLR_UART_HV_UART_WRITE_02}
 *       1. Call `reg_write32()` with the arguments `p->base + PL011_DR` and
 *          `byte`.
 *    3. Otherwise: \llrref{LLR_UART_HV_UART_WRITE_03}
 *       1. Call `reg_write8()` with the arguments `p->base + UART_THR` and
 *          `byte`.
 *
 * @calls
 * - `port_of()`
 * - `tx_ready()`
 * - `arm64_yield()`
 * - `reg_write32()`
 * - `reg_write8()`
 */
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

/**
 * @funcname `uart_putc`
 *
 * @description
 * Writes a character of the hypervisor's console.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 * | `c` | `char` | [in] The character. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_UART_ARM64_UART_PUTC_01}: `uart_putc()` shall write a character
 *   of the console of the hypervisor.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *
 * @llr
 * - \llrid{LLR_UART_ARM64_UART_PUTC_01}: `uart_putc()` shall call
 *   `hv_uart_write()` once with `port` and the character `c`.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_ARM64_UART_PUTC_01}
 *
 * @design
 * 1. Call `hv_uart_write()` with the arguments `port` and `(uint8_t) c`.
 *    \llrref{LLR_UART_ARM64_UART_PUTC_01}
 *
 * @calls
 * - `hv_uart_write()`
 */
void uart_putc(uint16_t port, char c)
{
    hv_uart_write(port, (uint8_t) c);
}

/**
 * @funcname `hv_uart_rx_ready`
 *
 * @description
 * Tells whether a host port has received a byte.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 *
 * @return true if a byte can be read (false without a port).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const host_port_t *` | The port, or NULL |
 * | `ready` | `bool` | Whether a byte was received |
 *
 * @hlr
 * - \hlrid{HLR_UART_HV_UART_RX_READY_01}: `hv_uart_rx_ready()` shall tell
 *   whether a port of the host has received a byte.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 *
 * @llr
 * - \llrid{LLR_UART_HV_UART_RX_READY_01}: If `port_of()` returns NULL for
 *   `port`, `hv_uart_rx_ready()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_RX_READY_01}
 * - \llrid{LLR_UART_HV_UART_RX_READY_02}: If `port_of()` returns a port whose
 *   `pl011` is true, `hv_uart_rx_ready()` shall return true if `PL011_FR_RXFE`
 *   is not set in the 32-bit register (`reg_read32()`) at the base of the port
 *   plus `PL011_FR`, and false if it is set.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_RX_READY_01}
 * - \llrid{LLR_UART_HV_UART_RX_READY_03}: If `port_of()` returns a port whose
 *   `pl011` is false, `hv_uart_rx_ready()` shall return true if `UART_LSR_DR`
 *   is set in the 8-bit register (`reg_read8()`) at the base of the port plus
 *   `UART_LSR`, and false if it is not.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_RX_READY_01}
 *
 * @design
 * 1. Call `port_of()` with the argument `port`, and store the result in `p`.
 *    \llrref{LLR_UART_HV_UART_RX_READY_01},
 *    \llrref{LLR_UART_HV_UART_RX_READY_02},
 *    \llrref{LLR_UART_HV_UART_RX_READY_03}
 * 2. Set `ready` to `false`. \llrref{LLR_UART_HV_UART_RX_READY_01}
 * 3. If `p` is NULL: \llrref{LLR_UART_HV_UART_RX_READY_01}
 * 4. Otherwise, if `p->pl011` is true: \llrref{LLR_UART_HV_UART_RX_READY_02}
 *    1. Set `ready` to
 *       `(reg_read32(p->base + PL011_FR) & PL011_FR_RXFE) == 0U`.
 * 5. Otherwise: \llrref{LLR_UART_HV_UART_RX_READY_03}
 *    1. Set `ready` to `(reg_read8(p->base + UART_LSR) & UART_LSR_DR) != 0U`.
 * 6. Return `ready`. \llrref{LLR_UART_HV_UART_RX_READY_01},
 *    \llrref{LLR_UART_HV_UART_RX_READY_02},
 *    \llrref{LLR_UART_HV_UART_RX_READY_03}
 *
 * @calls
 * - `port_of()`
 * - `reg_read32()`
 * - `reg_read8()`
 */
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

/**
 * @funcname `hv_uart_read`
 *
 * @description
 * Reads the next byte that a host port has received.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 *
 * @return The byte, or 0 if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const host_port_t *` | The port, or NULL |
 * | `byte` | `uint8_t` | The byte read |
 *
 * @hlr
 * - \hlrid{HLR_UART_HV_UART_READ_01}: `hv_uart_read()` shall read the next byte
 *   that a port of the host has received, or 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 *
 * @llr
 * - \llrid{LLR_UART_HV_UART_READ_01}: If `port_of()` returns a port for `port`,
 *   `hv_uart_rx_ready()` returns true for `port` and `pl011` of the port is
 *   true, `hv_uart_read()` shall return the low 8 bits of the 32-bit register
 *   (`reg_read32()`) at the base of the port plus `PL011_DR`, read once.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_READ_01}
 * - \llrid{LLR_UART_HV_UART_READ_02}: If `port_of()` returns a port for `port`,
 *   `hv_uart_rx_ready()` returns true for `port` and `pl011` of the port is
 *   false, `hv_uart_read()` shall return the 8-bit register (`reg_read8()`) at
 *   the base of the port plus `UART_RBR`, read once.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_READ_01}
 * - \llrid{LLR_UART_HV_UART_READ_03}: If `port_of()` returns NULL for `port`,
 *   or `hv_uart_rx_ready()` returns false for `port`, `hv_uart_read()` shall
 *   return 0, without reading a data register.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_READ_01}
 *
 * @design
 * 1. Call `port_of()` with the argument `port`, and store the result in `p`.
 *    \llrref{LLR_UART_HV_UART_READ_01}, \llrref{LLR_UART_HV_UART_READ_02},
 *    \llrref{LLR_UART_HV_UART_READ_03}
 * 2. Set `byte` to `0U`. \llrref{LLR_UART_HV_UART_READ_03}
 * 3. If `p` is not NULL and `hv_uart_rx_ready(port)` is true:
 *    \llrref{LLR_UART_HV_UART_READ_01}, \llrref{LLR_UART_HV_UART_READ_02},
 *    \llrref{LLR_UART_HV_UART_READ_03}
 *    1. If `p->pl011` is true: \llrref{LLR_UART_HV_UART_READ_01}
 *       1. Set `byte` to `(uint8_t) reg_read32(p->base + PL011_DR)`.
 *    2. Otherwise: \llrref{LLR_UART_HV_UART_READ_02}
 *       1. Call `reg_read8()` with the argument `p->base + UART_RBR`, and store
 *          the result in `byte`.
 * 4. Return `byte`. \llrref{LLR_UART_HV_UART_READ_01},
 *    \llrref{LLR_UART_HV_UART_READ_02}, \llrref{LLR_UART_HV_UART_READ_03}
 *
 * @calls
 * - `port_of()`
 * - `hv_uart_rx_ready()`
 * - `reg_read32()`
 * - `reg_read8()`
 */
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

/**
 * @funcname `hv_uart_tx_empty`
 *
 * @description
 * Tells whether everything written to a host port has been sent.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The port number. |
 *
 * @return true if the port is empty (true without a port).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const host_port_t *` | The port, or NULL |
 * | `empty` | `bool` | Whether everything was sent |
 *
 * @hlr
 * - \hlrid{HLR_UART_HV_UART_TX_EMPTY_01}: `hv_uart_tx_empty()` shall tell
 *   whether everything written to a port of the host has been sent.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 *
 * @llr
 * - \llrid{LLR_UART_HV_UART_TX_EMPTY_01}: If `port_of()` returns NULL for
 *   `port`, `hv_uart_tx_empty()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_TX_EMPTY_01}
 * - \llrid{LLR_UART_HV_UART_TX_EMPTY_02}: If `port_of()` returns a port whose
 *   `pl011` is true, `hv_uart_tx_empty()` shall return true if `PL011_FR_TXFE`
 *   is set in the 32-bit register (`reg_read32()`) at the base of the port plus
 *   `PL011_FR`, and false if it is not.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_TX_EMPTY_01}
 * - \llrid{LLR_UART_HV_UART_TX_EMPTY_03}: If `port_of()` returns a port whose
 *   `pl011` is false, `hv_uart_tx_empty()` shall return true if `UART_LSR_TEMT`
 *   is set in the 8-bit register (`reg_read8()`) at the base of the port plus
 *   `UART_LSR`, and false if it is not.
 *   <br>Traces to:
 *   - \hlrref{HLR_UART_HV_UART_TX_EMPTY_01}
 *
 * @design
 * 1. Call `port_of()` with the argument `port`, and store the result in `p`.
 *    \llrref{LLR_UART_HV_UART_TX_EMPTY_01},
 *    \llrref{LLR_UART_HV_UART_TX_EMPTY_02},
 *    \llrref{LLR_UART_HV_UART_TX_EMPTY_03}
 * 2. Set `empty` to `true`. \llrref{LLR_UART_HV_UART_TX_EMPTY_01}
 * 3. If `p` is NULL: \llrref{LLR_UART_HV_UART_TX_EMPTY_01}
 * 4. Otherwise, if `p->pl011` is true: \llrref{LLR_UART_HV_UART_TX_EMPTY_02}
 *    1. Set `empty` to
 *       `(reg_read32(p->base + PL011_FR) & PL011_FR_TXFE) != 0U`.
 * 5. Otherwise: \llrref{LLR_UART_HV_UART_TX_EMPTY_03}
 *    1. Set `empty` to `(reg_read8(p->base + UART_LSR) & UART_LSR_TEMT) != 0U`.
 * 6. Return `empty`. \llrref{LLR_UART_HV_UART_TX_EMPTY_01},
 *    \llrref{LLR_UART_HV_UART_TX_EMPTY_02},
 *    \llrref{LLR_UART_HV_UART_TX_EMPTY_03}
 *
 * @calls
 * - `port_of()`
 * - `reg_read32()`
 * - `reg_read8()`
 */
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
