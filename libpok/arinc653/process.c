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
 * Modified by Kirill Brazhnikov, 2026-10-31: lock level is passed with
 * the type the kernel expects (64-bit support).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: CREATE_PROCESS checks the
 * BASE_PRIORITY range before narrowing it to the kernel's 8-bit type, so an
 * out-of-range priority returns INVALID_PARAM instead of wrapping around.
 *
 * Modified by Kirill Brazhnikov, 2026-11-01: added the ARINC 653P1-4
 * multicore services INITIALIZE_PROCESS_CORE_AFFINITY,
 * GET_MY_PROCESSOR_CORE_ID and GET_MY_INDEX.
 *
 * Modified by Kirill Brazhnikov, 2026-11-01: SET_PRIORITY of a process
 * that owns a mutex changes only its retained priority (priority ceiling
 * protocol) and rejects process IDs above the number of threads instead of
 * wrapping them to another process; LOCK_PREEMPTION and UNLOCK_PREEMPTION
 * record the holder of the preemption lock for GET_PROCESS_MUTEX_STATE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; operands of the
 * same essential type; deviations are documented where a guideline can't
 * be followed; SET_PRIORITY() of a mutex owner goes through the mutex
 * code (see mutex.c).
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of mixed
 * precedence are parenthesized. Long lines are wrapped at 80 columns. The
 * parameters have the names of the declarations in <arinc653/process.h> (Rule
 * 8.3); the Rule 3.1 comment is removed (MISRA C:2025 allows a double slash in
 * a URI).
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: name
 * parameters are const (they are only read), so callers can pass string
 * literals.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros MAP_ERROR() and
 * MAP_ERROR_DEFAULT() are moved to libpok/include/arinc653/arincutils.h,
 * CHECK_PROCESS_ID() and MAP_STATUS() to libpok/include/arinc653/arincutils.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * ARINC 653 process services: process IDs and status, creation, priority,
 * suspension, start and stop, preemption locking, and the multicore
 * services (ARINC 653P1-4) on JetOS's single core.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_PROCESS

#include <core/dependencies.h>

#include <core/thread.h>
#include <arinc653/arincutils.h>
#include <arinc653/types.h>
#include <arinc653/process.h>
#include <string.h>
#include <utils.h>
#include <core/partition.h>

#include <kernel_shared_data.h>
#include "mutex.h"
#include <core/assert_os.h>

pok_thread_id_t arinc_preemption_lock_holder = JET_THREAD_ID_NONE;

/**
 * @funcname `GET_PROCESS_ID`
 *
 * @description
 * Returns the ID of a process by name (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_NAME` | `const PROCESS_NAME_TYPE` | [in] The name. |
 * | `PROCESS_ID` | `PROCESS_ID_TYPE *` | [out] Where the ID is stored (the kernel thread ID plus 1). |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_CONFIG. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `id` | `pok_thread_id_t` | The kernel thread ID |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_thread_find()` with the arguments `PROCESS_NAME` and the address
 *    of `id`, and store the result in `core_ret`.
 * 2. If `core_ret` is equal to `POK_ERRNO_OK`:
 *    1. Set `*PROCESS_ID` to `(PROCESS_ID_TYPE) id + 1`.
 *    2. Set `*RETURN_CODE` to `NO_ERROR`.
 * 3. Otherwise:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *
 * @calls
 * - `pok_thread_find()`
 */
void GET_PROCESS_ID(
    const PROCESS_NAME_TYPE PROCESS_NAME,
    PROCESS_ID_TYPE   *PROCESS_ID,
    RETURN_CODE_TYPE  *RETURN_CODE)
{
    pok_thread_id_t id;
    pok_ret_e core_ret;

    core_ret = pok_thread_find(PROCESS_NAME, &id);

    if (core_ret == POK_ERRNO_OK) {
        *PROCESS_ID = (PROCESS_ID_TYPE) id + 1;
        *RETURN_CODE = NO_ERROR;
    } else {
        *RETURN_CODE = INVALID_CONFIG;
    }
}

