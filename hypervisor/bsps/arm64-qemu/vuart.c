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
 * hypervisor/arch/arm64/vuart.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps); the PL011 registers have
 * their shared names (PL011_DR, PL011_FR and the bits of PL011_FR).
 */

/**
 * @file
 * The guest's PL011s, when they are not passed through, and its PL031,
 * emulated (each access exits, mmio.c).
 *
 * PL011: the data register goes to and comes from the VM's host port (a
 * 16550 on the PCI bus); the flag register gives the host port's state
 * (transmit FIFO never full: a write waits for room in the host port). The
 * control registers are stored, the interrupt status reads as 0 and the ID
 * registers give a PL011's. A UART without a host port drops what is written
 * and receives nothing.
 *
 * PL031: the data register gives the host RTC's time; the rest reads as 0
 * and ignores writes, except the ID registers.
 */

#include <arch/internal.h>

/**
 * @funcname `vuart_reset`
 *
 * @description
 * Resets an emulated PL011 of a VM as at power-on; its host port stays.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uart` | `hv_vuart_t *` | [out] The emulated UART. |
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
 * 1. Set `uart->cr` to `UART_CR_RESET`.
 * 2. Set `uart->lcr_h` to `0U`.
 * 3. Set `uart->ibrd` to `0U`.
 * 4. Set `uart->fbrd` to `0U`.
 * 5. Set `uart->ifls` to `0U`.
 * 6. Set `uart->imsc` to `0U`.
 *
 * @calls
 * None.
 */
void vuart_reset(hv_vuart_t *uart)
{
    uart->cr = UART_CR_RESET;
    uart->lcr_h = 0U;
    uart->ibrd = 0U;
    uart->fbrd = 0U;
    uart->ifls = 0U;
    uart->imsc = 0U;
}

/**
 * @funcname `id_register`
 *
 * @description
 * Reads a PrimeCell identification register (0xFE0-0xFFC) of an emulated
 * device.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ids` | `const uint8_t[ID_COUNT]` | [in] The device's eight ID bytes. |
 * | `offset` | `uint32_t` | [in] The register offset. |
 * | `value` | `uint64_t *` | [out] Where the value is stored. |
 *
 * @return true if the offset is an ID register.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `is_id` | `bool` | Whether the offset is an ID register |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `is_id` to
 *    `(offset >= ID_FIRST) && (offset < (ID_FIRST + (4U * ID_COUNT))) &&`
 *    `((offset & 3U) == 0U)`.
 * 2. If `is_id` is true:
 *    1. Set `*value` to `ids[(offset - ID_FIRST) / 4U]`.
 * 3. Return `is_id`.
 *
 * @calls
 * None.
 */
static bool id_register(const uint8_t ids[ID_COUNT], uint32_t offset,
                        uint64_t *value)
{
    bool is_id = (offset >= ID_FIRST)
                 && (offset < (ID_FIRST + (4U * ID_COUNT)))
                 && ((offset & 3U) == 0U);

    if (is_id) {
        *value = ids[(offset - ID_FIRST) / 4U];
    }
    return is_id;
}

/**
 * @funcname `stored_register`
 *
 * @description
 * Returns the stored register of an emulated PL011 at an offset (baud rate,
 * line control, control, FIFO level, interrupt mask).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uart` | `hv_vuart_t *` | [in] The emulated UART. |
 * | `offset` | `uint32_t` | [in] The register offset. |
 *
 * @return The register, or NULL for the others.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `reg` | `uint32_t *` | The register, or NULL |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Depending on `offset`:
 *    1. Case `UART_IBRD`:
 *       1. Set `reg` to the address of `uart->ibrd`.
 *       2. Leave the switch.
 *    2. Case `UART_FBRD`:
 *       1. Set `reg` to the address of `uart->fbrd`.
 *       2. Leave the switch.
 *    3. Case `UART_LCR_H`:
 *       1. Set `reg` to the address of `uart->lcr_h`.
 *       2. Leave the switch.
 *    4. Case `UART_CR`:
 *       1. Set `reg` to the address of `uart->cr`.
 *       2. Leave the switch.
 *    5. Case `UART_IFLS`:
 *       1. Set `reg` to the address of `uart->ifls`.
 *       2. Leave the switch.
 *    6. Case `UART_IMSC`:
 *       1. Set `reg` to the address of `uart->imsc`.
 *       2. Leave the switch.
 *    7. Otherwise (default):
 *       1. Set `reg` to `NULL`.
 *       2. Leave the switch.
 * 2. Return `reg`.
 *
 * @calls
 * None.
 */
static uint32_t *stored_register(hv_vuart_t *uart, uint32_t offset)
{
    uint32_t *reg;

    switch (offset) {
        case UART_IBRD:
            reg = &uart->ibrd;
            break;
        case UART_FBRD:
            reg = &uart->fbrd;
            break;
        case UART_LCR_H:
            reg = &uart->lcr_h;
            break;
        case UART_CR:
            reg = &uart->cr;
            break;
        case UART_IFLS:
            reg = &uart->ifls;
            break;
        case UART_IMSC:
            reg = &uart->imsc;
            break;
        default:
            reg = NULL;
            break;
    }
    return reg;
}

