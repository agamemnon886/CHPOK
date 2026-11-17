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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
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
 * - \hlrid{HLR_VUART_VUART_RESET_01}: `vuart_reset()` shall reset an emulated
 *   PL011 of a VM as at power-on, keeping its port of the host.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_VM_004}
 *
 * @llr
 * - \llrid{LLR_VUART_VUART_RESET_01}: `vuart_reset()` shall set `cr` of the
 *   UART to `UART_CR_RESET`, and `lcr_h`, `ibrd`, `fbrd`, `ifls` and `imsc`
 *   to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_VUART_RESET_01}
 *
 * @design
 * 1. Set `uart->cr` to `UART_CR_RESET`. \llrref{LLR_VUART_VUART_RESET_01}
 * 2. Set `uart->lcr_h` to `0U`. \llrref{LLR_VUART_VUART_RESET_01}
 * 3. Set `uart->ibrd` to `0U`. \llrref{LLR_VUART_VUART_RESET_01}
 * 4. Set `uart->fbrd` to `0U`. \llrref{LLR_VUART_VUART_RESET_01}
 * 5. Set `uart->ifls` to `0U`. \llrref{LLR_VUART_VUART_RESET_01}
 * 6. Set `uart->imsc` to `0U`. \llrref{LLR_VUART_VUART_RESET_01}
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
 * - \hlrid{HLR_VUART_ID_REGISTER_01}: `id_register()` shall give the
 *   identification registers of an emulated PrimeCell device.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_003}
 *
 * @llr
 * - \llrid{LLR_VUART_ID_REGISTER_01}: If `offset` is `ID_FIRST` or more, is
 *   less than `ID_FIRST` plus 4 times `ID_COUNT` and is a multiple of 4,
 *   `id_register()` shall store `ids[(offset - ID_FIRST) / 4]` at `value` and
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ID_REGISTER_01}
 * - \llrid{LLR_VUART_ID_REGISTER_02}: If `offset` is less than `ID_FIRST`, or
 *   is not less than `ID_FIRST` plus 4 times `ID_COUNT`, or is not a multiple
 *   of 4, `id_register()` shall return false and leave the value at `value` as
 *   it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ID_REGISTER_01}
 *
 * @design
 * 1. Set `is_id` to
 *    `(offset >= ID_FIRST) && (offset < (ID_FIRST + (4U * ID_COUNT))) &&`
 *    `((offset & 3U) == 0U)`. \llrref{LLR_VUART_ID_REGISTER_01},
 *    \llrref{LLR_VUART_ID_REGISTER_02}
 * 2. If `is_id` is true: \llrref{LLR_VUART_ID_REGISTER_01},
 *    \llrref{LLR_VUART_ID_REGISTER_02}
 *    1. Set `*value` to `ids[(offset - ID_FIRST) / 4U]`.
 * 3. Return `is_id`. \llrref{LLR_VUART_ID_REGISTER_01},
 *    \llrref{LLR_VUART_ID_REGISTER_02}
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
 * - \hlrid{HLR_VUART_STORED_REGISTER_01}: `stored_register()` shall give the
 *   stored registers of an emulated PL011: baud rate, line control, control,
 *   FIFO level and interrupt mask.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_003}
 *
 * @llr
 * - \llrid{LLR_VUART_STORED_REGISTER_01}: If `offset` is `UART_IBRD`,
 *   `UART_FBRD`, `UART_LCR_H`, `UART_CR`, `UART_IFLS` or `UART_IMSC`,
 *   `stored_register()` shall return the address of `ibrd`, `fbrd`, `lcr_h`,
 *   `cr`, `ifls` or `imsc` of the UART, in this order.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_STORED_REGISTER_01}
 * - \llrid{LLR_VUART_STORED_REGISTER_02}: If `offset` is none of `UART_IBRD`,
 *   `UART_FBRD`, `UART_LCR_H`, `UART_CR`, `UART_IFLS` and `UART_IMSC`,
 *   `stored_register()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_STORED_REGISTER_01}
 *
 * @design
 * 1. Depending on `offset`: \llrref{LLR_VUART_STORED_REGISTER_01},
 *    \llrref{LLR_VUART_STORED_REGISTER_02}
 *    1. Case `UART_IBRD`: \llrref{LLR_VUART_STORED_REGISTER_01}
 *       1. Set `reg` to the address of `uart->ibrd`.
 *       2. Leave the switch.
 *    2. Case `UART_FBRD`: \llrref{LLR_VUART_STORED_REGISTER_01}
 *       1. Set `reg` to the address of `uart->fbrd`.
 *       2. Leave the switch.
 *    3. Case `UART_LCR_H`: \llrref{LLR_VUART_STORED_REGISTER_01}
 *       1. Set `reg` to the address of `uart->lcr_h`.
 *       2. Leave the switch.
 *    4. Case `UART_CR`: \llrref{LLR_VUART_STORED_REGISTER_01}
 *       1. Set `reg` to the address of `uart->cr`.
 *       2. Leave the switch.
 *    5. Case `UART_IFLS`: \llrref{LLR_VUART_STORED_REGISTER_01}
 *       1. Set `reg` to the address of `uart->ifls`.
 *       2. Leave the switch.
 *    6. Case `UART_IMSC`: \llrref{LLR_VUART_STORED_REGISTER_01}
 *       1. Set `reg` to the address of `uart->imsc`.
 *       2. Leave the switch.
 *    7. Otherwise (default): \llrref{LLR_VUART_STORED_REGISTER_02}
 *       1. Set `reg` to `NULL`.
 *       2. Leave the switch.
 * 2. Return `reg`. \llrref{LLR_VUART_STORED_REGISTER_01},
 *    \llrref{LLR_VUART_STORED_REGISTER_02}
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
 * - \hlrid{HLR_VUART_ARM64_VUART_ACCESS_01}: `vuart_access()` shall emulate an
 *   access of a guest to one of its PL011s by repeating the data and the status
 *   on its port of the host.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 * - \hlrid{HLR_VUART_ARM64_VUART_ACCESS_02}: `vuart_access()` shall tell
 *   whether the register is emulated.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_004}
 *   - \srqref{SRQ_HV_DEV_007}
 * - \hlrid{HLR_VUART_ARM64_VUART_ACCESS_03}: `vuart_access()` shall give zero
 *   for a read of a register other than the data, flag, stored and
 *   identification registers.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_007}
 *
 * @llr
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_01}: If `offset` is `PL011_DR` and
 *   `write` is true, `vuart_access()` shall call `hv_uart_write()` once with
 *   the host port of the UART and the low 8 bits of the value at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_02}: If `offset` is `PL011_DR` and
 *   `write` is false, `vuart_access()` shall store the result of
 *   `hv_uart_read()` for the host port of the UART at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_03}: If `offset` is `PL011_FR` and
 *   `write` is false, `vuart_access()` shall store at `value` a value in which
 *   `PL011_FR_RXFE` is set if and only if `hv_uart_rx_ready()` returns false
 *   for the host port, `PL011_FR_TXFE` is set if and only if
 *   `hv_uart_tx_empty()` returns true for the host port, and the other bits
 *   are 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_04}: If `offset` is neither `PL011_DR`
 *   nor `PL011_FR`, `stored_register()` returns a register for it and `write`
 *   is true, `vuart_access()` shall store the low 16 bits of the value at
 *   `value` in that register.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_05}: If `offset` is neither `PL011_DR`
 *   nor `PL011_FR`, `stored_register()` returns a register for it and `write`
 *   is false, `vuart_access()` shall store the value of that register at
 *   `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_06}: If `offset` is neither `PL011_DR`
 *   nor `PL011_FR`, `stored_register()` returns NULL for it, `write` is false
 *   and `id_register()` returns true for `pl011_id`, `offset` and `value`,
 *   `vuart_access()` shall give the value that `id_register()` stores at
 *   `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_07}: If `offset` is neither `PL011_DR`
 *   nor `PL011_FR`, `stored_register()` returns NULL for it, `write` is false
 *   and `id_register()` returns false, `vuart_access()` shall store 0 at
 *   `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_03}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_08}: If `write` is true and `offset` is
 *   `PL011_FR`, or `write` is true, `offset` is neither `PL011_DR` nor
 *   `PL011_FR` and `stored_register()` returns NULL for it, `vuart_access()`
 *   shall leave the UART and the value at `value` as they are, without
 *   accessing the host port.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_01}
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_02}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_09}: If `offset` is `PL011_DR` or
 *   `PL011_FR`, or `stored_register()` returns a register for it, or `write` is
 *   false and `id_register()` returns true, `vuart_access()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_02}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_10}: If `offset` is `UART_RSR`,
 *   `UART_ILPR`, `UART_RIS`, `UART_MIS`, `UART_ICR` or `UART_DMACR`, and
 *   `write` is true or `id_register()` returns false, `vuart_access()` shall
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_02}
 * - \llrid{LLR_VUART_ARM64_VUART_ACCESS_11}: If `offset` is neither `PL011_DR`
 *   nor `PL011_FR`, `stored_register()` returns NULL for it, `write` is true or
 *   `id_register()` returns false, and `offset` is none of `UART_RSR`,
 *   `UART_ILPR`, `UART_RIS`, `UART_MIS`, `UART_ICR` and `UART_DMACR`,
 *   `vuart_access()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_ARM64_VUART_ACCESS_02}
 *
 * @design
 * 1. Call `stored_register()` with the arguments `uart` and `offset`, and store
 *    the result in `reg`. \llrref{LLR_VUART_ARM64_VUART_ACCESS_04},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_05},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_06},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_07},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_08},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_09},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_10},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_11}
 * 2. Set `known` to `true`. \llrref{LLR_VUART_ARM64_VUART_ACCESS_09}
 * 3. If `offset` is equal to `PL011_DR`:
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_01},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_02},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_09}
 *    1. If `write` is true: \llrref{LLR_VUART_ARM64_VUART_ACCESS_01}
 *       1. Call `hv_uart_write()` with the arguments `uart->host_port` and
 *          `(uint8_t) *value`.
 *    2. Otherwise: \llrref{LLR_VUART_ARM64_VUART_ACCESS_02}
 *       1. Call `hv_uart_read()` with the argument `uart->host_port`, and store
 *          the result in `*value`.
 * 4. Otherwise, if `offset` is equal to `PL011_FR`:
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_03},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_08},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_09}
 *    1. If `write` is false: \llrref{LLR_VUART_ARM64_VUART_ACCESS_03},
 *       \llrref{LLR_VUART_ARM64_VUART_ACCESS_08}
 *       1. Set `*value` to
 *          `(hv_uart_rx_ready(uart->host_port) ? 0U : PL011_FR_RXFE) |`
 *          `(hv_uart_tx_empty(uart->host_port) ? PL011_FR_TXFE : 0U)`.
 * 5. Otherwise, if `reg` is not NULL: \llrref{LLR_VUART_ARM64_VUART_ACCESS_04},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_05},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_09}
 *    1. If `write` is true: \llrref{LLR_VUART_ARM64_VUART_ACCESS_04}
 *       1. Set `*reg` to `(uint32_t) *value & 0xFFFFU`.
 *    2. Otherwise: \llrref{LLR_VUART_ARM64_VUART_ACCESS_05}
 *       1. Set `*value` to the value at `reg`.
 * 6. Otherwise, if `write` is false and `id_register(pl011_id, offset, value)`
 *    is true: \llrref{LLR_VUART_ARM64_VUART_ACCESS_06},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_09}
 * 7. Otherwise: \llrref{LLR_VUART_ARM64_VUART_ACCESS_07},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_08},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_10},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_11}
 *    1. If `write` is false: \llrref{LLR_VUART_ARM64_VUART_ACCESS_07},
 *       \llrref{LLR_VUART_ARM64_VUART_ACCESS_08}
 *       1. Set `*value` to `0U`.
 *    2. Set `known` to
 *       `(offset == UART_RSR) || (offset == UART_ILPR) || (offset == UART_RIS)`
 *       `|| (offset == UART_MIS) || (offset == UART_ICR) || (offset ==`
 *       `UART_DMACR)`. \llrref{LLR_VUART_ARM64_VUART_ACCESS_10},
 *       \llrref{LLR_VUART_ARM64_VUART_ACCESS_11}
 * 8. Return `known`. \llrref{LLR_VUART_ARM64_VUART_ACCESS_09},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_10},
 *    \llrref{LLR_VUART_ARM64_VUART_ACCESS_11}
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
 * - \hlrid{HLR_VUART_VRTC_ACCESS_01}: `vrtc_access()` shall emulate the PL031
 *   real-time clock of a guest with the time of the host.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_004}
 * - \hlrid{HLR_VUART_VRTC_ACCESS_02}: `vrtc_access()` shall tell whether the
 *   register is emulated.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_004}
 *   - \srqref{SRQ_HV_DEV_007}
 * - \hlrid{HLR_VUART_VRTC_ACCESS_03}: `vrtc_access()` shall give zero for a
 *   read of a register other than the data and identification registers.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_007}
 *
 * @llr
 * - \llrid{LLR_VUART_VRTC_ACCESS_01}: If `offset` is `RTC_DR` and `write` is
 *   false, `vrtc_access()` shall store at `value` the 32-bit register at
 *   `HV_HOST_RTC` plus `RTC_DR`, read once.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_VRTC_ACCESS_01}
 * - \llrid{LLR_VUART_VRTC_ACCESS_02}: If `offset` is not `RTC_DR`, `write` is
 *   false and `id_register()` returns true for `pl031_id`, `offset` and
 *   `value`, `vrtc_access()` shall give the value that `id_register()` stores
 *   at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_VRTC_ACCESS_01}
 * - \llrid{LLR_VUART_VRTC_ACCESS_03}: If `offset` is not `RTC_DR`, `write` is
 *   false and `id_register()` returns false, `vrtc_access()` shall store 0 at
 *   `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_VRTC_ACCESS_03}
 * - \llrid{LLR_VUART_VRTC_ACCESS_04}: If `write` is true, `vrtc_access()` shall
 *   leave the value at `value` as it is, without reading the register of the
 *   host.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_VRTC_ACCESS_02}
 * - \llrid{LLR_VUART_VRTC_ACCESS_05}: If `offset` is `RTC_DR`, or `write` is
 *   false and `id_register()` returns true, `vrtc_access()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_VRTC_ACCESS_01}
 *   - \hlrref{HLR_VUART_VRTC_ACCESS_02}
 * - \llrid{LLR_VUART_VRTC_ACCESS_06}: If `offset` is not `RTC_DR`, and `write`
 *   is true or `id_register()` returns false, `vrtc_access()` shall return
 *   false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_VRTC_ACCESS_02}
 *
 * @design
 * 1. Set `known` to `true`. \llrref{LLR_VUART_VRTC_ACCESS_05}
 * 2. If `offset` is equal to `RTC_DR`: \llrref{LLR_VUART_VRTC_ACCESS_01},
 *    \llrref{LLR_VUART_VRTC_ACCESS_04}, \llrref{LLR_VUART_VRTC_ACCESS_05}
 *    1. If `write` is false: \llrref{LLR_VUART_VRTC_ACCESS_01},
 *       \llrref{LLR_VUART_VRTC_ACCESS_04}
 *       1. Set `*value` to the value at
 *          `(const volatile uint32_t *) (uintptr_t) (HV_HOST_RTC + RTC_DR)`.
 * 3. Otherwise, if `write` is false and `id_register(pl031_id, offset, value)`
 *    is true: \llrref{LLR_VUART_VRTC_ACCESS_02},
 *    \llrref{LLR_VUART_VRTC_ACCESS_05}
 * 4. Otherwise: \llrref{LLR_VUART_VRTC_ACCESS_03},
 *    \llrref{LLR_VUART_VRTC_ACCESS_04}, \llrref{LLR_VUART_VRTC_ACCESS_06}
 *    1. If `write` is false: \llrref{LLR_VUART_VRTC_ACCESS_03},
 *       \llrref{LLR_VUART_VRTC_ACCESS_04}
 *       1. Set `*value` to `0U`.
 *    2. Set `known` to `false`. \llrref{LLR_VUART_VRTC_ACCESS_06}
 * 5. Return `known`. \llrref{LLR_VUART_VRTC_ACCESS_05},
 *    \llrref{LLR_VUART_VRTC_ACCESS_06}
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
