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
 */

#include <string.h>

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
