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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: rewritten
 * without arithmetic on two characters: both characters are converted to
 * upper case and compared as unsigned char (the result is unchanged for
 * ASCII text); a single loop exit.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 */

#include <config.h>

#if defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_PORTS_QUEUEING) || defined (POK_NEEDS_PORTS_SAMPLING)

#include <libc.h>

/* The upper-case letter for a lower-case one; other characters are returned as
 * is. */
static char to_upper(char c)
{
    char upper = c;

    if ((c >= 'a') && (c <= 'z')) {
        upper = (c - 'a') + 'A';
    }
    return upper;
}

/*
 * MISRA C:2025 Dir 1.2 deviation: __attribute__((weak)) is a GNU extension;
 * it lets a board or a configuration provide its own implementation.
 */
__attribute__ ((weak))
int strncasecmp(const char *s1, const char *s2, size_t size)
{
    int result = 0;
    size_t i = 0U;
    bool done = false;

    while ((!done) && (i < size)) {
        uint8_t c1 = (uint8_t) to_upper(s1[i]);
        uint8_t c2 = (uint8_t) to_upper(s2[i]);

        if (c1 != c2) {
            result = (c1 < c2) ? -1 : 1;
            done = true;
        } else if (c1 == 0U) {
            done = true; // Both strings end here.
        } else {
            i++;
        }
    }
    return result;
}
#endif
