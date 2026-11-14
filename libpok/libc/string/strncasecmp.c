/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized.
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
 * strncasecmp() of libpok (weak: may be replaced).
 */
 
#include <core/dependencies.h>
#include <string.h>

#include <ctype.h>

#ifdef POK_CONFIG_NEEDS_FUNC_STRNCASECMP

/**
 * @funcname `strncasecmp`
 *
 * @description
 * Compares two strings without regard to case, up to n characters (POSIX).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s1` | `const char *` | [in] The first string. |
 * | `s2` | `const char *` | [in] The second string. |
 * | `n` | `size_t` | [in] The maximum number of characters. |
 *
 * @return 0 if they are equal, otherwise the difference of the first characters
 *         that differ, in lower case (as POSIX.1-2008 requires).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `diff` | `int` | The difference of the characters |
 * | `i` | `unsigned int` | The index of the character |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0`; then while `i` is less than `n`, repeat (after each pass:
 *    increment `i`):
 *    1. If `s1[i]` is equal to `'\0'` and `s2[i]` is equal to `'\0'`:
 *       1. Return `0`.
 *    2. Set `diff` to `tolower(s1[i]) - tolower(s2[i])`.
 *    3. If `diff` is not equal to `0`:
 *       1. Return `diff`.
 * 2. Return `0`.
 *
 * @calls
 * - `tolower()`
 */
__attribute__ ((weak))
int strncasecmp(const char *s1, const char *s2, size_t n)
{
  unsigned int i;
  for (i = 0; i < n; i++)
  { 
      if ((s1[i] == '\0') && (s2[i] == '\0')) {
          return 0;
      }
      
      int diff = tolower(s1[i]) - tolower(s2[i]); // As POSIX.1-2008 requires.
      
      if(diff != 0) {
        return diff;
      }
  }
  return 0;
}

#endif