/**
 * @funcname `GET_MY_ID`
 *
 * @description
 * Returns the ID of the current process (ARINC 653 service); the main process
 * and the error handler have none.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE *` | [out] Where the ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_MODE. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `kshd.partition_mode` is not equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 * 2. Otherwise, if `kshd.current_thread_id` is equal to `kshd.error_thread_id`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 * 3. Otherwise:
 *    1. Set `*PROCESS_ID` to `kshd.current_thread_id + 1U`.
 *    2. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * None.
 */
void GET_MY_ID (PROCESS_ID_TYPE   *PROCESS_ID,
		RETURN_CODE_TYPE  *RETURN_CODE )
{
    if(kshd.partition_mode != POK_PARTITION_MODE_NORMAL)
    {
        // Main thread has no id.
        *RETURN_CODE = INVALID_MODE;
    }
    else if(kshd.current_thread_id == kshd.error_thread_id)
    {
        // Error thread has no id.
        *RETURN_CODE = INVALID_MODE;
    }
    else
    {
        *PROCESS_ID = kshd.current_thread_id + 1U;
        *RETURN_CODE = NO_ERROR;
    }
}

/**
 * @funcname `GET_PROCESS_STATUS`
 *
 * @description
 * Returns the status of a process (ARINC 653 service): its attributes, state,
 * current priority and deadline time.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The process. |
 * | `PROCESS_STATUS` | `PROCESS_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `status` | `pok_thread_status_t` | The kernel's status of the thread |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_thread_get_status()` with the arguments `PROCESS_ID - 1`,
 *    `PROCESS_STATUS->ATTRIBUTES.NAME`, the address of
 *    `PROCESS_STATUS->ATTRIBUTES.ENTRY_POINT` and the address of `status`, and
 *    store the result in `core_ret`.
 * 2. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 * 3. If `core_ret` is not equal to `POK_ERRNO_OK`:
 *    1. Return.
 * 4. Set `PROCESS_STATUS->DEADLINE_TIME` to `status.deadline_time`.
 * 5. Depending on `status.state`:
 *    1. Case `MAP_STATUS(POK_STATE_STOPPED, DORMANT)`:
 *       1. Use the macro `MAP_STATUS()` with the arguments `POK_STATE_STOPPED`
 *          and `DORMANT`.
 *       2. Leave the switch.
 *    2. Case `MAP_STATUS(POK_STATE_RUNNABLE, READY)`:
 *       1. Use the macro `MAP_STATUS()` with the arguments `POK_STATE_RUNNABLE`
 *          and `READY`.
 *       2. Leave the switch.
 *    3. Case `MAP_STATUS(POK_STATE_WAITING, WAITING)`:
 *       1. Use the macro `MAP_STATUS()` with the arguments `POK_STATE_WAITING`
 *          and `WAITING`.
 *       2. Leave the switch.
 *    4. Case `MAP_STATUS(POK_STATE_RUNNING, RUNNING)`:
 *       1. Use the macro `MAP_STATUS()` with the arguments `POK_STATE_RUNNING`
 *          and `RUNNING`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Check that `FALSE` holds (assert_os: stop otherwise).
 *       2. Leave the switch.
 * 6. Set `PROCESS_STATUS->ATTRIBUTES.BASE_PRIORITY` to
 *    `status.attributes.priority`.
 * 7. Set `PROCESS_STATUS->ATTRIBUTES.DEADLINE` to `SOFT` if
 *    `status.attributes.deadline` is equal to `DEADLINE_SOFT`, otherwise
 *    `HARD`.
 * 8. Set `PROCESS_STATUS->CURRENT_PRIORITY` to `status.current_priority`.
 * 9. Set `PROCESS_STATUS->ATTRIBUTES.PERIOD` to `status.attributes.period`.
 * 10. Set `PROCESS_STATUS->ATTRIBUTES.TIME_CAPACITY` to
 *     `status.attributes.time_capacity`.
 * 11. Set `PROCESS_STATUS->ATTRIBUTES.STACK_SIZE` to
 *     `status.attributes.stack_size`.
 * 12. Set `PROCESS_STATUS->DEADLINE_TIME` to `status.deadline_time`.
 *
 * @calls
 * - `pok_thread_get_status()`
 * - `assertion_os_fail()`
 */
