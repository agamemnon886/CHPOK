/*
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the copy direction
 * is chosen by comparing the addresses as integers (the pointers may point
 * into different objects), and the copy loops use indexes.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/memmove.c
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
 * memmove() of the kernel's C library.
 */


#include <libc.h>

/**
 * @funcname `memmove`
 *
 * @description
 * Copies a block of memory that may overlap the destination (C standard):
 * forwards if the destination is before the source, backwards otherwise.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dest` | `void *` | [out] The destination. |
 * | `src` | `const void *` | [in] The source. |
 * | `count` | `size_t` | [in] The number of bytes. |
 *
 * @return dest.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `unsigned char *` | The destination, as bytes |
 * | `i` | `size_t` | The index of the byte |
 * | `s` | `const unsigned char *` | The source, as bytes |
 *
 * @hlr
 * - \hlrid{HLR_MEMMOVE_MEMMOVE_01}: `memmove()` shall copy `count` bytes from a
 *   source block to a destination block that may overlap it, so that the
 *   destination gets the bytes that the source had, and give the destination.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_MEMMOVE_MEMMOVE_01}: If the address `dest` is not greater than
 *   the address `src`, `memmove()` shall copy the `count` bytes at `src` to
 *   `dest`, from the first byte on.
 *   <br>Traces to:
 *   - \hlrref{HLR_MEMMOVE_MEMMOVE_01}
 * - \llrid{LLR_MEMMOVE_MEMMOVE_02}: If the address `dest` is greater than the
 *   address `src`, `memmove()` shall copy the `count` bytes at `src` to `dest`,
 *   from the last byte back.
 *   <br>Traces to:
 *   - \hlrref{HLR_MEMMOVE_MEMMOVE_01}
 * - \llrid{LLR_MEMMOVE_MEMMOVE_03}: `memmove()` shall return `dest`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MEMMOVE_MEMMOVE_01}
 *
 * @design
 * 1. Set `d` to `(unsigned char *) dest`. \llrref{LLR_MEMMOVE_MEMMOVE_01},
 *    \llrref{LLR_MEMMOVE_MEMMOVE_02}
 * 2. Set `s` to `(const unsigned char *) src`. \llrref{LLR_MEMMOVE_MEMMOVE_01},
 *    \llrref{LLR_MEMMOVE_MEMMOVE_02}
 * 3. If `(uintptr_t) dest` is less than or equal to `(uintptr_t) src`:
 *    \llrref{LLR_MEMMOVE_MEMMOVE_01}
 *    1. Set `i` to `0U`; then while `i` is less than `count`, repeat (after
 *       each pass: increment `i`):
 *       1. Set `d[i]` to `s[i]`.
 * 4. Otherwise: \llrref{LLR_MEMMOVE_MEMMOVE_02}
 *    1. Set `i` to `count`.
 *    2. While `i` is greater than `0U`:
 *       1. Decrement `i`.
 *       2. Set `d[i]` to `s[i]`.
 * 5. Return `dest`. \llrref{LLR_MEMMOVE_MEMMOVE_03}
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memmove() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
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
