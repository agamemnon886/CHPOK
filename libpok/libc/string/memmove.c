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
 * all if/else/loop bodies; U suffixes for unsigned constants; overlapping
 * areas are copied in the right direction (the direction was reversed,
 * which corrupted the copy); the addresses are compared as integers;
 * deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * memmove() of libpok (GCC requires it even for a freestanding
 * environment).
 */

#include <string.h>
#include <stdint.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memmove() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */

/**
 * @funcname `memmove`
 *
 * @description
 * Copies a block of memory that may overlap the destination (C standard): from
 * the beginning if the destination starts before the source or the areas don't
 * overlap, from the end otherwise.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dest` | `void *` | [out] The destination. |
 * | `src` | `const void *` | [in] The source. |
 * | `n` | `size_t` | [in] The number of bytes. |
 *
 * @return dest.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `unsigned char *` | The destination, as bytes |
 * | `d_addr` | `uintptr_t` | The address of the destination |
 * | `i` | `size_t` | The index of the byte |
 * | `s` | `const unsigned char *` | The source, as bytes |
 * | `s_addr` | `uintptr_t` | The address of the source |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `d` to `(unsigned char *) dest`.
 * 2. Set `s` to `(const unsigned char *) src`.
 * 3. Set `d_addr` to `(uintptr_t) dest`.
 * 4. Set `s_addr` to `(uintptr_t) src`.
 * 5. If `d_addr` is less than or equal to `s_addr` or `d_addr` is greater than
 *    or equal to `(s_addr + n)`:
 *    1. Set `i` to `0U`; then while `i` is less than `n`, repeat (after each
 *       pass: increment `i`):
 *       1. Set `d[i]` to `s[i]`.
 * 6. Otherwise:
 *    1. Set `i` to `n`.
 *    2. While `i` is greater than `0U`:
 *       1. Decrement `i`.
 *       2. Set `d[i]` to `s[i]`.
 * 7. Return `dest`.
 *
 * @calls
 * None.
 */
void *memmove(void* dest, const void* src, size_t n)
{
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;

    /*
     * MISRA C:2025 Rule 11.6 deviation: dest and src may point into different
     * objects, which cannot be compared as pointers (Rule 18.3). Their
     * addresses are compared as integers to choose the copy direction.
     */
    uintptr_t d_addr = (uintptr_t) dest;
    uintptr_t s_addr = (uintptr_t) src;

    if ((d_addr <= s_addr) || (d_addr >= (s_addr + n))) {
        // dest starts before src or the areas don't overlap: copy from the
        // beginning.
        for (size_t i = 0U; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        // dest overlaps the end of src: copy from the end.
        size_t i = n;

        while (i > 0U) {
            i--;
            d[i] = s[i];
        }
    }
    return dest;
}
