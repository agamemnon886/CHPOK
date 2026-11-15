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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: memset() takes an
 * int value, as in the C standard; the loop doesn't modify the parameter.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/memset.c
 * (kernel/libc is split up: this file is in the library shared with the
 * hypervisor); it is shared with the hypervisor, and declared in
 * include/common/string.h.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * memset() of the library that the kernel and the hypervisor share (weak:
 * an architecture may have its own).
 */


#include <common/string.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memset() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */

/**
 * @funcname `memset`
 *
 * @description
 * Sets a block of memory to a byte value (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dest` | `void *` | [out] The memory. |
 * | `val` | `int` | [in] The value (converted to unsigned char). |
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
 * | `d` | `unsigned char *` | The memory, as bytes |
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `d` to `(unsigned char *) dest`.
 * 2. Set `i` to `0U`; then while `i` is less than `count`, repeat (after each
 *    pass: increment `i`):
 *    1. Set `d[i]` to `(unsigned char) val`.
 * 3. Return `dest`.
 *
 * @calls
 * None.
 */
__attribute__ ((weak))
void* memset (void *dest, int val, size_t count)
{
  unsigned char *d = (unsigned char *) dest;

  for (size_t i = 0U; i < count; i++)
  {
    d[i] = (unsigned char) val;
  }

  return dest;
}