void GET_PROCESS_STATUS (
    PROCESS_ID_TYPE     PROCESS_ID,
    PROCESS_STATUS_TYPE *PROCESS_STATUS,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    pok_thread_status_t status;
    pok_ret_e           core_ret;

    core_ret = pok_thread_get_status(PROCESS_ID - 1,
        PROCESS_STATUS->ATTRIBUTES.NAME,
        &PROCESS_STATUS->ATTRIBUTES.ENTRY_POINT,
        &status);

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }

    if (core_ret != POK_ERRNO_OK) {
        return;
    }

    PROCESS_STATUS->DEADLINE_TIME = status.deadline_time;
    switch (status.state) {
        MAP_STATUS(POK_STATE_STOPPED, DORMANT);
        MAP_STATUS(POK_STATE_RUNNABLE, READY);
        MAP_STATUS(POK_STATE_WAITING, WAITING);
        MAP_STATUS(POK_STATE_RUNNING, RUNNING);
        default: assert_os(FALSE); break;   // the kernel has no other states
    }
    PROCESS_STATUS->ATTRIBUTES.BASE_PRIORITY = status.attributes.priority;
    PROCESS_STATUS->ATTRIBUTES.DEADLINE =
        (status.attributes.deadline == DEADLINE_SOFT)
            ? SOFT : HARD;
    PROCESS_STATUS->CURRENT_PRIORITY = status.current_priority;
    PROCESS_STATUS->ATTRIBUTES.PERIOD = status.attributes.period;
    PROCESS_STATUS->ATTRIBUTES.TIME_CAPACITY = status.attributes.time_capacity;
    PROCESS_STATUS->ATTRIBUTES.STACK_SIZE = status.attributes.stack_size;
    PROCESS_STATUS->DEADLINE_TIME = status.deadline_time;
}

/**
 * @funcname `CREATE_PROCESS`
 *
 * @description
 * Creates a process (ARINC 653 service): a kernel thread with its attributes;
 * the name must not be empty.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ATTRIBUTES` | `PROCESS_ATTRIBUTE_TYPE *` | [in] The attributes: name, entry point, stack size, base priority, period, time capacity and deadline. |
 * | `PROCESS_ID` | `PROCESS_ID_TYPE *` | [out] Where its ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION (the name exists), INVALID_PARAM, INVALID_CONFIG or INVALID_MODE. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_attr` | `pok_thread_attr_t` | The kernel's attributes |
 * | `core_process_id` | `pok_thread_id_t` | The kernel thread ID |
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `ATTRIBUTES->NAME[0]` is equal to `'\0'`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. If `ATTRIBUTES->BASE_PRIORITY` is less than `MIN_PRIORITY_VALUE` or
 *    `ATTRIBUTES->BASE_PRIORITY` is greater than `MAX_PRIORITY_VALUE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 3. Set `core_attr.priority` to `(uint8_t) ATTRIBUTES->BASE_PRIORITY`.
 * 4. Set `core_attr.period` to `ATTRIBUTES->PERIOD`.
 * 5. If `ATTRIBUTES->DEADLINE` is equal to `SOFT`:
 *    1. Set `core_attr.deadline` to `DEADLINE_SOFT`.
 * 6. Otherwise, if `ATTRIBUTES->DEADLINE` is equal to `HARD`:
 *    1. Set `core_attr.deadline` to `DEADLINE_HARD`.
 * 7. Otherwise:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 8. Set `core_attr.time_capacity` to `ATTRIBUTES->TIME_CAPACITY`.
 * 9. Set `core_attr.stack_size` to `ATTRIBUTES->STACK_SIZE`.
 * 10. Call `pok_thread_create()` with the arguments `ATTRIBUTES->NAME`,
 *     `ATTRIBUTES->ENTRY_POINT`, the address of `core_attr` and the address of
 *     `core_process_id`, and store the result in `core_ret`.
 * 11. Depending on `core_ret`:
 *     1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *        1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *           `NO_ERROR`.
 *        2. Leave the switch.
 *     2. Case `MAP_ERROR(POK_ERRNO_EXISTS, NO_ACTION)`:
 *        1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EXISTS`
 *           and `NO_ACTION`.
 *        2. Leave the switch.
 *     3. Case `MAP_ERROR(POK_ERRNO_PARAM, INVALID_PARAM)`:
 *        1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_PARAM`
 *           and `INVALID_PARAM`.
 *        2. Leave the switch.
 *     4. Case `MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG)`:
 *        1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EINVAL`
 *           and `INVALID_CONFIG`.
 *        2. Leave the switch.
 *     5. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, INVALID_CONFIG)`:
 *        1. Use the macro `MAP_ERROR()` with the arguments
 *           `POK_ERRNO_UNAVAILABLE` and `INVALID_CONFIG`.
 *        2. Leave the switch.
 *     6. Case `MAP_ERROR(POK_ERRNO_TOOMANY, INVALID_CONFIG)`:
 *        1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_TOOMANY`
 *           and `INVALID_CONFIG`.
 *        2. Leave the switch.
 *     7. Case `MAP_ERROR(POK_ERRNO_PARTITION_MODE, INVALID_MODE)`:
 *        1. Use the macro `MAP_ERROR()` with the arguments
 *           `POK_ERRNO_PARTITION_MODE` and `INVALID_MODE`.
 *        2. Leave the switch.
 *     8. Otherwise (default):
 *        1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *           `INVALID_PARAM`.
 *        2. Leave the switch.
 * 12. If `core_ret` is not equal to `POK_ERRNO_OK`:
 *     1. Return.
 * 13. Set `*PROCESS_ID` to `(PROCESS_ID_TYPE) core_process_id + 1`.
 *
 * @calls
 * - `pok_thread_create()`
 */
