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
 * Forwarding of a guest serial port to a host serial port at another
 * address: the access is repeated on the host port with the same offset
 * and size. The guests poll their UARTs, so no interrupts are routed.
 * Each host port belongs to one VM only (checked by hvconfig.py).
 */

#include <hv/hv.h>
#include <hv/x86.h>

/*
 * Repeat an access of a guest to its serial port (offset, size, direction) on
 * the host port host_base.
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
