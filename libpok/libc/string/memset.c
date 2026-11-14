/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameters; deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * memset() of libpok (GCC requires it even for a freestanding
 * environment).
 */

#include <string.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memset() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */

/**
 * @funcname `memset`
 *
 * @description
 * Sets a block of memory to a byte value (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `void *` | [out] The memory. |
 * | `c` | `int` | [in] The value (converted to unsigned char). |
 * | `n` | `size_t` | [in] The number of bytes. |
 *
 * @return s.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `unsigned char *` | The memory, as bytes |
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `d` to `(unsigned char *)s`.
 * 2. Set `i` to `0U`; then while `i` is less than `n`, repeat (after each pass:
 *    increment `i`):
 *    1. Set `d[i]` to `(unsigned char)c`.
 * 3. Return `s`.
 *
 * @calls
 * None.
 */
void *memset(void *s, int c, size_t n)
{
    unsigned char *d = (unsigned char *)s;

    for (size_t i = 0U; i < n; i++) {
        d[i] = (unsigned char)c;
    }

    return s;
}
