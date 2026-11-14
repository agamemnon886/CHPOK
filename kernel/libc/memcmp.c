/*  
 *  Copyright (C) 2014 Maxim Malkov, ISPRAS <malkov@ispras.ru> 
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * memcmp() of the kernel's C library.
 */

#include <libc.h>

/**
 * @funcname `memcmp`
 *
 * @description
 * Compares two blocks of memory (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `v1` | `const void *` | [in] The first block. |
 * | `v2` | `const void *` | [in] The second block. |
 * | `n` | `size_t` | [in] The number of bytes. |
 *
 * @return 0 if they are equal, otherwise the difference of the first bytes that
 *         differ.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `diff` | `int` | The difference of the bytes |
 * | `i` | `size_t` | The index of the byte |
 * | `s1` | `const unsigned char *` | The first block, as bytes |
 * | `s2` | `const unsigned char *` | The second block, as bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `s1` to `v1`.
 * 2. Set `s2` to `v2`.
 * 3. Set `i` to `0`; then while `i` is less than `n`, repeat (after each pass:
 *    increment `i`):
 *    1. Set `diff` to `s1[i] - s2[i]`.
 *    2. If `diff` is not equal to `0`:
 *       1. Return `diff`.
 * 4. Return `0`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memcmp() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */
int memcmp (const void* v1, const void* v2, size_t n)
{
   const unsigned char *s1 = v1;
   const unsigned char *s2 = v2;
   size_t  i;

   for (i = 0; i < n; i++) {
      int diff = s1[i] - s2[i];
      if (diff != 0) {
         return diff;
      }
   }
   return 0;
}