void CREATE_PROCESS (
    PROCESS_ATTRIBUTE_TYPE  *ATTRIBUTES,
    PROCESS_ID_TYPE         *PROCESS_ID,
    RETURN_CODE_TYPE        *RETURN_CODE)
{
    pok_thread_attr_t core_attr;
    pok_ret_e         core_ret;
    pok_thread_id_t   core_process_id;

    if (ATTRIBUTES->NAME[0] == '\0') {
        // empty names are not allowed
        *RETURN_CODE = INVALID_PARAM;
        return;
    }

    if ((ATTRIBUTES->BASE_PRIORITY < MIN_PRIORITY_VALUE) ||
        (ATTRIBUTES->BASE_PRIORITY > MAX_PRIORITY_VALUE)) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }

    core_attr.priority        = (uint8_t) ATTRIBUTES->BASE_PRIORITY;
    core_attr.period          = ATTRIBUTES->PERIOD;
    if (ATTRIBUTES->DEADLINE == SOFT) {
        core_attr.deadline = DEADLINE_SOFT;
    } else if (ATTRIBUTES->DEADLINE == HARD) {
        core_attr.deadline = DEADLINE_HARD;
    } else {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }
    core_attr.time_capacity   = ATTRIBUTES->TIME_CAPACITY;
    core_attr.stack_size      = ATTRIBUTES->STACK_SIZE;
    core_ret = pok_thread_create (ATTRIBUTES->NAME, ATTRIBUTES->ENTRY_POINT,
        &core_attr, &core_process_id);
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_EXISTS, NO_ACTION);
        MAP_ERROR(POK_ERRNO_PARAM, INVALID_PARAM);
        MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, INVALID_CONFIG);
        MAP_ERROR(POK_ERRNO_TOOMANY, INVALID_CONFIG);
        MAP_ERROR(POK_ERRNO_PARTITION_MODE, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }

    if (core_ret != POK_ERRNO_OK) {
        return;
    }

    *PROCESS_ID = (PROCESS_ID_TYPE) core_process_id + 1;
}

/**
 * @funcname `STOP_SELF`
 *
 * @description
 * Stops the current process (ARINC 653 service).
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
 * 1. Call `pok_thread_stop()`.
 *
 * @calls
 * - `pok_thread_stop()`
 */
