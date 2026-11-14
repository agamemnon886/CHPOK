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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: rewritten
 * without arithmetic on two characters: both characters are converted to
 * upper case and compared as unsigned char (the result is unchanged for
 * ASCII text); a single loop exit.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strncasecmp() of the kernel's C library (used to compare ARINC 653
 * names).
 */

#include <config.h>

#if defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_PORTS_QUEUEING) || defined (POK_NEEDS_PORTS_SAMPLING)

#include <libc.h>

/**
 * @funcname `to_upper`
 *
 * @description
 * Converts an ASCII letter to upper case.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 *
 * @return The upper case letter, or c if it isn't a lower case letter.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `upper` | `char` | The result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `upper` to `c`.
 * 2. If `c` is greater than or equal to `'a'` and `c` is less than or equal to
 *    `'z'`:
 *    1. Set `upper` to `(c - 'a') + 'A'`.
 * 3. Return `upper`.
 *
 * @calls
 * None.
 */
static char to_upper(char c)
{
    char upper = c;

    if ((c >= 'a') && (c <= 'z')) {
        upper = (c - 'a') + 'A';
    }
    return upper;
}

/**
 * @funcname `strncasecmp`
 *
 * @description
 * Compares two strings without regard to case, up to a number of characters
 * (POSIX).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const char *` | [in] The first string. |
 * | `s2` | `const char *` | [in] The second string. |
 * | `size` | `size_t` | [in] The maximum number of characters. |
 *
 * @return 0 if they are equal, -1 if s1 comes first, 1 otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c1` | `uint8_t` | The character of s1, in upper case |
 * | `c2` | `uint8_t` | The character of s2, in upper case |
 * | `done` | `bool` | Whether the comparison is over |
 * | `i` | `size_t` | The index of the character |
 * | `result` | `int` | The result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `result` to `0`.
 * 2. Set `i` to `0U`.
 * 3. Set `done` to `false`.
 * 4. While `done` is false and `i` is less than `size`:
 *    1. Set `c1` to `(uint8_t) to_upper(s1[i])`.
 *    2. Set `c2` to `(uint8_t) to_upper(s2[i])`.
 *    3. If `c1` is not equal to `c2`:
 *       1. Set `result` to `-1` if `c1` is less than `c2`, otherwise `1`.
 *       2. Set `done` to `true`.
 *    4. Otherwise, if `c1` is equal to `0U`:
 *       1. Set `done` to `true`.
 *    5. Otherwise:
 *       1. Increment `i`.
 * 5. Return `result`.
 *
 * @calls
 * - `to_upper()`
 */
/*
 * MISRA C:2025 Dir 1.2 deviation: __attribute__((weak)) is a GNU extension;
 * it lets a board or a configuration provide its own implementation.
 */
__attribute__ ((weak))
int strncasecmp(const char *s1, const char *s2, size_t size)
{
    int result = 0;
    size_t i = 0U;
    bool done = false;

    while ((!done) && (i < size)) {
        uint8_t c1 = (uint8_t) to_upper(s1[i]);
        uint8_t c2 = (uint8_t) to_upper(s2[i]);

        if (c1 != c2) {
            result = (c1 < c2) ? -1 : 1;
            done = true;
        } else if (c1 == 0U) {
            done = true; // Both strings end here.
        } else {
            i++;
        }
    }
    return result;
}
#endif
