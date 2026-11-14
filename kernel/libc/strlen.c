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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: strlen() and
 * strnlen() return size_t, as in the C standard; strncpy() fills the rest of
 * the n bytes with null characters, as in the C standard (it wrote one null
 * character only).
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * strlen(), strnlen() and strncpy() of the kernel's C library.
 */

#include <config.h>
#include <libc.h>

#ifdef POK_NEEDS_DEBUG

/**
 * @funcname `strlen`
 *
 * @description
 * Returns the length of a string (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `str` | `const char *` | [in] The string. |
 *
 * @return The number of characters before the NUL.
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
 * 2. While `str[i]` is not equal to `'\0'`:
 *    1. Increment `i`.
 * 3. Return `i`.
 *
 * @calls
 * None.
 */
size_t strlen (const char* str)
{
   size_t i = 0U;

   while (str[i] != '\0')
   {
      i++;
   }

   return i;
}

// TODO: move into own file?

/**
 * @funcname `strnlen`
 *
 * @description
 * Returns the length of a string, at most a maximum (POSIX).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `str` | `const char *` | [in] The string. |
 * | `n` | `size_t` | [in] The maximum. |
 *
 * @return The number of characters before the NUL, or n.
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
 * 2. While `i` is less than `n` and `str[i]` is not equal to `'\0'`:
 *    1. Increment `i`.
 * 3. Return `i`.
 *
 * @calls
 * None.
 */
size_t strnlen (const char* str, size_t n)
{
   size_t i = 0U;

   while ((i < n) && (str[i] != '\0'))
   {
      i++;
   }

   return i;
}

// TODO: move into own file?

/**
 * @funcname `strncpy`
 *
 * @description
 * Copies a string into a buffer of n bytes, filling the rest with NUL
 * characters (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dest` | `char *` | [out] The buffer. |
 * | `src` | `const char *` | [in] The string. |
 * | `n` | `size_t` | [in] The size of the buffer. |
 *
 * @return dest.
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
 * 2. While `i` is less than `n` and `src[i]` is not equal to `'\0'`:
 *    1. Set `dest[i]` to `src[i]`.
 *    2. Increment `i`.
 * 3. While `i` is less than `n`:
 *    1. Set `dest[i]` to `'\0'`.
 *    2. Increment `i`.
 * 4. Return `dest`.
 *
 * @calls
 * None.
 */
char* strncpy(char* dest, const char* src, size_t n)
{
   size_t i = 0U;

   while ((i < n) && (src[i] != '\0')) {
      dest[i] = src[i];
      i++;
   }

   // The rest of the n bytes are filled with null characters.
   while (i < n) {
      dest[i] = '\0';
      i++;
   }

   return dest;
}

#endif
