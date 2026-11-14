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
 * unsigned constants; strchr() and strrchr() convert c to char, as in the
 * C standard (on x86_64 they didn't find characters above 0x7f);
 * deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strchr() and strrchr() of libpok.
 */

#include <string.h>
#include <stdbool.h>

/**
 * @funcname `strchr`
 *
 * @description
 * Finds the first occurrence of a character in a string (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The string. |
 * | `c` | `int` | [in] The character (converted to char). |
 *
 * @return The address of the character, or NULL if it isn't there.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `const char` | The character |
 * | `i` | `size_t` | The index of the character |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ch` to `(char)c`.
 * 2. Set `i` to `0U`.
 * 3. While `s[i]` is not equal to `'\0'` and `s[i]` is not equal to `ch`:
 *    1. Increment `i`.
 * 4. Return `(char*)&s[i]` if `s[i]` is equal to `ch`, otherwise `NULL`.
 *
 * @calls
 * None.
 */
char *strchr(const char *s, int c)
{
    // As in the C standard, c is converted to char.
    const char ch = (char)c;
    size_t i = 0U;

    while((s[i] != '\0') && (s[i] != ch)) {
        i++;
    }

    /*
     * MISRA C:2025 Rule 11.8 deviation: the standard strchr() returns a
     * pointer to char into the string it gets as a pointer to const.
     */
    return (s[i] == ch) ? (char*)&s[i] : NULL;
}

/**
 * @funcname `strrchr`
 *
 * @description
 * Finds the last occurrence of a character in a string; the terminating NUL is
 * a part of the string (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The string. |
 * | `c` | `int` | [in] The character (converted to char). |
 *
 * @return The address of the character, or NULL if it isn't there.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `const char` | The character |
 * | `done` | `bool` | Whether the end of the string was reached |
 * | `i` | `size_t` | The index of the character |
 * | `res` | `const char *` | The last occurrence so far |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ch` to `(char)c`.
 * 2. Set `res` to `NULL`.
 * 3. Set `i` to `0U`.
 * 4. Set `done` to `false`.
 * 5. While `done` is false:
 *    1. If `s[i]` is equal to `ch`:
 *       1. Set `res` to the address of `s[i]`.
 *    2. If `s[i]` is equal to `'\0'`:
 *       1. Set `done` to `true`.
 *    3. Otherwise:
 *       1. Increment `i`.
 * 6. Return `(char*)res`.
 *
 * @calls
 * None.
 */
char *strrchr(const char *s, int c)
{
    // As in the C standard, c is converted to char.
    const char ch = (char)c;
    const char *res = NULL;
    size_t i = 0U;
    bool done = false;

    // The terminating null character is a part of the string.
    while(!done) {
        if(s[i] == ch) {
            res = &s[i];
        }
        if(s[i] == '\0') {
            done = true;
        }
        else {
            i++;
        }
    }

    /*
     * MISRA C:2025 Rule 11.8 deviation: the standard strrchr() returns a
     * pointer to char into the string it gets as a pointer to const.
     */
    return (char*)res;
}
