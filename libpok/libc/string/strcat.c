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
 * all if/else/loop bodies; no restrict qualifiers.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: strcat() and
 * strncat() use index loops that don't modify the parameters.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strcat() and strncat() of libpok.
 */

#include <string.h>

/**
 * @funcname `strcat`
 *
 * @description
 * Appends a string to another (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dest` | `char *` | [in,out] The string appended to. |
 * | `src` | `const char *` | [in] The string appended. |
 *
 * @return dest.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `size_t` | The length of dest |
 * | `i` | `size_t` | The index of the character of src |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `strlen()` with the argument `dest`, and store the result in `d`.
 * 2. Set `i` to `0U`.
 * 3. While `src[i]` is not equal to `'\0'`:
 *    1. Set `dest[d + i]` to `src[i]`.
 *    2. Increment `i`.
 * 4. Set `dest[d + i]` to `'\0'`.
 * 5. Return `dest`.
 *
 * @calls
 * - `strlen()`
 */
char *strcat(char * dest, const char * src)
{
    size_t d = strlen(dest);
    size_t i = 0U;

    while(src[i] != '\0') {
        dest[d + i] = src[i];
        i++;
    }

    dest[d + i] = '\0';

    return dest;
}

/**
 * @funcname `strncat`
 *
 * @description
 * Appends at most n characters of a string to another, and a NUL (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dest` | `char *` | [in,out] The string appended to. |
 * | `src` | `const char *` | [in] The string appended. |
 * | `n` | `size_t` | [in] The maximum number of characters. |
 *
 * @return dest.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `size_t` | The length of dest |
 * | `i` | `size_t` | The index of the character of src |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `strlen()` with the argument `dest`, and store the result in `d`.
 * 2. Set `i` to `0U`.
 * 3. While `i` is less than `n` and `src[i]` is not equal to `'\0'`:
 *    1. Set `dest[d + i]` to `src[i]`.
 *    2. Increment `i`.
 * 4. Set `dest[d + i]` to `'\0'`.
 * 5. Return `dest`.
 *
 * @calls
 * - `strlen()`
 */
char *strncat(char * dest, const char * src, size_t n)
{
    size_t d = strlen(dest);
    size_t i = 0U;

    while((i < n) && (src[i] != '\0')) {
        dest[d + i] = src[i];
        i++;
    }

    dest[d + i] = '\0';

    return dest;
}
