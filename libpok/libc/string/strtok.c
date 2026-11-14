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
 * all if/else/loop bodies; essentially Boolean conditions; no restrict
 * qualifiers.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: strtok_r()
 * doesn't modify its parameter; deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * strtok() of libpok.
 */

#include <string.h>

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): the function walks the
 * string with pointers into it.
 */

/**
 * @funcname `strtok_r`
 *
 * @description
 * Returns the next token of a string, separated by characters of another (POSIX
 * strtok_r()): the separator after it is replaced with a NUL, and the position
 * after it is saved for the next call.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `str` | `char *` | [in,out] The string, or NULL to go on with the saved position. |
 * | `s2` | `const char *` | [in] The separators. |
 * | `save_ptr` | `char **` | [in,out] The saved position. |
 *
 * @return The token, or NULL if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `char *` | The start of the token |
 * | `s1_end` | `char *` | Its end |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `s1` to `str`.
 * 2. If `s1` is NULL:
 *    1. If `((*save_ptr))` is NULL:
 *       1. Return `NULL`.
 *    2. Set `s1` to the value at `save_ptr`.
 * 3. While the value at `s1` is not equal to `'\0'`, repeat (after each pass:
 *    increment `s1`):
 *    1. If `strchr(s2, *s1)` is NULL:
 *       1. Leave the loop.
 * 4. If the value at `s1` is equal to `'\0'`:
 *    1. Set `*save_ptr` to `NULL`.
 *    2. Return `NULL`.
 * 5. Set `s1_end` to `s1 + 1`.
 * 6. While the value at `s1_end` is not equal to `'\0'`, repeat (after each
 *    pass: increment `s1_end`):
 *    1. If `strchr(s2, *s1_end)` is not NULL:
 *       1. Leave the loop.
 * 7. If the value at `s1_end` is not equal to `'\0'`:
 *    1. Set `*s1_end` to `'\0'`.
 *    2. Set `*save_ptr` to `s1_end + 1`.
 * 8. Otherwise:
 *    1. Set `*save_ptr` to `NULL`.
 * 9. Return `s1`.
 *
 * @calls
 * - `strchr()`
 */
static char *strtok_r(char * str, const char * s2,
    char** save_ptr)
{
    char *s1 = str;

    // Forward implementation, O(n*m)
    if(s1 == NULL) {
        if(((*save_ptr)) == NULL) {
            return NULL;
        }
        s1 = *save_ptr;
    }

    // Look for the first character not in s2.
    for(; (*s1) != '\0'; s1++) {
        if(strchr(s2, *s1) == NULL) {
            break;
        }
    }

    if((*s1) == '\0') {
        // Not found.
        *save_ptr = NULL;
        return NULL;
    }

    char* s1_end = s1 + 1;

    // Search for the end of the token.
    for(; (*s1_end) != '\0'; s1_end++) {
        if(strchr(s2, *s1_end) != NULL) {
            break;
        }
    }

    if((*s1_end) != '\0') {
        // Found delimiter, replace it with null.
        *s1_end = '\0';
        // And store followed position for the future search.
        *save_ptr = s1_end + 1;
    }
    else {
        // Found end of string. This is the last token.
        *save_ptr = NULL;
    }

    return s1;
}

/**
 * @funcname `strtok`
 *
 * @description
 * Returns the next token of a string, separated by characters of another (C
 * standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `char *` | [in,out] The string, or NULL to go on with the last one. |
 * | `s2` | `const char *` | [in] The separators. |
 *
 * @return The token, or NULL if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `next_s1` | `static char *` | The saved position |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `strtok_r()` with the arguments `s1`, `s2` and the address of
 *    `next_s1`, and return its result.
 *
 * @calls
 * - `strtok_r()`
 */
char *strtok(char * s1, const char * s2)
{
    static char* next_s1 = NULL;
    return strtok_r(s1, s2, &next_s1);
}
