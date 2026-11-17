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
 * arch/x86_64/. Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and external
 * declarations are in arch/internal.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/x86_64/vuart.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps).
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 */

/**
 * @file
 * Forwarding of a guest serial port to a host serial port at another
 * address: the access is repeated on the host port with the same offset and
 * size. The guests poll their UARTs, so no interrupts are routed. Each host
 * port belongs to one VM only (checked by hvconfig.py).
 */

#include <arch/internal.h>

/**
 * @funcname `vuart_access`
 *
 * @description
 * Repeats an access of a guest to its serial port on the host port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `host_base` | `uint16_t` | [in] The host port's I/O base address. |
 * | `offset` | `uint16_t` | [in] The register offset, 0-7. |
 * | `size` | `uint32_t` | [in] The access size: 1, 2 or 4 bytes. |
 * | `is_in` | `bool` | [in] true for a read. |
 * | `value` | `uint32_t *` | [in,out] The value written, or where the value read is stored. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | The host port's register |
 *
 * @hlr
 * - \hlrid{HLR_VUART_X86_64_VUART_ACCESS_01}: `vuart_access()` shall repeat an
 *   access of a guest to its serial port on the host port.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_004}
 *   - \srqref{SRQ_HV_CON_005}
 *
 * @llr
 * - \llrid{LLR_VUART_X86_64_VUART_ACCESS_01}: `vuart_access()` shall access the
 *   host port whose number is the sum of `host_base` and `offset`, truncated to
 *   16 bits.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_X86_64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_X86_64_VUART_ACCESS_02}: If `is_in` is true and `size` is
 *   1, `vuart_access()` shall read the port once with `x86_inb()` and store the
 *   byte read at `value`, with the upper 24 bits 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_X86_64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_X86_64_VUART_ACCESS_03}: If `is_in` is true and `size` is
 *   2, `vuart_access()` shall read the port once with `x86_inw()` and store the
 *   16 bits read at `value`, with the upper 16 bits 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_X86_64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_X86_64_VUART_ACCESS_04}: If `is_in` is true and `size` is
 *   neither 1 nor 2, `vuart_access()` shall read the port once with `x86_inl()`
 *   and store the 32 bits read at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_X86_64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_X86_64_VUART_ACCESS_05}: If `is_in` is false and `size` is
 *   1, `vuart_access()` shall write the low 8 bits of the value at `value` to
 *   the port once with `x86_outb()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_X86_64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_X86_64_VUART_ACCESS_06}: If `is_in` is false and `size` is
 *   2, `vuart_access()` shall write the low 16 bits of the value at `value` to
 *   the port once with `x86_outw()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_X86_64_VUART_ACCESS_01}
 * - \llrid{LLR_VUART_X86_64_VUART_ACCESS_07}: If `is_in` is false and `size` is
 *   neither 1 nor 2, `vuart_access()` shall write the 32 bits of the value at
 *   `value` to the port once with `x86_outl()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VUART_X86_64_VUART_ACCESS_01}
 *
 * @design
 * 1. Set `port` to `(uint16_t) (host_base + offset)`.
 *    \llrref{LLR_VUART_X86_64_VUART_ACCESS_01}
 * 2. If `is_in` is true: \llrref{LLR_VUART_X86_64_VUART_ACCESS_02},
 *    \llrref{LLR_VUART_X86_64_VUART_ACCESS_03},
 *    \llrref{LLR_VUART_X86_64_VUART_ACCESS_04}
 *    1. If `size` is equal to `1U`: \llrref{LLR_VUART_X86_64_VUART_ACCESS_02}
 *       1. Call `x86_inb()` with the argument `port`, and store the result in
 *          `*value`.
 *    2. Otherwise, if `size` is equal to `2U`:
 *       \llrref{LLR_VUART_X86_64_VUART_ACCESS_03}
 *       1. Call `x86_inw()` with the argument `port`, and store the result in
 *          `*value`.
 *    3. Otherwise: \llrref{LLR_VUART_X86_64_VUART_ACCESS_04}
 *       1. Call `x86_inl()` with the argument `port`, and store the result in
 *          `*value`.
 * 3. Otherwise: \llrref{LLR_VUART_X86_64_VUART_ACCESS_05},
 *    \llrref{LLR_VUART_X86_64_VUART_ACCESS_06},
 *    \llrref{LLR_VUART_X86_64_VUART_ACCESS_07}
 *    1. If `size` is equal to `1U`: \llrref{LLR_VUART_X86_64_VUART_ACCESS_05}
 *       1. Call `x86_outb()` with the arguments `port` and `(uint8_t) *value`.
 *    2. Otherwise, if `size` is equal to `2U`:
 *       \llrref{LLR_VUART_X86_64_VUART_ACCESS_06}
 *       1. Call `x86_outw()` with the arguments `port` and `(uint16_t) *value`.
 *    3. Otherwise: \llrref{LLR_VUART_X86_64_VUART_ACCESS_07}
 *       1. Call `x86_outl()` with the arguments `port` and the value at
 *          `value`.
 *
 * @calls
 * - `x86_inb()`
 * - `x86_inw()`
 * - `x86_inl()`
 * - `x86_outb()`
 * - `x86_outw()`
 * - `x86_outl()`
 */
void vuart_access(uint16_t host_base, uint16_t offset, uint32_t size,
                  bool is_in, uint32_t *value)
{
    uint16_t port = (uint16_t) (host_base + offset);

    if (is_in) {
        if (size == 1U) {
            *value = x86_inb(port);
        } else if (size == 2U) {
            *value = x86_inw(port);
        } else {
            *value = x86_inl(port);
        }
    } else {
        if (size == 1U) {
            x86_outb(port, (uint8_t) *value);
        } else if (size == 2U) {
            x86_outw(port, (uint16_t) *value);
        } else {
            x86_outl(port, *value);
        }
    }
}
