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
 * Memory functions of the hypervisor, and memset()/memcpy(), which GCC
 * may call for structure assignments and initializations.
 */

#include <hv/hv.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the functions take their
 * memory as pointers to void, as memset() and memcpy() do, and access it
 * as bytes.
 */

// Set size bytes at dst to value.
void hv_memset(void *dst, uint8_t value, size_t size)
{
    uint8_t *d = dst;

    for (size_t i = 0U; i < size; i++) {
        d[i] = value;
    }
}

// Copy size bytes from src to dst (they must not overlap).
void hv_memcpy(void *dst, const void *src, size_t size)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    for (size_t i = 0U; i < size; i++) {
        d[i] = s[i];
    }
}

/*
 * MISRA C:2025 Rule 5.10 deviation: GCC may emit calls to memset() and
 * memcpy() (C standard semantics) even in freestanding code, so the
 * hypervisor, which has no C library, defines these reserved names.
 */
void *memset(void *dst, int value, size_t size);
void *memcpy(void *dst, const void *src, size_t size);

// C standard memset(), for the calls GCC generates.
void *memset(void *dst, int value, size_t size)
{
    hv_memset(dst, (uint8_t) value, size);
    return dst;
}

// C standard memcpy(), for the calls GCC generates.
void *memcpy(void *dst, const void *src, size_t size)
{
    hv_memcpy(dst, src, size);
    return dst;
}
