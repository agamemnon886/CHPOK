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
 * | `value` | `uint32_t *` | [out] The value written, or where the value read is stored. |
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
 *
 * @llr
 *
 * @design
 * 1. Set `port` to `(uint16_t) (host_base + offset)`.
 * 2. If `is_in` is true:
 *    1. If `size` is equal to `1U`:
 *       1. Call `x86_inb()` with the argument `port`, and store the result in
 *          `*value`.
 *    2. Otherwise, if `size` is equal to `2U`:
 *       1. Call `x86_inw()` with the argument `port`, and store the result in
 *          `*value`.
 *    3. Otherwise:
 *       1. Call `x86_inl()` with the argument `port`, and store the result in
 *          `*value`.
 * 3. Otherwise:
 *    1. If `size` is equal to `1U`:
 *       1. Call `x86_outb()` with the arguments `port` and `(uint8_t) *value`.
 *    2. Otherwise, if `size` is equal to `2U`:
 *       1. Call `x86_outw()` with the arguments `port` and `(uint16_t) *value`.
 *    3. Otherwise:
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
