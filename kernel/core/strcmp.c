/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: strcmp() and
 * strncmp() share one loop with a single exit and a size_t index.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/strcmp.c
 * (kernel/libc is split up: this file is in the library shared with the
 * hypervisor); moved from common/ to kernel/core, because only the kernel uses
 * it and it depends on the kernel's configuration.
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * strcmp() and strncmp() of the kernel's C library.
 */

#include <config.h>

#if defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_PORTS_QUEUEING) || defined (POK_NEEDS_PORTS_SAMPLING)

#include <libc.h>

/**
 * @funcname `compare`
 *
 * @description
 * Compares two strings, up to a number of characters.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const char *` | [in] The first string. |
 * | `s2` | `const char *` | [in] The second string. |
 * | `size` | `size_t` | [in] The maximum number of characters. |
 *
 * @return 0 if they are equal, -1 if s1 comes first, 1 otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `done` | `bool` | Whether the comparison is over |
 * | `i` | `size_t` | The index of the character |
 * | `result` | `int` | The result |
 *
 * @hlr
 * - \hlrid{HLR_STRCMP_COMPARE_01}: `compare()` shall compare two strings, up to
 *   a number of characters, and give 0 if they are equal, -1 if the first one
 *   comes first and 1 otherwise.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_STRCMP_COMPARE_01}: If, at the first index less than `size` at
 *   which the characters of `s1` and `s2` differ or are both `'\0'`, they
 *   differ and the one of `s1` is less, `compare()` shall return -1.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRCMP_COMPARE_01}
 * - \llrid{LLR_STRCMP_COMPARE_02}: If, at the first index less than `size` at
 *   which the characters of `s1` and `s2` differ or are both `'\0'`, they
 *   differ and the one of `s1` is greater, `compare()` shall return 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRCMP_COMPARE_01}
 * - \llrid{LLR_STRCMP_COMPARE_03}: If, at the first index less than `size` at
 *   which the characters of `s1` and `s2` differ or are both `'\0'`, they are
 *   both `'\0'`, or there is no such index less than `size`, `compare()` shall
 *   return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRCMP_COMPARE_01}
 *
 * @design
 * 1. Set `i` to `0U`. \llrref{LLR_STRCMP_COMPARE_01},
 *    \llrref{LLR_STRCMP_COMPARE_02}, \llrref{LLR_STRCMP_COMPARE_03}
 * 2. Set `result` to `0`. \llrref{LLR_STRCMP_COMPARE_01},
 *    \llrref{LLR_STRCMP_COMPARE_02}, \llrref{LLR_STRCMP_COMPARE_03}
 * 3. Set `done` to `false`. \llrref{LLR_STRCMP_COMPARE_01},
 *    \llrref{LLR_STRCMP_COMPARE_02}, \llrref{LLR_STRCMP_COMPARE_03}
 * 4. While `done` is false and `i` is less than `size`:
 *    \llrref{LLR_STRCMP_COMPARE_01}, \llrref{LLR_STRCMP_COMPARE_02},
 *    \llrref{LLR_STRCMP_COMPARE_03}
 *    1. If `s1[i]` is equal to `'\0'` and `s2[i]` is equal to `'\0'`:
 *       \llrref{LLR_STRCMP_COMPARE_03}
 *       1. Set `done` to `true`.
 *    2. Otherwise, if `s1[i]` is less than `s2[i]`:
 *       \llrref{LLR_STRCMP_COMPARE_01}
 *       1. Set `result` to `-1`.
 *       2. Set `done` to `true`.
 *    3. Otherwise, if `s1[i]` is greater than `s2[i]`:
 *       \llrref{LLR_STRCMP_COMPARE_02}
 *       1. Set `result` to `1`.
 *       2. Set `done` to `true`.
 *    4. Otherwise:
 *       1. Increment `i`.
 * 5. Return `result`. \llrref{LLR_STRCMP_COMPARE_01},
 *    \llrref{LLR_STRCMP_COMPARE_02}, \llrref{LLR_STRCMP_COMPARE_03}
 *
 * @calls
 * None.
 */
static int compare(const char *s1, const char *s2, size_t size)
{
   size_t i = 0U;
   int result = 0;
   bool done = false;

   while ((!done) && (i < size))
   {
      if ((s1[i] == '\0') && (s2[i] == '\0'))
      {
         done = true;
      }
      else if (s1[i] < s2[i])
      {
         result = -1;
         done = true;
      }
      else if (s1[i] > s2[i])
      {
         result = 1;
         done = true;
      }
      else
      {
         i++;
      }
   }
   return result;
}

/**
 * @funcname `strcmp`
 *
 * @description
 * Compares two strings (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const char *` | [in] The first string. |
 * | `s2` | `const char *` | [in] The second string. |
 *
 * @return 0 if they are equal, -1 if s1 comes first, 1 otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_STRCMP_STRCMP_01}: `strcmp()` shall compare two whole strings,
 *   giving 0, -1 or 1 as `compare()` does.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_STRCMP_STRCMP_01}: `strcmp()` shall return the result of
 *   `compare()` with the size `SIZE_MAX`.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRCMP_STRCMP_01}
 *
 * @design
 * 1. Call `compare()` with the arguments `s1`, `s2` and `SIZE_MAX`, and return
 *    its result. \llrref{LLR_STRCMP_STRCMP_01}
 *
 * @calls
 * - `compare()`
 */
/*
 * MISRA C:2025 Dir 1.2 deviation: __attribute__((weak)) is a GNU extension;
 * it lets a board or a configuration provide its own implementation.
 */
__attribute__ ((weak))
int strcmp (const char *s1,
            const char *s2)
{
   return compare(s1, s2, SIZE_MAX);
}

/**
 * @funcname `strncmp`
 *
 * @description
 * Compares two strings, up to a number of characters (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const char *` | [in] The first string. |
 * | `s2` | `const char *` | [in] The second string. |
 * | `size` | `size_t` | [in] The maximum number of characters. |
 *
 * @return 0 if they are equal, -1 if s1 comes first, 1 otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_STRCMP_STRNCMP_01}: `strncmp()` shall compare two strings up to
 *   a number of characters, giving 0, -1 or 1 as `compare()` does.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_STRCMP_STRNCMP_01}: `strncmp()` shall return the result of
 *   `compare()` with `size`.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRCMP_STRNCMP_01}
 *
 * @design
 * 1. Call `compare()` with the arguments `s1`, `s2` and `size`, and return its
 *    result. \llrref{LLR_STRCMP_STRNCMP_01}
 *
 * @calls
 * - `compare()`
 */
__attribute__ ((weak))
int strncmp (const char *s1,
             const char *s2,
             size_t size)
{
   return compare(s1, s2, size);
}

#endif
