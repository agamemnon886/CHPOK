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
 * unsigned constants; index loops with a single exit.
 */

#include <string.h>

int strcmp(const char *s1, const char *s2)
{
    const unsigned char *str1 = (const unsigned char *)s1;
    const unsigned char *str2 = (const unsigned char *)s2;
    size_t i = 0U;

    while ((str1[i] != 0U) && (str1[i] == str2[i])) {
        i++;
    }

    return (int) str1[i] - (int) str2[i];
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    const unsigned char *str1 = (const unsigned char *)s1;
    const unsigned char *str2 = (const unsigned char *)s2;
    size_t i = 0U;
    int result = 0;

    while ((i < n) && (str1[i] != 0U) && (str1[i] == str2[i])) {
        i++;
    }

    if (i < n) {
        result = (int) str1[i] - (int) str2[i];
    }
    return result;
}
