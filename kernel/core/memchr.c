/*
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; U suffixes for unsigned constants; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameters; deviations are documented where a
 * guideline can't be followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/memchr.c
 * (kernel/libc is split up: the routines only the kernel uses are in
 * kernel/core).
 */

/**
 * @file
 * memchr() of the kernel's C library.
 */

#include <libc.h>

/**
 * @funcname `memchr`
 *
 * @description
 * Finds the first byte of a value in a block of memory (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const void *` | [in] The memory. |
 * | `c` | `int` | [in] The value (converted to unsigned char). |
 * | `n` | `size_t` | [in] The size of the memory. |
 *
 * @return The address of the byte, or NULL if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte |
 * | `p` | `const unsigned char *` | The memory, as bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `s`.
 * 2. Set `i` to `0U`; then while `i` is less than `n`, repeat (after each pass:
 *    increment `i`):
 *    1. If `(unsigned char)c` is equal to `p[i]`:
 *       1. Return `(void *)&p[i]`.
 * 3. Return `NULL`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memchr() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
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