/**
 * @funcname `vuart_access`
 *
 * @description
 * Performs an access of the guest to one of its emulated PL011s (an MMIO exit).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `uart` | `hv_vuart_t *` | [in] The emulated UART. |
 * | `offset` | `uint32_t` | [in] The register offset. |
 * | `write` | `bool` | [in] true for a write. |
 * | `value` | `uint64_t *` | [in,out] The value written, or where the value read is stored. |
 *
 * @return false if the register isn't emulated (a read gives 0).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pl011_id` | `static const uint8_t[ID_COUNT]` | A PL011's ID bytes |
 * | `reg` | `uint32_t *` | The stored register, or NULL |
 * | `known` | `bool` | Whether the register is emulated |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `stored_register()` with the arguments `uart` and `offset`, and store
 *    the result in `reg`.
 * 2. Set `known` to `true`.
 * 3. If `offset` is equal to `PL011_DR`:
 *    1. If `write` is true:
 *       1. Call `hv_uart_write()` with the arguments `uart->host_port` and
 *          `(uint8_t) *value`.
 *    2. Otherwise:
 *       1. Call `hv_uart_read()` with the argument `uart->host_port`, and store
 *          the result in `*value`.
 * 4. Otherwise, if `offset` is equal to `PL011_FR`:
 *    1. If `write` is false:
 *       1. Set `*value` to
 *          `(hv_uart_rx_ready(uart->host_port) ? 0U : PL011_FR_RXFE) |`
 *          `(hv_uart_tx_empty(uart->host_port) ? PL011_FR_TXFE : 0U)`.
 * 5. Otherwise, if `reg` is not NULL:
 *    1. If `write` is true:
 *       1. Set `*reg` to `(uint32_t) *value & 0xFFFFU`.
 *    2. Otherwise:
 *       1. Set `*value` to the value at `reg`.
 * 6. Otherwise, if `write` is false and `id_register(pl011_id, offset, value)`
 *    is true:
 *    1. Do nothing.
 * 7. Otherwise:
 *    1. If `write` is false:
 *       1. Set `*value` to `0U`.
 *    2. Set `known` to
 *       `(offset == UART_RSR) || (offset == UART_ILPR) || (offset == UART_RIS)`
 *       `|| (offset == UART_MIS) || (offset == UART_ICR) || (offset ==`
 *       `UART_DMACR)`.
 * 8. Return `known`.
 *
 * @calls
 * - `stored_register()`
 * - `hv_uart_write()`
 * - `hv_uart_read()`
 * - `hv_uart_rx_ready()`
 * - `hv_uart_tx_empty()`
 * - `id_register()`
 */
bool vuart_access(hv_vuart_t *uart, uint32_t offset, bool write,
                  uint64_t *value)
{
    static const uint8_t pl011_id[ID_COUNT] = {
        0x11U, 0x10U, 0x14U, 0x00U, 0x0DU, 0xF0U, 0x05U, 0xB1U
    };
    uint32_t *reg = stored_register(uart, offset);
    bool known = true;

    if (offset == PL011_DR) {
        if (write) {
            hv_uart_write(uart->host_port, (uint8_t) *value);
        } else {
            *value = hv_uart_read(uart->host_port);
        }
    } else if (offset == PL011_FR) {
        if (!write) {
            *value = (hv_uart_rx_ready(uart->host_port) ? 0U : PL011_FR_RXFE)
                     | (hv_uart_tx_empty(uart->host_port) ? PL011_FR_TXFE
                                                           : 0U);
        }
    } else if (reg != NULL) {
        if (write) {
            *reg = (uint32_t) *value & 0xFFFFU;
        } else {
            *value = *reg;
        }
    } else if (!write && id_register(pl011_id, offset, value)) {
        // An ID register.
    } else {
        // Read as 0 (error and interrupt status), writes ignored.
        if (!write) {
            *value = 0U;
        }
        known = (offset == UART_RSR) || (offset == UART_ILPR)
                || (offset == UART_RIS) || (offset == UART_MIS)
                || (offset == UART_ICR) || (offset == UART_DMACR);
    }
    return known;
}

/**
 * @funcname `vrtc_access`
 *
 * @description
 * Performs an access of the guest to its emulated PL031 RTC (an MMIO exit).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `offset` | `uint32_t` | [in] The register offset. |
 * | `write` | `bool` | [in] true for a write. |
 * | `value` | `uint64_t *` | [out] The value written, or where the value read is stored. |
 *
 * @return false if the register isn't emulated (a read gives 0).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pl031_id` | `static const uint8_t[ID_COUNT]` | A PL031's ID bytes |
 * | `known` | `bool` | Whether the register is emulated |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `known` to `true`.
 * 2. If `offset` is equal to `RTC_DR`:
 *    1. If `write` is false:
 *       1. Set `*value` to the value at
 *          `(const volatile uint32_t *) (uintptr_t) (HV_HOST_RTC + RTC_DR)`.
 * 3. Otherwise, if `write` is false and `id_register(pl031_id, offset, value)`
 *    is true:
 *    1. Do nothing.
 * 4. Otherwise:
 *    1. If `write` is false:
 *       1. Set `*value` to `0U`.
 *    2. Set `known` to `false`.
 * 5. Return `known`.
 *
 * @calls
 * - `id_register()`
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (vrtc_access()): the host RTC's data
 * register is read by its physical address (identity-mapped), converted
 * to a pointer.
 */
bool vrtc_access(uint32_t offset, bool write, uint64_t *value)
{
    static const uint8_t pl031_id[ID_COUNT] = {
        0x31U, 0x10U, 0x04U, 0x00U, 0x0DU, 0xF0U, 0x05U, 0xB1U
    };
    bool known = true;

    if (offset == RTC_DR) {
        if (!write) {
            *value = *(const volatile uint32_t *) (uintptr_t)
                     (HV_HOST_RTC + RTC_DR);
        }
    } else if (!write && id_register(pl031_id, offset, value)) {
        // An ID register.
    } else {
        if (!write) {
            *value = 0U;
        }
        known = false;
    }
    return known;
}
