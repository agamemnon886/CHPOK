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
 * This file also incorporates work covered by the following
 * copyright and license notice:
 *
 *  Copyright (C) 2013-2014 Maxim Malkov, ISPRAS <malkov@ispras.ru>
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Created by julien on Thu Jan 15 23:34:13 2009
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns. The Rule 3.1 comment is removed (MISRA C:2025 allows a double slash
 * in a URI).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros are moved to
 * libpok/include/arinc653/arincutils.h, as MAP_TIME_ERROR() and
 * MAP_TIME_ERROR_DEFAULT().
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * ARINC 653 time services: TIMED_WAIT, PERIODIC_WAIT, GET_TIME and
 * REPLENISH.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_TIME

#include <arinc653/types.h>
#include <arinc653/time.h>

#include <core/thread.h>
#include <core/time.h>
#include <types.h>
#include <arinc653/arincutils.h>

/**
 * @funcname `TIMED_WAIT`
 *
 * @description
 * Suspends the current process for a delay (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `delay_time` | `SYSTEM_TIME_TYPE` | [in] The delay (0: yield). |
 * | `return_code` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_MODE or INVALID_PARAM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `delay_time` is less than `0`:
 *    1. Set `*return_code` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Call `pok_thread_sleep()` with the address of `delay_time`, and store the
 *    result in `core_ret`.
 * 3. Depending on `core_ret`:
 *    1. Case `MAP_TIME_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_TIME_ERROR()` with the arguments `POK_ERRNO_OK`
 *          and `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_TIME_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_TIME_ERROR()` with the arguments `POK_ERRNO_MODE`
 *          and `INVALID_MODE`.
 *       2. Leave the switch.
 *    3. Otherwise (default):
 *       1. Use the macro `MAP_TIME_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_thread_sleep()`
 */
void TIMED_WAIT (SYSTEM_TIME_TYPE delay_time, RETURN_CODE_TYPE *return_code)
{
   if (delay_time < 0) {
       // arinc doesn't allow infinite sleep
       *return_code = INVALID_PARAM;
       return;
   }

   pok_ret_e core_ret;
   core_ret = pok_thread_sleep(&delay_time);

   switch (core_ret) {
      MAP_TIME_ERROR(POK_ERRNO_OK, NO_ERROR);
      MAP_TIME_ERROR(POK_ERRNO_MODE, INVALID_MODE);
      MAP_TIME_ERROR_DEFAULT(INVALID_PARAM);
   }
}

/**
 * @funcname `PERIODIC_WAIT`
 *
 * @description
 * Suspends the current periodic process until its next release point (ARINC 653
 * service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `return_code` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_MODE or INVALID_PARAM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_sched_end_period()`, and store the result in `core_ret`.
 * 2. Depending on `core_ret`:
 *    1. Case `MAP_TIME_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_TIME_ERROR()` with the arguments `POK_ERRNO_OK`
 *          and `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_TIME_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_TIME_ERROR()` with the arguments `POK_ERRNO_MODE`
 *          and `INVALID_MODE`.
 *       2. Leave the switch.
 *    3. Otherwise (default):
 *       1. Use the macro `MAP_TIME_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_sched_end_period()`
 */
void PERIODIC_WAIT (RETURN_CODE_TYPE *return_code)
{
   pok_ret_e core_ret;
   core_ret = pok_thread_period ();
   switch (core_ret) {
      MAP_TIME_ERROR(POK_ERRNO_OK, NO_ERROR);
      MAP_TIME_ERROR(POK_ERRNO_MODE, INVALID_MODE);
      MAP_TIME_ERROR_DEFAULT(INVALID_PARAM);
   }
}

/**
 * @funcname `GET_TIME`
 *
 * @description
 * Returns the system time (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `system_time` | `SYSTEM_TIME_TYPE *` | [out] Where the time is stored. |
 * | `return_code` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR. |
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
 * 1. Call `pok_time_get()`, and store the result in `*system_time`.
 * 2. Set `*return_code` to `NO_ERROR`.
 *
 * @calls
 * - `pok_time_get()`
 */
void GET_TIME (SYSTEM_TIME_TYPE *system_time, RETURN_CODE_TYPE *return_code)
{
   *system_time = pok_time_get();
   *return_code = NO_ERROR;
}

/**
 * @funcname `REPLENISH`
 *
 * @description
 * Postpones the deadline of the current process by a budget time (ARINC 653
 * service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `budget_time` | `SYSTEM_TIME_TYPE` | [in] The budget time. |
 * | `return_code` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION, INVALID_MODE or INVALID_PARAM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_sched_replenish()` with the address of `budget_time`, and store
 *    the result in `core_ret`.
 * 2. Depending on `core_ret`:
 *    1. Case `MAP_TIME_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_TIME_ERROR()` with the arguments `POK_ERRNO_OK`
 *          and `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_TIME_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION)`:
 *       1. Use the macro `MAP_TIME_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Case `MAP_TIME_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_TIME_ERROR()` with the arguments `POK_ERRNO_MODE`
 *          and `INVALID_MODE`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_TIME_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_sched_replenish()`
 */
void REPLENISH (SYSTEM_TIME_TYPE budget_time, RETURN_CODE_TYPE *return_code)
{
    pok_ret_e core_ret = pok_thread_replenish(&budget_time);

    switch (core_ret) {
        MAP_TIME_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_TIME_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_TIME_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_TIME_ERROR_DEFAULT(INVALID_PARAM);
    }
}

#endif
