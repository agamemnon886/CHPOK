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
 */

#include <string.h>

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): the function walks the
 * string with pointers into it.
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

char *strtok(char * s1, const char * s2)
{
    static char* next_s1 = NULL;
    return strtok_r(s1, s2, &next_s1);
}
