/* Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; U suffixes for unsigned constants; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameters; deviations are documented where a
 * guideline can't be followed. Long lines are wrapped at 80 columns.
 */

#include <libc.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memchr() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */
/*
 *  linux/lib/string.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */
/**
 * memchr - Find a character in an area of memory.
 * @s: The memory area
 * @c: The byte to search for
 * @n: The size of the area.
 *
 * returns the address of the first occurrence of @c, or %NULL
 * if @c is not found
 */
void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = s;
	for (size_t i = 0U; i < n; i++) {
		if ((unsigned char)c == p[i]) {
			/*
			 * MISRA C:2025 Rule 11.8 deviation: the standard
			 * memchr() returns a pointer to void into the memory it
			 * gets as a pointer to const.
			 */
			return (void *)&p[i];
		}
	}
	return NULL;
}
