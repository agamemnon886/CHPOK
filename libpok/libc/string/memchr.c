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
 * all if/else/loop bodies; U suffixes for unsigned constants; an index
 * loop; deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * memchr() of libpok.
 */

#include <string.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memchr() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */

/**
 * @funcname `memchr`
 *
 * @description
 * Finds the first byte of a value in a block of memory (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const void *` | [in] The memory. |
 * | `c` | `int` | [in] The value (converted to unsigned char). |
 * | `n` | `size_t` | [in] The size of the memory. |
 *
 * @return The address of the byte, or NULL if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `const unsigned char` | The value |
 * | `i` | `size_t` | The index of the byte |
 * | `mem` | `const unsigned char *` | The memory, as bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `mem` to `(const unsigned char *)s`.
 * 2. Set `ch` to `(unsigned char)c`.
 * 3. Set `i` to `0U`.
 * 4. While `i` is less than `n` and `mem[i]` is not equal to `ch`:
 *    1. Increment `i`.
 * 5. Return `(void*)&mem[i]` if `i` is less than `n`, otherwise `NULL`.
 *
 * @calls
 * None.
 */
void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *mem = (const unsigned char *)s;
    const unsigned char ch = (unsigned char)c;
    size_t i = 0U;

    while((i < n) && (mem[i] != ch)) {
        i++;
    }

    /*
     * MISRA C:2025 Rule 11.8 deviation: the standard memchr() returns a
     * pointer to void into the memory it gets as a pointer to const.
     */
    return (i < n) ? (void*)&mem[i] : NULL;
}
