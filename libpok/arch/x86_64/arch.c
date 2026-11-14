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
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * libpok/arch/x86/arch.c to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; U suffixes for unsigned constants; deviations are
 * documented where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * Architecture helpers of libpok on x86_64: the idle loop and the
 * alignment of objects.
 */


#ifndef POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE

#include <arch.h>
#include <asp/alloc.h>

/**
 * @funcname `pok_arch_idle`
 *
 * @description
 * Loops forever (old POK interface).
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
 * 1. While `true` is true:
 *    1. Execute the assembly code `nop`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (pok_arch_idle()): privileged
 * instructions are written as GNU inline assembly, each isolated in a small
 * function.
 */
void pok_arch_idle (void)
{
   while (true)
   {
      asm ("nop");
   }
}

#endif

/**
 * @funcname `libja_mem_get_alignment`
 *
 * @description
 * Returns the alignment for an object of a size: 1, 2, 4, 8 or 16 bytes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `obj_size` | `size_t` | [in] The size. |
 *
 * @return The alignment.
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
 * 1. If `obj_size` is less than or equal to `1U`:
 *    1. Return `1`.
 * 2. Otherwise, if `obj_size` is less than `4U`:
 *    1. Return `2`.
 * 3. Otherwise, if `obj_size` is less than `8U`:
 *    1. Return `4`.
 * 4. Otherwise, if `obj_size` is less than `16U`:
 *    1. Return `8`.
 * 5. Otherwise:
 *    1. Return `16`.
 *
 * @calls
 * None.
 */
size_t libja_mem_get_alignment(size_t obj_size)
{
   if(obj_size <= 1U) {
      return 1;
   }
   else if(obj_size < 4U) {
      return 2;
   }
   else if(obj_size < 8U) {
      return 4;
   }
   else if(obj_size < 16U) {
      return 8;
   }
   else {
      return 16;
   }
}