void STOP_SELF (void)
{
    (void) pok_thread_stop ();
}


/**
 * @funcname `SET_PRIORITY`
 *
 * @description
 * Sets the current priority of a process (ARINC 653 service); for the owner of
 * a mutex, the priority it gets when it releases the mutex.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The process. |
 * | `priority` | `PRIORITY_TYPE` | [in] The priority. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_MODE (a dormant process) or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
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
 * 1. Use the macro `CHECK_PROCESS_ID()`.
 * 2. Do nothing.
 * 3. If `priority` is less than `MIN_PRIORITY_VALUE` or `priority` is greater
 *    than `MAX_PRIORITY_VALUE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 4. If `PROCESS_ID` is less than `1` or `PROCESS_ID` is greater than
 *    `(PROCESS_ID_TYPE)kshd.max_n_threads`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 5. Call `arinc_mutex_set_priority()` with the arguments `PROCESS_ID - 1` and
 *    `priority`, and store the result in `core_ret`.
 * 6. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `INVALID_MODE`.
 *       2. Leave the switch.
 *    3. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `arinc_mutex_set_priority()`
 */
void SET_PRIORITY (
    PROCESS_ID_TYPE  PROCESS_ID,
    PRIORITY_TYPE    priority,
    RETURN_CODE_TYPE *RETURN_CODE)
{
    CHECK_PROCESS_ID();

    if ((priority < MIN_PRIORITY_VALUE) || (priority > MAX_PRIORITY_VALUE)) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }

    // Check the range before narrowing to the 8-bit kernel thread ID.
    if ((PROCESS_ID < 1)
        || (PROCESS_ID > (PROCESS_ID_TYPE)kshd.max_n_threads)) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }

#ifdef POK_NEEDS_ARINC653_MUTEX
    pok_ret_e core_ret = arinc_mutex_set_priority(PROCESS_ID - 1, priority);
#else
    pok_ret_e core_ret = pok_thread_set_priority(PROCESS_ID - 1, priority);
#endif
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

/**
 * @funcname `SUSPEND_SELF`
 *
 * @description
 * Suspends the current process up to a time-out (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `TIME_OUT` | `SYSTEM_TIME_TYPE` | [in] The time-out. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, TIMED_OUT, INVALID_MODE or INVALID_PARAM. |
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
 * 1. Call `pok_thread_suspend()` with the address of `TIME_OUT`, and store the
 *    result in `core_ret`.
 * 2. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_TIMEOUT, TIMED_OUT)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_TIMEOUT`
 *          and `TIMED_OUT`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_MODE` and
 *          `INVALID_MODE`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_thread_suspend()`
 */
void SUSPEND_SELF (
    SYSTEM_TIME_TYPE TIME_OUT,
    RETURN_CODE_TYPE *RETURN_CODE)
{
    pok_ret_e core_ret = pok_thread_suspend(&TIME_OUT);

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_TIMEOUT, TIMED_OUT);
        MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

/**
 * @funcname `SUSPEND`
 *
 * @description
 * Suspends another process (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The process. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_MODE, NO_ACTION or INVALID_PARAM. |
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
 * 1. Use the macro `CHECK_PROCESS_ID()`.
 * 2. Do nothing.
 * 3. Call `pok_thread_suspend_target()` with the argument `PROCESS_ID - 1`, and
 *    store the result in `core_ret`.
 * 4. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_MODE` and
 *          `INVALID_MODE`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `NO_ACTION`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_thread_suspend_target()`
 */
