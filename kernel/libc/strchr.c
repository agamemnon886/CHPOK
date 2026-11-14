/*
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; deviations are documented where a guideline
 * can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameters. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * strchr() of the kernel's C library.
 */

#include <libc.h>
/*
 *  linux/lib/string.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/**
 * @funcname `strchr`
 *
 * @description
 * Finds the first occurrence of a character in a string (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The string. |
 * | `c` | `int` | [in] The character (converted to char). |
 *
 * @return The address of the character, or NULL if it isn't there.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the character |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`.
 * 2. While `s[i]` is not equal to `(char)c`:
 *    1. If `s[i]` is equal to `'\0'`:
 *       1. Return `NULL`.
 *    2. Increment `i`.
 * 3. Return `(char *)&s[i]`.
 *
 * @calls
 * None.
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
