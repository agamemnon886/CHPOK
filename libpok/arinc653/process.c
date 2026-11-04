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

/*
 * Each MAP_ERROR()/MAP_ERROR_DEFAULT() expands to a whole switch-clause ending
 * with break (MISRA C:2025 Rule 16.3); cppcheck's MISRA addon doesn't see the
 * break inside the macro and reports these switches by mistake.
 */
#define MAP_ERROR(from, to) case (from): *RETURN_CODE = (to); break
#define MAP_ERROR_DEFAULT(to) default: *RETURN_CODE = (to); break

#define CHECK_PROCESS_ID() \
    if (PROCESS_ID == 0) { \
        *RETURN_CODE = INVALID_PARAM; \
        return; \
    }

void GET_PROCESS_ID(
    const PROCESS_NAME_TYPE PROCESS_NAME,
    PROCESS_ID_TYPE   *PROCESS_ID,
    RETURN_CODE_TYPE  *RETURN_CODE)
{
    pok_thread_id_t id;
    pok_ret_t core_ret;

    core_ret = pok_thread_find(PROCESS_NAME, &id);

    if (core_ret == POK_ERRNO_OK) {
        *PROCESS_ID = (PROCESS_ID_TYPE) id + 1;
        *RETURN_CODE = NO_ERROR;
    } else {
        *RETURN_CODE = INVALID_CONFIG;
    }
}

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

void GET_PROCESS_STATUS (
    PROCESS_ID_TYPE     PROCESS_ID,
    PROCESS_STATUS_TYPE *PROCESS_STATUS,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    pok_thread_status_t status;
    pok_ret_t           core_ret;

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
#define MAP_STATUS(from, to) \
    case (from): PROCESS_STATUS->PROCESS_STATE = (to); break
    switch (status.state) {
        MAP_STATUS(POK_STATE_STOPPED, DORMANT);
        MAP_STATUS(POK_STATE_RUNNABLE, READY);
        MAP_STATUS(POK_STATE_WAITING, WAITING);
        MAP_STATUS(POK_STATE_RUNNING, RUNNING);
        default: assert_os(FALSE); break;   // the kernel has no other states
    }
#undef MAP_STATUS
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

void CREATE_PROCESS (
    PROCESS_ATTRIBUTE_TYPE  *ATTRIBUTES,
    PROCESS_ID_TYPE         *PROCESS_ID,
    RETURN_CODE_TYPE        *RETURN_CODE)
{
    pok_thread_attr_t core_attr;
    pok_ret_t         core_ret;
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

void STOP_SELF (void)
{
    (void) pok_thread_stop ();
}


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
    pok_ret_t core_ret = arinc_mutex_set_priority(PROCESS_ID - 1, priority);
#else
    pok_ret_t core_ret = pok_thread_set_priority(PROCESS_ID - 1, priority);
#endif
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

void SUSPEND_SELF (
    SYSTEM_TIME_TYPE TIME_OUT,
    RETURN_CODE_TYPE *RETURN_CODE)
{
    pok_ret_t core_ret = pok_thread_suspend(&TIME_OUT);

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_TIMEOUT, TIMED_OUT);
        MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

void SUSPEND (
    PROCESS_ID_TYPE     PROCESS_ID,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    CHECK_PROCESS_ID();

    pok_ret_t core_ret = pok_thread_suspend_target(PROCESS_ID - 1);
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

void RESUME (
    PROCESS_ID_TYPE     PROCESS_ID,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    CHECK_PROCESS_ID();

    pok_ret_t core_ret = pok_thread_resume(PROCESS_ID - 1);
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

void START (
    PROCESS_ID_TYPE     PROCESS_ID,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    DELAYED_START(PROCESS_ID, 0, RETURN_CODE);
}

void STOP(
    PROCESS_ID_TYPE     PROCESS_ID,
    RETURN_CODE_TYPE    *RETURN_CODE)
{
    CHECK_PROCESS_ID();

    pok_ret_t core_ret = pok_thread_stop_target(PROCESS_ID - 1);
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

void DELAYED_START(
    PROCESS_ID_TYPE   PROCESS_ID,
    SYSTEM_TIME_TYPE  DELAY_TIME,
    RETURN_CODE_TYPE *RETURN_CODE)
{
    CHECK_PROCESS_ID();

    pok_ret_t core_ret = pok_thread_delayed_start(PROCESS_ID - 1, &DELAY_TIME);
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

void LOCK_PREEMPTION (LOCK_LEVEL_TYPE *LOCK_LEVEL,
                      RETURN_CODE_TYPE *RETURN_CODE)
{
    // LOCK_LEVEL_TYPE may be wider than int32_t (e.g. on arm64).
    int32_t lock_level = (int32_t)*LOCK_LEVEL;
    pok_ret_t ret = pok_partition_inc_lock_level(&lock_level);
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

void UNLOCK_PREEMPTION (LOCK_LEVEL_TYPE *LOCK_LEVEL,
                        RETURN_CODE_TYPE *RETURN_CODE)
{
    // LOCK_LEVEL_TYPE may be wider than int32_t (e.g. on arm64).
    int32_t lock_level = (int32_t)*LOCK_LEVEL;
    pok_ret_t ret = pok_partition_dec_lock_level(&lock_level);
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

void GET_MY_PROCESSOR_CORE_ID (
    PROCESSOR_CORE_ID_TYPE *PROCESSOR_CORE_ID,
    RETURN_CODE_TYPE       *RETURN_CODE)
{
    *PROCESSOR_CORE_ID = 0;
    *RETURN_CODE = NO_ERROR;
}

/*
 * The process index is the position of the process in creation order:
 * 1 for the first process created by CREATE_PROCESS, up to the number of
 * processes in the configuration (Threads Count). Unlike PROCESS_ID_TYPE,
 * it can be used directly as an array index (minus one) for per-process
 * data.
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
