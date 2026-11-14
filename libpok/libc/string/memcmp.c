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
 * all if/else/loop bodies; essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * with a single exit; the bytes are compared as int; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * memcmp() of libpok (GCC requires it even for a freestanding
 * environment).
 */

#include <string.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memcmp() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */

/**
 * @funcname `memcmp`
 *
 * @description
 * Compares two blocks of memory (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const void *` | [in] The first block. |
 * | `s2` | `const void *` | [in] The second block. |
 * | `n` | `size_t` | [in] The number of bytes. |
 *
 * @return 0 if they are equal, otherwise the difference of the first bytes that
 *         differ.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte |
 * | `mem1` | `const unsigned char *` | The first block, as bytes |
 * | `mem2` | `const unsigned char *` | The second block, as bytes |
 * | `result` | `int` | The result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `mem1` to `s1`.
 * 2. Set `mem2` to `s2`.
 * 3. Set `i` to `0U`.
 * 4. Set `result` to `0`.
 * 5. While `i` is less than `n` and `mem1[i]` is equal to `mem2[i]`:
 *    1. Increment `i`.
 * 6. If `i` is less than `n`:
 *    1. Set `result` to `(int) mem1[i] - (int) mem2[i]`.
 * 7. Return `result`.
 *
 * @calls
 * None.
 */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *mem1 = s1;
    const unsigned char *mem2 = s2;
    size_t i = 0U;
    int result = 0;

    while((i < n) && (mem1[i] == mem2[i]))
    {
        i++;
    }

    if(i < n) {
        result = (int) mem1[i] - (int) mem2[i];
    }
    return result;
}
