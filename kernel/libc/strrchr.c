/* Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; deviations are documented where a guideline
 * can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameters. Long lines are wrapped at 80 columns.
 */

#include <libc.h>
/*
 *  linux/lib/string.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */
/**
 * strrchr - Find the last occurrence of a character in a string
 * @s: The string to be searched
 * @c: The character to search for
 */
char *strrchr(const char *s, int c)
{
        const char *last = NULL;
        size_t i = 0U;
        bool done = false;

        while (!done) {
                if (s[i] == (char)c) {
                        last = &s[i];
                }
                done = (s[i] == '\0');
                i++;
        }
        /*
         * MISRA C:2025 Rule 11.8 deviation: the standard strrchr() returns a
         * pointer to char into the string it gets as a pointer to const.
         */
        return (char *)last;
}
