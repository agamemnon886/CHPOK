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
 * unsigned constants; a single loop with one exit; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strpbrk() of libpok.
 */

#include <string.h>

/**
 * @funcname `strpbrk`
 *
 * @description
 * Finds the first character of a string that is in another (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const char *` | [in] The string. |
 * | `s2` | `const char *` | [in] The characters looked for. |
 *
 * @return The address of the character, or NULL if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `found` | `const char *` | The character found |
 * | `i` | `size_t` | The index of the character |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `found` to `NULL`.
 * 2. Set `i` to `0U`.
 * 3. While `found` is NULL and `s1[i]` is not equal to `'\0'`:
 *    1. If `strchr(s2, (int)s1[i])` is not NULL:
 *       1. Set `found` to the address of `s1[i]`.
 *    2. Otherwise:
 *       1. Increment `i`.
 * 4. Return `(char*)found`.
 *
 * @calls
 * - `strchr()`
 */
char *strpbrk(const char *s1, const char *s2)
{
    const char *found = NULL;
    size_t i = 0U;

    while((found == NULL) && (s1[i] != '\0')) {
        if(strchr(s2, (int)s1[i]) != NULL) {
            found = &s1[i];
        }
        else {
            i++;
        }
    }

    /*
     * MISRA C:2025 Rule 11.8 deviation: the standard strpbrk() returns a
     * pointer to char into the string it gets as a pointer to const.
     */
    return (char*)found;
}
