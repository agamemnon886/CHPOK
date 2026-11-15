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
 * hypervisor).
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
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`.
 * 2. Set `result` to `0`.
 * 3. Set `done` to `false`.
 * 4. While `done` is false and `i` is less than `size`:
 *    1. If `s1[i]` is equal to `'\0'` and `s2[i]` is equal to `'\0'`:
 *       1. Set `done` to `true`.
 *    2. Otherwise, if `s1[i]` is less than `s2[i]`:
 *       1. Set `result` to `-1`.
 *       2. Set `done` to `true`.
 *    3. Otherwise, if `s1[i]` is greater than `s2[i]`:
 *       1. Set `result` to `1`.
 *       2. Set `done` to `true`.
 *    4. Otherwise:
 *       1. Increment `i`.
 * 5. Return `result`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `compare()` with the arguments `s1`, `s2` and `SIZE_MAX`, and return
 *    its result.
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
 *
 * @llr
 *
 * @design
 * 1. Call `compare()` with the arguments `s1`, `s2` and `size`, and return its
 *    result.
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
