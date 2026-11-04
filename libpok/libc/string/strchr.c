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
 */

#include <string.h>
#include <stdbool.h>

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
