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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 */

#include <config.h>

#include <types.h>
#include <errno.h>

#include <core/time.h>
#include <core/uaccess.h>
#include <core/sched.h>

#include <asp/entries.h> /* jet_on_tick() declaration. */

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the jet_user_to_kernel*()
 * helpers return the kernel address of user memory as a pointer to void,
 * which is converted to the type of the object the user passed.
 */

void jet_on_tick(void)
{
    pok_sched_on_time_changed();
}

#ifdef POK_NEEDS_GETTICK
/**
 * Get the current ticks value, store it in
 * \a clk_val
 * Returns POK_ERRNO_OK
 * Need the GETTICK service (POK_NEEDS_GETTICKS maccro)
 */
pok_ret_t   pok_clock_gettime (clockid_t clk_id, pok_time_t* JET_USER val)
{
   pok_time_t* JET_KUSER k_val = jet_user_to_kernel_typed(val);
   if(k_val == NULL) {
      return POK_ERRNO_EFAULT;
   }
   
   switch(clk_id) {
      case CLOCK_REALTIME:
         *k_val = jet_system_time();
         break;
      default:
         *k_val = -1; // Not supported
         return POK_ERRNO_EINVAL;
   }

   return POK_ERRNO_OK;
}
#endif

pok_ret_t   jet_time(time_t* JET_USER val)
{
   time_t* JET_KUSER k_val = jet_user_to_kernel_typed(val);
   if(k_val == NULL) {
      return POK_ERRNO_EFAULT;
   }

   *k_val =  jet_calendar_time();

   return POK_ERRNO_OK;
}
