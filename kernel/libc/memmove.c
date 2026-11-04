/*
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the copy direction
 * is chosen by comparing the addresses as integers (the pointers may point
 * into different objects), and the copy loops use indexes.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Long lines are wrapped at 80 columns.
 */


#include <libc.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memmove() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */
/*
 *  linux/lib/string.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */
/**
 * memmove - Copy one area of memory to another
 * @dest: Where to copy to
 * @src: Where to copy from
 * @count: The size of the area.
 *
 * Unlike memcpy(), memmove() copes with overlapping areas.
 */
void *memmove(void *dest, const void *src, size_t count)
{
	unsigned char *d = (unsigned char *) dest;
	const unsigned char *s = (const unsigned char *) src;

	/*
	 * MISRA C:2025 Rule 11.6 deviation: dest and src may point into
	 * different objects, which cannot be compared as pointers (Rule 18.3).
	 * Their addresses are compared as integers to choose the copy
	 * direction.
	 */
	if ((uintptr_t) dest <= (uintptr_t) src) {
		for (size_t i = 0U; i < count; i++) {
			d[i] = s[i];
		}
	} else {
		size_t i = count;

		while (i > 0U) {
			i--;
			d[i] = s[i];
		}
	}
	return dest;
}
