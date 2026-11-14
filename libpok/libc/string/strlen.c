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
 * all if/else/loop bodies.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameter.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strlen() of libpok.
 */

#include <string.h>

/**
 * @funcname `strlen`
 *
 * @description
 * Returns the length of a string (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The string. |
 *
 * @return The number of characters before the NUL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `len` | `size_t` | The length |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `len` to `0U`.
 * 2. While `s[len]` is not equal to `'\0'`:
 *    1. Increment `len`.
 * 3. Return `len`.
 *
 * @calls
 * None.
 */
size_t strlen(const char *s)
{
    size_t len = 0U;

    while(s[len] != '\0') {
        ++len;
    }

    return len;
}
