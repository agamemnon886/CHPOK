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
 * unsigned constants; index loops with a single exit.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strcmp() and strncmp() of libpok.
 */

#include <string.h>

/**
 * @funcname `strcmp`
 *
 * @description
 * Compares two strings (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const char *` | [in] The first string. |
 * | `s2` | `const char *` | [in] The second string. |
 *
 * @return 0 if they are equal, otherwise the difference of the first characters
 *         that differ (as unsigned char).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the character |
 * | `str1` | `const unsigned char *` | The first string, as unsigned char |
 * | `str2` | `const unsigned char *` | The second string, as unsigned char |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `str1` to `(const unsigned char *)s1`.
 * 2. Set `str2` to `(const unsigned char *)s2`.
 * 3. Set `i` to `0U`.
 * 4. While `str1[i]` is not equal to `0U` and `str1[i]` is equal to `str2[i]`:
 *    1. Increment `i`.
 * 5. Return `(int) str1[i] - (int) str2[i]`.
 *
 * @calls
 * None.
 */
int strcmp(const char *s1, const char *s2)
{
    const unsigned char *str1 = (const unsigned char *)s1;
    const unsigned char *str2 = (const unsigned char *)s2;
    size_t i = 0U;

    while ((str1[i] != 0U) && (str1[i] == str2[i])) {
        i++;
    }

    return (int) str1[i] - (int) str2[i];
}

/**
 * @funcname `strncmp`
 *
 * @description
 * Compares two strings, up to n characters (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const char *` | [in] The first string. |
 * | `s2` | `const char *` | [in] The second string. |
 * | `n` | `size_t` | [in] The maximum number of characters. |
 *
 * @return 0 if they are equal, otherwise the difference of the first characters
 *         that differ (as unsigned char).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the character |
 * | `result` | `int` | The result |
 * | `str1` | `const unsigned char *` | The first string, as unsigned char |
 * | `str2` | `const unsigned char *` | The second string, as unsigned char |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `str1` to `(const unsigned char *)s1`.
 * 2. Set `str2` to `(const unsigned char *)s2`.
 * 3. Set `i` to `0U`.
 * 4. Set `result` to `0`.
 * 5. While `i` is less than `n` and `str1[i]` is not equal to `0U` and
 *    `str1[i]` is equal to `str2[i]`:
 *    1. Increment `i`.
 * 6. If `i` is less than `n`:
 *    1. Set `result` to `(int) str1[i] - (int) str2[i]`.
 * 7. Return `result`.
 *
 * @calls
 * None.
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
    const unsigned char *str1 = (const unsigned char *)s1;
    const unsigned char *str2 = (const unsigned char *)s2;
    size_t i = 0U;
    int result = 0;

    while ((i < n) && (str1[i] != 0U) && (str1[i] == str2[i])) {
        i++;
    }

    if (i < n) {
        result = (int) str1[i] - (int) str2[i];
    }
    return result;
}
