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
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/strrchr.c
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
 * strrchr() of the kernel's C library.
 */

#include <libc.h>
/**
 * @funcname `strrchr`
 *
 * @description
 * Finds the last occurrence of a character in a string (C standard).
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
 * | `done` | `bool` | Whether the end of the string was reached |
 * | `i` | `size_t` | The index of the character |
 * | `last` | `const char *` | The last occurrence so far |
 *
 * @hlr
 * - \hlrid{HLR_STRRCHR_STRRCHR_01}: `strrchr()` shall give the address of the
 *   last occurrence of a character in a string, the NUL included, or NULL if it
 *   is not there.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_STRRCHR_STRRCHR_01}: If `c` converted to `char` is one of the
 *   characters of `s` up to and including its `'\0'`, `strrchr()` shall return
 *   the address of the last such character.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRRCHR_STRRCHR_01}
 * - \llrid{LLR_STRRCHR_STRRCHR_02}: If `c` converted to `char` is neither
 *   `'\0'` nor one of the characters of `s` before its `'\0'`, `strrchr()`
 *   shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRRCHR_STRRCHR_01}
 *
 * @design
 * 1. Set `last` to `NULL`. \llrref{LLR_STRRCHR_STRRCHR_01},
 *    \llrref{LLR_STRRCHR_STRRCHR_02}
 * 2. Set `i` to `0U`. \llrref{LLR_STRRCHR_STRRCHR_01},
 *    \llrref{LLR_STRRCHR_STRRCHR_02}
 * 3. Set `done` to `false`. \llrref{LLR_STRRCHR_STRRCHR_01},
 *    \llrref{LLR_STRRCHR_STRRCHR_02}
 * 4. While `done` is false: \llrref{LLR_STRRCHR_STRRCHR_01},
 *    \llrref{LLR_STRRCHR_STRRCHR_02}
 *    1. If `s[i]` is equal to `(char)c`: \llrref{LLR_STRRCHR_STRRCHR_01}
 *       1. Set `last` to the address of `s[i]`.
 *    2. Set `done` to `(s[i] == '\0')`.
 *    3. Increment `i`.
 * 5. Return `(char *)last`. \llrref{LLR_STRRCHR_STRRCHR_01},
 *    \llrref{LLR_STRRCHR_STRRCHR_02}
 *
 * @calls
 * None.
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
