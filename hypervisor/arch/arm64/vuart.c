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
 * The guest's PL011s, when they are not passed through, and its PL031,
 * emulated (each access exits, mmio.c).
 *
 * PL011: the data register goes to and comes from the VM's host port (a
 * 16550 on the PCI bus); the flag register gives the host port's state
 * (transmit FIFO never full: a write waits for room in the host port).
 * The control registers are stored, the interrupt status reads as 0 and
 * the ID registers give a PL011's. A UART without a host port drops what
 * is written and receives nothing.
 *
 * PL031: the data register gives the host RTC's time; the rest reads as 0
 * and ignores writes, except the ID registers.
 */

#include <arch/internal.h>

// PL011 registers.
#define UART_DR             0x000U
#define UART_RSR            0x004U
#define UART_FR             0x018U
#define UART_ILPR           0x020U
#define UART_IBRD           0x024U
#define UART_FBRD           0x028U
#define UART_LCR_H          0x02CU
#define UART_CR             0x030U
#define UART_IFLS           0x034U
#define UART_IMSC           0x038U
#define UART_RIS            0x03CU
#define UART_MIS            0x040U
#define UART_ICR            0x044U
#define UART_DMACR          0x048U
#define UART_FR_RXFE        (1U << 4)
#define UART_FR_TXFE        (1U << 7)
#define UART_CR_RESET       0x300U      // TXE, RXE

// PL031 registers.
#define RTC_DR              0x000U

// PrimeCell identification registers (0xFE0-0xFFC).
#define ID_FIRST            0xFE0U
#define ID_COUNT            8U

// The UART as at power-on (its host port stays).
void vuart_reset(hv_vuart_t *uart)
{
    uart->cr = UART_CR_RESET;
    uart->lcr_h = 0U;
    uart->ibrd = 0U;
    uart->fbrd = 0U;
    uart->ifls = 0U;
    uart->imsc = 0U;
}

// The identification register at offset, of a PrimeCell with these IDs.
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

// A stored register of the UART, or NULL.
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

/*
 * An access of the guest to one of its PL011s at offset; value is read or
 * written. Returns false if the register isn't emulated (it reads as 0).
 */
bool vuart_access(hv_vuart_t *uart, uint32_t offset, bool write,
                  uint64_t *value)
{
    static const uint8_t pl011_id[ID_COUNT] = {
        0x11U, 0x10U, 0x14U, 0x00U, 0x0DU, 0xF0U, 0x05U, 0xB1U
    };
    uint32_t *reg = stored_register(uart, offset);
    bool known = true;

    if (offset == UART_DR) {
        if (write) {
            hv_uart_write(uart->host_port, (uint8_t) *value);
        } else {
            *value = hv_uart_read(uart->host_port);
        }
    } else if (offset == UART_FR) {
        if (!write) {
            *value = (hv_uart_rx_ready(uart->host_port) ? 0U : UART_FR_RXFE)
                     | (hv_uart_tx_empty(uart->host_port) ? UART_FR_TXFE
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

/*
 * An access of the guest to its PL031 at offset; value is read or
 * written. Returns false if the register isn't emulated (it reads as 0).
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
