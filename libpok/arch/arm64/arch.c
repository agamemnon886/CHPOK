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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * libpok/arch/x86/arch.c for arm64.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <types.h>

#ifndef POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE

#include <arch.h>

void pok_arch_idle (void)
{
   while (1)
   {
      asm ("nop");
   }
}

#endif

size_t libja_mem_get_alignment(size_t obj_size)
{
   if(obj_size <= 1) {
      return 1;
   }
   else if(obj_size < 4) {
      return 2;
   }
   else if(obj_size < 8) {
      return 4;
   }
   else if(obj_size < 16) {
      return 8;
   }
   else {
      return 16;
   }
}
