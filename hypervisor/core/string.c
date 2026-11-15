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
 * are in include/hv/hv.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/src/string.c (the common code and its headers are in
 * hypervisor/core).
 */

/**
 * @file
 * Memory functions of the hypervisor, and memset()/memcpy(), which GCC may
 * call for structure assignments and initializations.
 */

#include <hv/hv.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the functions take their
 * memory as pointers to void, as memset() and memcpy() do, and access it
 * as bytes.
 */

/**
 * @funcname `hv_memset`
 *
 * @description
 * Sets a block of memory to one byte value.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dst` | `void *` | [out] The memory. |
 * | `value` | `uint8_t` | [in] The byte value. |
 * | `size` | `size_t` | [in] The number of bytes. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `uint8_t *` | The memory, as bytes |
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `d` to `dst`.
 * 2. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`):
 *    1. Set `d[i]` to `value`.
 *
 * @calls
 * None.
 */
void hv_memset(void *dst, uint8_t value, size_t size)
{
    uint8_t *d = dst;

    for (size_t i = 0U; i < size; i++) {
        d[i] = value;
    }
}

/**
 * @funcname `hv_memcpy`
 *
 * @description
 * Copies a block of memory; the source and the destination must not
 * overlap.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dst` | `void *` | [out] The destination. |
 * | `src` | `const void *` | [in] The source. |
 * | `size` | `size_t` | [in] The number of bytes. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `uint8_t *` | The destination, as bytes |
 * | `s` | `const uint8_t *` | The source, as bytes |
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `d` to `dst`.
 * 2. Set `s` to `src`.
 * 3. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`):
 *    1. Set `d[i]` to `s[i]`.
 *
 * @calls
 * None.
 */
void hv_memcpy(void *dst, const void *src, size_t size)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    for (size_t i = 0U; i < size; i++) {
        d[i] = s[i];
    }
}

/**
 * @funcname `memset`
 *
 * @description
 * The C standard memset(), for the calls that GCC generates (the hypervisor
 * has no C library).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dst` | `void *` | [out] The memory. |
 * | `value` | `int` | [in] The byte value (converted to uint8_t). |
 * | `size` | `size_t` | [in] The number of bytes. |
 *
 * @return dst.
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
 * 1. Call `hv_memset()` with the arguments `dst`, `(uint8_t) value` and `size`.
 * 2. Return `dst`.
 *
 * @calls
 * - `hv_memset()`
 */
void *memset(void *dst, int value, size_t size)
{
    hv_memset(dst, (uint8_t) value, size);
    return dst;
}

/**
 * @funcname `memcpy`
 *
 * @description
 * The C standard memcpy(), for the calls that GCC generates (the hypervisor
 * has no C library).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dst` | `void *` | [out] The destination. |
 * | `src` | `const void *` | [in] The source (must not overlap dst). |
 * | `size` | `size_t` | [in] The number of bytes. |
 *
 * @return dst.
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
 * 1. Call `hv_memcpy()` with the arguments `dst`, `src` and `size`.
 * 2. Return `dst`.
 *
 * @calls
 * - `hv_memcpy()`
 */
void *memcpy(void *dst, const void *src, size_t size)
{
    hv_memcpy(dst, src, size);
    return dst;
}
