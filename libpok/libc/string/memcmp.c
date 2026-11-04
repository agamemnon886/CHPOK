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
 * all if/else/loop bodies; essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * with a single exit; the bytes are compared as int; deviations are
 * documented where a guideline can't be followed.
 */

#include <string.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memcmp() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */

/* GCC requires this function even for freestanding environment. */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *mem1 = s1;
    const unsigned char *mem2 = s2;
    size_t i = 0U;
    int result = 0;

    while((i < n) && (mem1[i] == mem2[i]))
    {
        i++;
    }

    if(i < n) {
        result = (int) mem1[i] - (int) mem2[i];
    }
    return result;
}
