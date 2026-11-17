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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
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
 * - \hlrid{HLR_TIME_JET_ON_TICK_01}: `jet_on_tick()` shall make the scheduler
 *   evaluate its decisions again at each tick of the timer of the board.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *
 * @llr
 * - \llrid{LLR_TIME_JET_ON_TICK_01}: `jet_on_tick()` shall call
 *   `pok_sched_on_time_changed()` once for each call.
 *   <br>Traces to:
 *   - \hlrref{HLR_TIME_JET_ON_TICK_01}
 *
 * @design
 * 1. Call `pok_sched_on_time_changed()`. \llrref{LLR_TIME_JET_ON_TICK_01}
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
 * - \hlrid{HLR_TIME_POK_CLOCK_GETTIME_01}: `pok_clock_gettime()` shall store
 *   the system time at the partition address `val` for the clock
 *   `CLOCK_REALTIME` and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_002}
 * - \hlrid{HLR_TIME_POK_CLOCK_GETTIME_02}: `pok_clock_gettime()` shall refuse
 *   any other clock with `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_007}
 * - \hlrid{HLR_TIME_POK_CLOCK_GETTIME_03}: `pok_clock_gettime()` shall refuse
 *   an address `val` that the current partition may not write with
 *   `POK_ERRNO_EFAULT`, without writing to memory.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *   - \srqref{SRQ_JET_MEM_017}
 *
 * @llr
 * - \llrid{LLR_TIME_POK_CLOCK_GETTIME_01}: If `jet_user_to_kernel_typed()`
 *   returns NULL for `val`, `pok_clock_gettime()` shall return
 *   `POK_ERRNO_EFAULT` without writing.
 *   <br>Traces to:
 *   - \hlrref{HLR_TIME_POK_CLOCK_GETTIME_03}
 * - \llrid{LLR_TIME_POK_CLOCK_GETTIME_02}: If `jet_user_to_kernel_typed()`
 *   returns an address for `val` and `clk_id` is `CLOCK_REALTIME`,
 *   `pok_clock_gettime()` shall write the result of `jet_system_time()` at that
 *   address and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_TIME_POK_CLOCK_GETTIME_01}
 * - \llrid{LLR_TIME_POK_CLOCK_GETTIME_03}: If `jet_user_to_kernel_typed()`
 *   returns an address for `val` and `clk_id` is not `CLOCK_REALTIME`,
 *   `pok_clock_gettime()` shall write -1 at that address and return
 *   `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_TIME_POK_CLOCK_GETTIME_02}
 *
 * @design
 * 1. Set `k_val` to `jet_user_to_kernel_typed(val)`.
 *    \llrref{LLR_TIME_POK_CLOCK_GETTIME_01},
 *    \llrref{LLR_TIME_POK_CLOCK_GETTIME_02},
 *    \llrref{LLR_TIME_POK_CLOCK_GETTIME_03}
 * 2. If `k_val` is NULL: \llrref{LLR_TIME_POK_CLOCK_GETTIME_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Depending on `clk_id`: \llrref{LLR_TIME_POK_CLOCK_GETTIME_02},
 *    \llrref{LLR_TIME_POK_CLOCK_GETTIME_03}
 *    1. Case `CLOCK_REALTIME`: \llrref{LLR_TIME_POK_CLOCK_GETTIME_02}
 *       1. Set `*k_val` to `jet_system_time()`.
 *       2. Leave the switch.
 *    2. Otherwise (default): \llrref{LLR_TIME_POK_CLOCK_GETTIME_03}
 *       1. Set `*k_val` to `-1`.
 *       2. Return `POK_ERRNO_EINVAL`.
 * 4. Return `POK_ERRNO_OK`. \llrref{LLR_TIME_POK_CLOCK_GETTIME_02}
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
 * - \hlrid{HLR_TIME_JET_TIME_01}: `jet_time()` shall store the calendar time at
 *   the partition address `val` and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_003}
 * - \hlrid{HLR_TIME_JET_TIME_02}: `jet_time()` shall refuse an address `val`
 *   that the current partition may not write with `POK_ERRNO_EFAULT`, without
 *   writing to memory.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *   - \srqref{SRQ_JET_MEM_017}
 *
 * @llr
 * - \llrid{LLR_TIME_JET_TIME_01}: If `jet_user_to_kernel_typed()` returns NULL
 *   for `val`, `jet_time()` shall return `POK_ERRNO_EFAULT` without calling
 *   `jet_calendar_time()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_TIME_JET_TIME_02}
 * - \llrid{LLR_TIME_JET_TIME_02}: If `jet_user_to_kernel_typed()` returns an
 *   address for `val`, `jet_time()` shall write the result of
 *   `jet_calendar_time()` at that address and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_TIME_JET_TIME_01}
 *
 * @design
 * 1. Set `k_val` to `jet_user_to_kernel_typed(val)`.
 *    \llrref{LLR_TIME_JET_TIME_01}, \llrref{LLR_TIME_JET_TIME_02}
 * 2. If `k_val` is NULL: \llrref{LLR_TIME_JET_TIME_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `*k_val` to `jet_calendar_time()`. \llrref{LLR_TIME_JET_TIME_02}
 * 4. Return `POK_ERRNO_OK`. \llrref{LLR_TIME_JET_TIME_02}
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
