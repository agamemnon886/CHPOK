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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: U suffixes for
 * unsigned constants; the i386 assembly version is removed; the copy loop
 * uses an index.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/memcpy.c
 * (kernel/libc is split up: this file is in the library shared with the
 * hypervisor).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * memcpy() of the kernel's C library.
 */


#include <libc.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memcpy() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */

/**
 * @funcname `memcpy`
 *
 * @description
 * Copies a block of memory; the blocks must not overlap (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `to` | `void *` | [out] The destination. |
 * | `from` | `const void *` | [in] The source. |
 * | `n` | `size_t` | [in] The number of bytes. |
 *
 * @return to.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cfrom` | `const char *` | The source, as bytes |
 * | `cto` | `char *` | The destination, as bytes |
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `cto` to `(char *)to`.
 * 2. Set `cfrom` to `(const char *)from`.
 * 3. Set `i` to `0U`; then while `i` is less than `n`, repeat (after each pass:
 *    increment `i`):
 *    1. Set `cto[i]` to `cfrom[i]`.
 * 4. Return `(to)`.
 *
 * @calls
 * None.
 */
void* memcpy (void*        to,
              const void*  from,
              size_t       n)
{
  char *cto = (char *)to;
  const char *cfrom = (const char *)from;

  for (size_t i = 0U; i < n; i++)
  {
       cto[i] = cfrom[i];
  }
  return (to);
}
