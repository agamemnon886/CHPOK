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
 * unsigned constants; the substring is compared with strncmp(), which
 * stops at the end of the string (memcmp() could read past it); one exit;
 * deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strstr() of libpok (forward implementation, O(n*m)).
 */

#include <string.h>

/**
 * @funcname `strstr`
 *
 * @description
 * Finds the first occurrence of a string in another (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const char *` | [in] The string. |
 * | `s2` | `const char *` | [in] The string looked for. |
 *
 * @return The address of the occurrence, s1 if s2 is empty, or NULL if there is
 *         none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `found` | `const char *` | The occurrence |
 * | `p` | `const char *` | The position in s1 |
 * | `s2_len` | `size_t` | The length of s2 |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `strlen()` with the argument `s2`, and store the result in `s2_len`.
 * 2. Set `found` to `NULL`.
 * 3. If `s2_len` is equal to `0U`:
 *    1. Set `found` to `s1`.
 * 4. Otherwise:
 *    1. Set `p` to `s1`.
 *    2. While `found` is NULL and the value at `p` is not equal to `'\0'`:
 *       1. If `strncmp(p, s2, s2_len)` is equal to `0`:
 *          1. Set `found` to `p`.
 *       2. Otherwise:
 *          1. Increment `p`.
 * 5. Return `(char*)found`.
 *
 * @calls
 * - `strlen()`
 * - `strncmp()`
 */
char *strstr(const char *s1, const char *s2)
{
    // Forward implementation, O(n*m)
    size_t s2_len = strlen(s2);
    const char *found = NULL;

    if(s2_len == 0U) {
        found = s1; // Nothing to search.
    }
    else {
        const char *p = s1;

        while((found == NULL) && (*p != '\0')) {
            if(strncmp(p, s2, s2_len) == 0) {
                found = p;
            }
            else {
                p++;
            }
        }
    }

    /*
     * MISRA C:2025 Rule 11.8 deviation: the standard strstr() returns a
     * pointer to char into the string it gets as a pointer to const char.
     */
    return (char*)found;
}
