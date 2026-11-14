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
 * all if/else/loop bodies; U suffixes for unsigned constants; no restrict
 * qualifiers; strncpy() writes exactly n bytes (it wrote one null
 * character more, past the end of the destination).
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameters.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strcpy() and strncpy() of libpok.
 */

#include <string.h>

/**
 * @funcname `strcpy`
 *
 * @description
 * Copies a string (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dest` | `char *` | [out] The destination. |
 * | `src` | `const char *` | [in] The string. |
 *
 * @return dest.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the character |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`.
 * 2. While `src[i]` is not equal to `'\0'`:
 *    1. Set `dest[i]` to `src[i]`.
 *    2. Increment `i`.
 * 3. Set `dest[i]` to `'\0'`.
 * 4. Return `dest`.
 *
 * @calls
 * None.
 */
char *strcpy(char * dest, const char * src)
{
    size_t i = 0U;

    while(src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';

    return dest;
}

/**
 * @funcname `strncpy`
 *
 * @description
 * Copies a string into a buffer of n bytes, filling the rest with NUL
 * characters (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dest` | `char *` | [out] The buffer. |
 * | `src` | `const char *` | [in] The string. |
 * | `n` | `size_t` | [in] The size of the buffer. |
 *
 * @return dest.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the character |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`.
 * 2. While `i` is less than `n` and `src[i]` is not equal to `'\0'`:
 *    1. Set `dest[i]` to `src[i]`.
 *    2. Increment `i`.
 * 3. While `i` is less than `n`:
 *    1. Set `dest[i]` to `'\0'`.
 *    2. Increment `i`.
 * 4. Return `dest`.
 *
 * @calls
 * None.
 */
char *strncpy(char * dest, const char * src, size_t n)
{
    size_t i = 0U;

    while ((i < n) && (src[i] != '\0')) {
        dest[i] = src[i];
        i++;
    }

    // The rest of the n bytes are filled with null characters.
    while (i < n) {
        dest[i] = '\0';
        i++;
    }

    return dest;
}
