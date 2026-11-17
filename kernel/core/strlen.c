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
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/strlen.c
 * (kernel/libc is split up: this file is in the library shared with the
 * hypervisor); moved from common/ to kernel/core, because only the kernel uses
 * it and it depends on the kernel's configuration.
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
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
 * - \hlrid{HLR_STRLEN_STRLEN_01}: `strlen()` shall give the number of
 *   characters of a string before its NUL.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_STRLEN_STRLEN_01}: `strlen()` shall count the characters from
 *   the first one until the first `'\0'` and return the count.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRLEN_STRLEN_01}
 *
 * @design
 * 1. Set `i` to `0U`. \llrref{LLR_STRLEN_STRLEN_01}
 * 2. While `str[i]` is not equal to `'\0'`: \llrref{LLR_STRLEN_STRLEN_01}
 *    1. Increment `i`.
 * 3. Return `i`. \llrref{LLR_STRLEN_STRLEN_01}
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
 * - \hlrid{HLR_STRLEN_STRNLEN_01}: `strnlen()` shall give the number of
 *   characters of a string before its NUL, at most a maximum `n`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_STRLEN_STRNLEN_01}: `strnlen()` shall count the characters until
 *   the first `'\0'` or until the count reaches `n`, and return the count.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRLEN_STRNLEN_01}
 *
 * @design
 * 1. Set `i` to `0U`. \llrref{LLR_STRLEN_STRNLEN_01}
 * 2. While `i` is less than `n` and `str[i]` is not equal to `'\0'`:
 *    \llrref{LLR_STRLEN_STRNLEN_01}
 *    1. Increment `i`.
 * 3. Return `i`. \llrref{LLR_STRLEN_STRNLEN_01}
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
 * - \hlrid{HLR_STRLEN_STRNCPY_01}: `strncpy()` shall copy a string into a
 *   buffer of `n` bytes, without its NUL if it does not fit, fill the rest of
 *   the buffer with NUL characters, and give the buffer.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_001}
 *
 * @llr
 * - \llrid{LLR_STRLEN_STRNCPY_01}: `strncpy()` shall copy the characters of
 *   `src` until its `'\0'` or `n` characters, then write `'\0'` to the
 *   remaining bytes of the `n`, and return `dest`.
 *   <br>Traces to:
 *   - \hlrref{HLR_STRLEN_STRNCPY_01}
 *
 * @design
 * 1. Set `i` to `0U`. \llrref{LLR_STRLEN_STRNCPY_01}
 * 2. While `i` is less than `n` and `src[i]` is not equal to `'\0'`:
 *    \llrref{LLR_STRLEN_STRNCPY_01}
 *    1. Set `dest[i]` to `src[i]`.
 *    2. Increment `i`.
 * 3. While `i` is less than `n`: \llrref{LLR_STRLEN_STRNCPY_01}
 *    1. Set `dest[i]` to `'\0'`.
 *    2. Increment `i`.
 * 4. Return `dest`. \llrref{LLR_STRLEN_STRNCPY_01}
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
