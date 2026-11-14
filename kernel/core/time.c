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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * The kernel's time services: the timer tick, and the partitions' clock and
 * calendar time syscalls.
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

/**
 * @funcname `jet_on_tick`
 *
 * @description
 * Called by the board's timer interrupt at each tick: lets the scheduler see
 * that the time changed.
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
 * 1. Call `pok_sched_on_time_changed()`.
 *
 * @calls
 * - `pok_sched_on_time_changed()`
 */
void jet_on_tick(void)
{
    pok_sched_on_time_changed();
}

#ifdef POK_NEEDS_GETTICK
/**
 * @funcname `pok_clock_gettime`
 *
 * @description
 * Stores the current time of a clock at a partition address (the clock_gettime
 * syscall). Only CLOCK_REALTIME is supported; it is the system time.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `clk_id` | `clockid_t` | [in] The clock. |
 * | `val` | `pok_time_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK, POK_ERRNO_EFAULT (val is not a partition address) or
 *         POK_ERRNO_EINVAL (another clock).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_val` | `pok_time_t *` | Kernel address of val, or NULL |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `k_val` to `jet_user_to_kernel_typed(val)`.
 * 2. If `k_val` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Depending on `clk_id`:
 *    1. Case `CLOCK_REALTIME`:
 *       1. Set `*k_val` to `jet_system_time()`.
 *       2. Leave the switch.
 *    2. Otherwise (default):
 *       1. Set `*k_val` to `-1`.
 *       2. Return `POK_ERRNO_EINVAL`.
 * 4. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `ja_system_time()`
 */
pok_ret_e   pok_clock_gettime (clockid_t clk_id, pok_time_t* JET_USER val)
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

/**
 * @funcname `jet_time`
 *
 * @description
 * Stores the calendar time at a partition address (the time syscall).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `val` | `time_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK, or POK_ERRNO_EFAULT if val is not a partition address.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_val` | `time_t *` | Kernel address of val, or NULL |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `k_val` to `jet_user_to_kernel_typed(val)`.
 * 2. If `k_val` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `*k_val` to `jet_calendar_time()`.
 * 4. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `ja_calendar_time()`
 */
pok_ret_e   jet_time(time_t* JET_USER val)
{
   time_t* JET_KUSER k_val = jet_user_to_kernel_typed(val);
   if(k_val == NULL) {
      return POK_ERRNO_EFAULT;
   }

   *k_val =  jet_calendar_time();

   return POK_ERRNO_OK;
}