void SUSPEND (
    PROCESS_ID_TYPE     PROCESS_ID,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    CHECK_PROCESS_ID();

    pok_ret_e core_ret = pok_thread_suspend_target(PROCESS_ID - 1);
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

/**
 * @funcname `RESUME`
 *
 * @description
 * Resumes a suspended process (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The process. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION, INVALID_MODE or INVALID_PARAM. |
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
 * 1. Use the macro `CHECK_PROCESS_ID()`.
 * 2. Do nothing.
 * 3. Call `pok_thread_resume()` with the argument `PROCESS_ID - 1`, and store
 *    the result in `core_ret`.
 * 4. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_MODE` and
 *          `INVALID_MODE`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_thread_resume()`
 */
void RESUME (
    PROCESS_ID_TYPE     PROCESS_ID,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    CHECK_PROCESS_ID();

    pok_ret_e core_ret = pok_thread_resume(PROCESS_ID - 1);
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

/**
 * @funcname `START`
 *
 * @description
 * Starts a dormant process (ARINC 653 service): DELAYED_START with no delay.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The process. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored (see DELAYED_START()). |
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
 * 1. Call `DELAYED_START()` with the arguments `PROCESS_ID`, `0` and
 *    `RETURN_CODE`.
 *
 * @calls
 * - `DELAYED_START()`
 */
void START (
    PROCESS_ID_TYPE     PROCESS_ID,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    DELAYED_START(PROCESS_ID, 0, RETURN_CODE);
}

/**
 * @funcname `STOP`
 *
 * @description
 * Stops another process (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The process. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION or INVALID_PARAM. |
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
 * 1. Use the macro `CHECK_PROCESS_ID()`.
 * 2. Do nothing.
 * 3. Call `pok_thread_stop_target()` with the argument `PROCESS_ID - 1`, and
 *    store the result in `core_ret`.
 * 4. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_thread_stop_target()`
 */
void STOP(
    PROCESS_ID_TYPE     PROCESS_ID,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    CHECK_PROCESS_ID();

    pok_ret_e core_ret = pok_thread_stop_target(PROCESS_ID - 1);
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

/**
 * @funcname `DELAYED_START`
 *
 * @description
 * Starts a dormant process after a delay (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The process. |
 * | `DELAY_TIME` | `SYSTEM_TIME_TYPE` | [in] The delay. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION or INVALID_PARAM. |
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
 * 1. Use the macro `CHECK_PROCESS_ID()`.
 * 2. Do nothing.
 * 3. Call `pok_thread_delayed_start()` with the arguments `PROCESS_ID - 1` and
 *    the address of `DELAY_TIME`, and store the result in `core_ret`.
 * 4. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_thread_delayed_start()`
 */
void DELAYED_START(
    PROCESS_ID_TYPE   PROCESS_ID,
    SYSTEM_TIME_TYPE  DELAY_TIME,
    RETURN_CODE_TYPE *RETURN_CODE)
{
    CHECK_PROCESS_ID();

    pok_ret_e core_ret = pok_thread_delayed_start(PROCESS_ID - 1, &DELAY_TIME);
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

/**
 * @funcname `LOCK_PREEMPTION`
 *
 * @description
 * Increases the preemption lock level of the partition (ARINC 653 service); the
 * current process holds the lock.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `LOCK_LEVEL` | `LOCK_LEVEL_TYPE *` | [in,out] Where the new lock level is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION (not in NORMAL mode), INVALID_CONFIG (MAX_LOCK_LEVEL reached) or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_preemption_lock_holder` | `pok_thread_id_t` | The process that holds the preemption lock, or JET_THREAD_ID_NONE |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lock_level` | `int32_t` | The lock level |
 * | `ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `lock_level` to `(int32_t)*LOCK_LEVEL`.
 * 2. Call `pok_current_partition_inc_lock_level()` with the address of
 *    `lock_level`, and store the result in `ret`.
 * 3. Set `*LOCK_LEVEL` to `lock_level`.
 * 4. If `ret` is equal to `POK_ERRNO_OK`:
 *    1. Set `arinc_preemption_lock_holder` to `kshd.current_thread_id`.
 * 5. Depending on `ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_PARTITION_MODE, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_PARTITION_MODE` and `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EINVAL`
 *          and `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_current_partition_inc_lock_level()`
 */
void LOCK_PREEMPTION (LOCK_LEVEL_TYPE *LOCK_LEVEL,
                      RETURN_CODE_TYPE *RETURN_CODE)
{
    // LOCK_LEVEL_TYPE may be wider than int32_t (e.g. on arm64).
    int32_t lock_level = (int32_t)*LOCK_LEVEL;
    pok_ret_e ret = pok_partition_inc_lock_level(&lock_level);
    *LOCK_LEVEL = lock_level;
    if (ret == POK_ERRNO_OK) {
        arinc_preemption_lock_holder = kshd.current_thread_id;
    }
    switch (ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_PARTITION_MODE, NO_ACTION);
        MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG); // yes, it's an error here...
        MAP_ERROR_DEFAULT(INVALID_PARAM); // shouldn't happen
    }
}

/**
 * @funcname `UNLOCK_PREEMPTION`
 *
 * @description
 * Decreases the preemption lock level of the partition (ARINC 653 service); at
 * 0 no process holds the lock.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `LOCK_LEVEL` | `LOCK_LEVEL_TYPE *` | [in,out] Where the new lock level is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_preemption_lock_holder` | `pok_thread_id_t` | The process that holds the preemption lock, or JET_THREAD_ID_NONE |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lock_level` | `int32_t` | The lock level |
 * | `ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `lock_level` to `(int32_t)*LOCK_LEVEL`.
 * 2. Call `pok_current_partition_dec_lock_level()` with the address of
 *    `lock_level`, and store the result in `ret`.
 * 3. Set `*LOCK_LEVEL` to `lock_level`.
 * 4. If `ret` is equal to `POK_ERRNO_OK` and `lock_level` is equal to `0`:
 *    1. Set `arinc_preemption_lock_holder` to `JET_THREAD_ID_NONE`.
 * 5. Depending on `ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_PARTITION_MODE, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_PARTITION_MODE` and `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_EINVAL, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EINVAL`
 *          and `NO_ACTION`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_current_partition_dec_lock_level()`
 */
void UNLOCK_PREEMPTION (LOCK_LEVEL_TYPE *LOCK_LEVEL,
                        RETURN_CODE_TYPE *RETURN_CODE)
{
    // LOCK_LEVEL_TYPE may be wider than int32_t (e.g. on arm64).
    int32_t lock_level = (int32_t)*LOCK_LEVEL;
    pok_ret_e ret = pok_partition_dec_lock_level(&lock_level);
    *LOCK_LEVEL = lock_level;
    if ((ret == POK_ERRNO_OK) && (lock_level == 0)) {
        arinc_preemption_lock_holder = JET_THREAD_ID_NONE;
    }
    switch (ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_PARTITION_MODE, NO_ACTION);
        MAP_ERROR(POK_ERRNO_EINVAL, NO_ACTION); // ...but here it's just NO_ACTION
        MAP_ERROR_DEFAULT(INVALID_PARAM); // shouldn't happen
    }
}

/*
 * Multicore services (ARINC 653P1-4).
 *
 * JetOS runs every partition on a single processor core, with
 * partition-relative core ID 0. All processes run on that core, so a core
 * affinity can't change anything and isn't stored.
 */

/**
 * @funcname `INITIALIZE_PROCESS_CORE_AFFINITY`
 *
 * @description
 * Sets the core affinity of a created process (ARINC 653P1-4 multicore
 * service), not in NORMAL mode. JetOS runs every partition on a single
 * processor core, with partition-relative core ID 0: all processes run on it,
 * so the affinity can't change anything and isn't stored.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The process. |
 * | `PROCESSOR_CORE_ID` | `PROCESSOR_CORE_ID_TYPE` | [in] The core: 0 or CORE_AFFINITY_NO_PREFERENCE. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, INVALID_CONFIG or INVALID_MODE. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `entry` | `void *` | Its entry (not used) |
 * | `name` | `char[30]` | The thread's name (not used) |
 * | `status` | `pok_thread_status_t` | Its status (not used) |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `PROCESS_ID` is less than `1` or `PROCESS_ID` is greater than
 *    `(PROCESS_ID_TYPE)kshd.max_n_threads` or
 *    `pok_thread_get_status(PROCESS_ID - 1, name, &entry, &status)` is not
 *    equal to `POK_ERRNO_OK`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. If `PROCESSOR_CORE_ID` is not equal to `CORE_AFFINITY_NO_PREFERENCE` and
 *    `PROCESSOR_CORE_ID` is not equal to `0`:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 3. If `kshd.partition_mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 *    2. Return.
 * 4. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `pok_thread_get_status()`
 */
void INITIALIZE_PROCESS_CORE_AFFINITY (
    PROCESS_ID_TYPE        PROCESS_ID,
    PROCESSOR_CORE_ID_TYPE PROCESSOR_CORE_ID,
    RETURN_CODE_TYPE       *RETURN_CODE)
{
    char name[MAX_NAME_LENGTH];
    void *entry;
    pok_thread_status_t status;

    // Check the range before narrowing to the kernel's 8-bit thread ID.
    // The kernel rejects the main process, the error handler and
    // processes that aren't created yet.
    if ((PROCESS_ID < 1) || (PROCESS_ID > (PROCESS_ID_TYPE)kshd.max_n_threads)
        || (pok_thread_get_status(PROCESS_ID - 1, name, &entry, &status)
           != POK_ERRNO_OK)) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }

    if ((PROCESSOR_CORE_ID != CORE_AFFINITY_NO_PREFERENCE)
        && (PROCESSOR_CORE_ID != 0)) {
        *RETURN_CODE = INVALID_CONFIG;
        return;
    }

    if (kshd.partition_mode == POK_PARTITION_MODE_NORMAL) {
        *RETURN_CODE = INVALID_MODE;
        return;
    }

    *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `GET_MY_PROCESSOR_CORE_ID`
 *
 * @description
 * Returns the processor core of the current process (ARINC 653P1-4 multicore
 * service): always 0.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESSOR_CORE_ID` | `PROCESSOR_CORE_ID_TYPE *` | [out] Where the core ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR. |
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
 * 1. Set `*PROCESSOR_CORE_ID` to `0`.
 * 2. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * None.
 */
void GET_MY_PROCESSOR_CORE_ID (
    PROCESSOR_CORE_ID_TYPE *PROCESSOR_CORE_ID,
    RETURN_CODE_TYPE       *RETURN_CODE)
{
    *PROCESSOR_CORE_ID = 0;
    *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `GET_MY_INDEX`
 *
 * @description
 * Returns the index of the current process (ARINC 653P1-4): its position in
 * creation order, 1 for the first process created by CREATE_PROCESS, up to the
 * number of processes in the configuration. Unlike PROCESS_ID_TYPE, it can be
 * used directly as an array index (minus one); the main process and the error
 * handler have none.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_INDEX` | `PROCESS_INDEX_TYPE *` | [out] Where the index is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_MODE. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current` | `pok_thread_id_t` | The current thread |
 * | `index` | `PROCESS_INDEX_TYPE` | The index |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `current` to `kshd.current_thread_id`.
 * 2. If `kshd.partition_mode` is not equal to `POK_PARTITION_MODE_NORMAL` or
 *    `current` is equal to `kshd.error_thread_id`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 *    2. Return.
 * 3. Set `index` to `current`.
 * 4. If `kshd.error_thread_id` is not equal to `JET_THREAD_ID_NONE` and
 *    `kshd.error_thread_id` is less than `current`:
 *    1. Decrement `index`.
 * 5. Set `*PROCESS_INDEX` to `index`.
 * 6. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * None.
 */
void GET_MY_INDEX (
    PROCESS_INDEX_TYPE *PROCESS_INDEX,
    RETURN_CODE_TYPE   *RETURN_CODE)
{
    pok_thread_id_t current = kshd.current_thread_id;
    PROCESS_INDEX_TYPE index;

    // Like GET_MY_ID: the main process (runs only during initialization)
    // and the error handler have no index.
    if ((kshd.partition_mode != POK_PARTITION_MODE_NORMAL)
        || (current == kshd.error_thread_id)) {
        *RETURN_CODE = INVALID_MODE;
        return;
    }

    // Kernel thread 0 is the main process; the error handler, if created
    // before this process, takes one more thread slot.
    index = current;
    if ((kshd.error_thread_id != JET_THREAD_ID_NONE)
        && (kshd.error_thread_id < current)) {
        index--;
    }

    *PROCESS_INDEX = index;
    *RETURN_CODE = NO_ERROR;
}

#endif
