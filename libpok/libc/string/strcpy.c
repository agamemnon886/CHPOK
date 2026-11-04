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
 */

#include <string.h>

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
