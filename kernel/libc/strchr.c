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
 * strchr - Find the first occurrence of a character in a string
 * @s: The string to be searched
 * @c: The character to search for
 */
char *strchr(const char *s, int c)
{
	size_t i = 0U;

	while (s[i] != (char)c) {
		if (s[i] == '\0') {
			return NULL;
		}
		i++;
	}
	/*
	 * MISRA C:2025 Rule 11.8 deviation: the standard strchr() returns a
	 * pointer to char into the string it gets as a pointer to const.
	 */
	return (char *)&s[i];
}
