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
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/strchr.c
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
 * - \hlrid{HLR_STRCHR_STRCHR_01}: `strchr()` shall give the address of the
 *   first occurrence of a character in a string, the NUL included, or NULL if
 *   it is not there.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_STRCHR_STRCHR_01}: If `c` converted to `char` is one of the
 *   characters of `s` up to and including its `'\0'`, `strchr()` shall return
 *   the address of the first such character.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRCHR_STRCHR_01}
 * - \llrid{LLR_STRCHR_STRCHR_02}: If `c` converted to `char` is neither `'\0'`
 *   nor one of the characters of `s` before its `'\0'`, `strchr()` shall return
 *   NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRCHR_STRCHR_01}
 *
 * @design
 * 1. Set `i` to `0U`. \llrref{LLR_STRCHR_STRCHR_01},
 *    \llrref{LLR_STRCHR_STRCHR_02}
 * 2. While `s[i]` is not equal to `(char)c`: \llrref{LLR_STRCHR_STRCHR_01},
 *    \llrref{LLR_STRCHR_STRCHR_02}
 *    1. If `s[i]` is equal to `'\0'`: \llrref{LLR_STRCHR_STRCHR_02}
 *       1. Return `NULL`.
 *    2. Increment `i`. \llrref{LLR_STRCHR_STRCHR_01}
 * 3. Return `(char *)&s[i]`. \llrref{LLR_STRCHR_STRCHR_01}
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
