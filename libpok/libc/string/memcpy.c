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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; U suffixes for unsigned constants; no restrict
 * qualifiers.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameters; deviations are documented where a
 * guideline can't be followed.
 */

#include <string.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memcpy() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */

/* GCC requires this function even for freestanding environment. */
void *memcpy(void * dest, const void * src, size_t n)
{
    const unsigned char *s = (const unsigned char*) src;
    unsigned char *d = (unsigned char*) dest;

    for (size_t i = 0U; i < n; i++)
    {
        d[i] = s[i];
    }

    return dest;
}
