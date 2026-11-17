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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design.
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
 * - \hlrid{HLR_MEMCHR_MEMCHR_01}: `memchr()` shall give the address of the
 *   first of `n` bytes of a block that has the value `c` (converted to
 *   `unsigned char`), or NULL if none has.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_MEMCHR_MEMCHR_01}: If one of the first `n` bytes at `s` is equal
 *   to `c` converted to `unsigned char`, `memchr()` shall return the address of
 *   the first such byte.
 *   <br>Traces to:
 *   - \hlrref{HLR_MEMCHR_MEMCHR_01}
 * - \llrid{LLR_MEMCHR_MEMCHR_02}: If none of the first `n` bytes at `s` is
 *   equal to `c` converted to `unsigned char`, `memchr()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_MEMCHR_MEMCHR_01}
 *
 * @design
 * 1. Set `p` to `s`. \llrref{LLR_MEMCHR_MEMCHR_01},
 *    \llrref{LLR_MEMCHR_MEMCHR_02}
 * 2. Set `i` to `0U`; then while `i` is less than `n`, repeat (after each pass:
 *    increment `i`): \llrref{LLR_MEMCHR_MEMCHR_01},
 *    \llrref{LLR_MEMCHR_MEMCHR_02}
 *    1. If `(unsigned char)c` is equal to `p[i]`: \llrref{LLR_MEMCHR_MEMCHR_01}
 *       1. Return `(void *)&p[i]`.
 * 3. Return `NULL`. \llrref{LLR_MEMCHR_MEMCHR_02}
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
