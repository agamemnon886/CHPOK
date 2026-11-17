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
 * Modified by Kirill Brazhnikov, 2026-10-31: pok_thread_create() rejects
 * a period that is not positive or not a multiple of the partition
 * period (INVALID_CONFIG for ARINC CREATE_PROCESS), a negative time
 * capacity, and reports an out-of-range priority as a parameter error
 * (INVALID_PARAM).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: the period is no longer
 * required to be a multiple of the partition period, which is always the
 * major frame here and so wrong for partitions with several windows per
 * frame (it rejected processes of existing benchmarks).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * discarded results are cast to void; if-else-if chains end with else;
 * deviations are documented where a guideline can't be followed. The time
 * parameters of pok_thread_sleep(), pok_thread_sleep_until() and
 * pok_thread_suspend() are renamed, so that they don't hide libpok's time().
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: one declarator per
 * declaration; increments and assignments are separate statements; operands of
 * mixed precedence are parenthesized; deviations are documented where a
 * guideline can't be followed; pok_thread_wq_add_prio() finds the insertion
 * point with a while loop that doesn't change the list. Long lines are wrapped
 * at 80 columns. Times from the user are added with pok_time_add(), which can't
 * overflow; pok_thread_sleep_until() compiles again; the msection wait queue
 * checks use assert_os().
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: jet_thread_shared_data_t, list_head_t, msection_t,
 * msection_wq_t.
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
 * empty branches) is left out of the design; requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
 */

/**
 * @file
 * The ARINC 653 process (thread) services of the kernel: create, start,
 * stop, suspend and resume threads, priorities, periodic waits and
 * replenish, the msections (critical sections shared with user space) with
 * their wait queues, and the thread wait queues of the ports.
 */

#include <config.h>

#include <types.h>

#include <core/debug.h>
#include <core/error.h>
#include <core/thread.h>
#include <core/sched_arinc.h>
#include <core/partition.h>
#include <core/time.h>
#include <libc.h>

#include <core/instrumentation.h>

#include "thread_internal.h"
#include <core/uaccess.h>

#include <system_limits.h>
#include <core/syscall.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the jet_user_to_kernel*()
 * helpers return the kernel address of user memory as a pointer to void,
 * which is converted to the type of the object the user passed.
 * MISRA C:2025 Rule 18.4 deviation (this file): thread and object IDs are
 * the index of an element, computed as the difference between its address
 * and the start of its array; the end of the thread array is its start plus
 * the number of threads used.
 */

/*
 * MISRA C:2025 Rule 15.1 deviation (this file): forward gotos to out
 * (and to suspend_timed) leave a function through its common exit path,
 * which restores the scheduler state in one place.
 */


/**
 * @funcname `find_thread`
 *
 * @description
 * Finds a thread of the current partition by name (case insensitive); the main
 * and error threads aren't searchable.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char[MAX_NAME_LENGTH]` | [in] The name, in kernel memory. |
 *
 * @return The thread, or NULL.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `t` | `pok_thread_t *` | The thread looked at |
 * | `t_end` | `pok_thread_t *` | The end of the threads used |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_FIND_THREAD_01}: `find_thread()` shall find a created
 *   process of the current partition by its name, without regard to case.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_003}
 *   - \srqref{SRQ_JET_CFG_004}
 * - \hlrid{HLR_THREAD_FIND_THREAD_02}: `find_thread()` shall not find the main
 *   process and the error handler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_015}
 *
 * @llr
 * - \llrid{LLR_THREAD_FIND_THREAD_01}: If `pok_compare_names()` returns 0 for
 *   `name` of one of the threads from `POK_PARTITION_ARINC_MAIN_THREAD_ID` plus
 *   1 below `nthreads_used` of the partition and `name`, `find_thread()` shall
 *   return the address of the first such thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_FIND_THREAD_01}
 *   - \hlrref{HLR_THREAD_FIND_THREAD_02}
 * - \llrid{LLR_THREAD_FIND_THREAD_02}: If `pok_compare_names()` returns a value
 *   other than 0 for each of these threads, `find_thread()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_FIND_THREAD_01}
 * - \llrid{LLR_THREAD_FIND_THREAD_03}: With `POK_NEEDS_ERROR_HANDLING`,
 *   `find_thread()` shall not compare the name of `thread_error` of the
 *   partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_FIND_THREAD_02}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_FIND_THREAD_01}, \llrref{LLR_THREAD_FIND_THREAD_02}
 * 2. Set `t_end` to `part->threads + part->nthreads_used`.
 *    \llrref{LLR_THREAD_FIND_THREAD_01}, \llrref{LLR_THREAD_FIND_THREAD_02}
 * 3. Set `t` to the address of
 *    `part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID + 1U]`; then while `t`
 *    is not equal to `t_end`, repeat (after each pass: increment `t`):
 *    \llrref{LLR_THREAD_FIND_THREAD_01}, \llrref{LLR_THREAD_FIND_THREAD_02}
 *    1. If `part->thread_error` is equal to `t`:
 *       \llrref{LLR_THREAD_FIND_THREAD_03}
 *       1. Go on with the next pass of the loop.
 *    2. If `pok_compare_names(t->name, name)` is equal to `0`:
 *       \llrref{LLR_THREAD_FIND_THREAD_01}
 *       1. Return `t`.
 * 4. Return `NULL`. \llrref{LLR_THREAD_FIND_THREAD_02}
 *
 * @calls
 * - `pok_compare_names()`
 */
static pok_thread_t* find_thread(const char name[MAX_NAME_LENGTH])
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_thread_t* t;
    pok_thread_t* t_end = part->threads + part->nthreads_used;

    for(t = &part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID + 1U];
        t != t_end;
        t++)
    {
#ifdef POK_NEEDS_ERROR_HANDLING
		if(part->thread_error == t) {
		    continue; /* error thread is not searchable. */
		}
#endif
        if(pok_compare_names(t->name, name) == 0) {
            return t;
        }
    }

    return NULL;
}

/**
 * @funcname `get_thread_by_id`
 *
 * @description
 * Returns a thread of the current partition by its ID.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_thread_id_t` | [in] The thread ID. |
 *
 * @return The thread, or NULL for the main thread, the error thread or a thread
 *         not created.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_GET_THREAD_BY_ID_01}: `get_thread_by_id()` shall give the
 *   created process of the current partition that has an identifier, and none
 *   for the main process, the error handler or an identifier of no created
 *   process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_003}
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_GET_THREAD_BY_ID_01}: If `id` is 0, or is not less than
 *   `nthreads_used` of the partition, or (with `POK_NEEDS_ERROR_HANDLING`)
 *   `threads[id]` is `thread_error`, `get_thread_by_id()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_GET_THREAD_BY_ID_01}
 * - \llrid{LLR_THREAD_GET_THREAD_BY_ID_02}: If `id` is not 0, is less than
 *   `nthreads_used` and `threads[id]` is not `thread_error`,
 *   `get_thread_by_id()` shall return the address of `threads[id]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_GET_THREAD_BY_ID_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_GET_THREAD_BY_ID_01},
 *    \llrref{LLR_THREAD_GET_THREAD_BY_ID_02}
 * 2. If `(id == 0U) || (id >= part->nthreads_used) #ifdef`
 *    `POK_NEEDS_ERROR_HANDLING || (part->thread_error == &part->threads[id])`
 *    is true: \llrref{LLR_THREAD_GET_THREAD_BY_ID_01},
 *    \llrref{LLR_THREAD_GET_THREAD_BY_ID_02}
 *    1. Return `NULL`. \llrref{LLR_THREAD_GET_THREAD_BY_ID_01}
 * 3. Return the address of `part->threads[id]`.
 *    \llrref{LLR_THREAD_GET_THREAD_BY_ID_02}
 *
 * @calls
 * None.
 */
static pok_thread_t* get_thread_by_id(pok_thread_id_t id)
{
	pok_partition_arinc_t* part = current_partition_arinc;

	if((id == 0U) /* main thread have no id*/
		|| (id >= part->nthreads_used) /* thread is not created yet */
#ifdef POK_NEEDS_ERROR_HANDLING
		/* error thread has no id */
		|| (part->thread_error == &part->threads[id])
#endif
        ) {
        return NULL;
    }

	return &part->threads[id];
}


/**
 * @funcname `pok_thread_create`
 *
 * @description
 * Creates a thread (ARINC 653 CREATE_PROCESS, only outside NORMAL mode): checks
 * the attributes, the name and the entry point, initializes the thread and
 * returns its ID.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char * JET_USER` | [in] Partition address of the name. |
 * | `entry` | `void * JET_USER` | [in] The entry point. |
 * | `attr` | `const pok_thread_attr_t * JET_USER` | [in] Partition address of the attributes. |
 * | `thread_id` | `pok_thread_id_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARTITION_MODE in NORMAL mode;
 *         POK_ERRNO_EFAULT for a bad address; POK_ERRNO_TOOMANY with no thread
 *         left; POK_ERRNO_PARAM for a bad priority, time capacity or entry;
 *         POK_ERRNO_EINVAL for a bad period; POK_ERRNO_EXISTS for a name in
 *         use; POK_ERRNO_UNAVAILABLE if the stack can't be allocated.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_attr` | `const pok_thread_attr_t *` | Kernel address of the attributes |
 * | `k_name` | `const char *` | Kernel address of the name |
 * | `k_thread_id` | `pok_thread_id_t *` | Kernel address of the result |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `t` | `pok_thread_t *` | The new thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_CREATE_01}: `pok_thread_create()` shall create
 *   a process of the current partition with the given name, entry point,
 *   priority, period, time capacity, deadline and stack size, and give its
 *   identifier, only while the partition is not in `NORMAL` mode.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_001}
 * - \hlrid{HLR_THREAD_POK_THREAD_CREATE_02}: `pok_thread_create()` shall refuse
 *   a priority outside 1 to 239, a period or a time capacity that is not
 *   positive and not infinite, a periodic process with an infinite time
 *   capacity or one greater than its period, an entry point outside the code of
 *   the partition, and a name that another process has.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_014}
 *   - \srqref{SRQ_JET_THR_002}
 *   - \srqref{SRQ_JET_MEM_007}
 * - \hlrid{HLR_THREAD_POK_THREAD_CREATE_03}: `pok_thread_create()` shall refuse
 *   addresses that the partition may not access, and fail when no process slot
 *   or no stack is left.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *   - \srqref{SRQ_JET_MEM_008}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_01}: If `mode` of the partition is
 *   `POK_PARTITION_MODE_NORMAL`, `pok_thread_create()` shall return
 *   `POK_ERRNO_PARTITION_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_01}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_02}: If `mode` is not
 *   `POK_PARTITION_MODE_NORMAL` and `jet_user_to_kernel_typed_ro()` returns
 *   NULL for `attr`, `pok_thread_create()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_03}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_03}: If `attr` is readable and
 *   `jet_user_to_kernel_typed()` returns NULL for `thread_id`,
 *   `pok_thread_create()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_03}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_04}: If `thread_id` is writable and
 *   `jet_user_to_kernel_ro()` returns NULL for `name` and `MAX_NAME_LENGTH`,
 *   `pok_thread_create()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_03}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_05}: If these conversions give
 *   addresses and `nthreads_used` of the partition is `nthreads`,
 *   `pok_thread_create()` shall return `POK_ERRNO_TOOMANY`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_03}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_06}: If `nthreads_used` is less than
 *   `nthreads`, `pok_thread_create()` shall set, in `threads[nthreads_used]`,
 *   `entry` to `entry`, and `base_priority`, `period`, `time_capacity` and
 *   `deadline` to `priority`, `period`, `time_capacity` and `deadline` of
 *   `attr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_02}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_07}: If `base_priority` is then greater
 *   than `MAX_PRIORITY_VALUE` or less than `MIN_PRIORITY_VALUE`,
 *   `pok_thread_create()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_02}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_08}: If the priority is valid, and
 *   `period` is not `POK_TIME_INFINITY` and not greater than 0,
 *   `pok_thread_create()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_02}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_09}: If the period is valid, and
 *   `time_capacity` is not `POK_TIME_INFINITY` and not greater than 0,
 *   `pok_thread_create()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_02}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_10}: If these checks pass,
 *   `pok_time_is_infinity()` returns false for `period` and true for
 *   `time_capacity`, `pok_thread_create()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_02}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_11}: If these checks pass, `period` and
 *   `time_capacity` are finite and `time_capacity` is greater than `period`,
 *   `pok_thread_create()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_02}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_12}: If these checks pass and
 *   `jet_check_access_exec()` returns false for `entry`, `pok_thread_create()`
 *   shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_02}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_13}: If `jet_check_access_exec()`
 *   returns true, `pok_thread_create()` shall copy the `MAX_NAME_LENGTH` bytes
 *   of the name to `name` of the thread with `memcpy()`, and return
 *   `POK_ERRNO_EXISTS` if `find_thread()` returns a thread for it.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_02}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_14}: If `find_thread()` returns NULL,
 *   `pok_thread_create()` shall set `user_stack_size` of the thread to
 *   `stack_size` of `attr` and call `thread_create()` once with the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_01}
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_03}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_15}: If `find_thread()` returns NULL
 *   and `thread_create()` returns false, `pok_thread_create()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_03}
 * - \llrid{LLR_THREAD_POK_THREAD_CREATE_16}: If `thread_create()` returns true,
 *   `pok_thread_create()` shall store `nthreads_used` at `thread_id`, increment
 *   `nthreads_used` and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_CREATE_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_POK_THREAD_CREATE_06}
 * 2. If `part->mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    \llrref{LLR_THREAD_POK_THREAD_CREATE_01}
 *    1. Return `POK_ERRNO_PARTITION_MODE`.
 * 3. Set `k_attr` to `jet_user_to_kernel_typed_ro(attr)`.
 *    \llrref{LLR_THREAD_POK_THREAD_CREATE_02}
 * 4. If `k_attr` is NULL: \llrref{LLR_THREAD_POK_THREAD_CREATE_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `k_thread_id` to `jet_user_to_kernel_typed(thread_id)`.
 *    \llrref{LLR_THREAD_POK_THREAD_CREATE_03}
 * 6. If `k_thread_id` is NULL: \llrref{LLR_THREAD_POK_THREAD_CREATE_03}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 7. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 *    \llrref{LLR_THREAD_POK_THREAD_CREATE_04}
 * 8. If `k_name` is NULL: \llrref{LLR_THREAD_POK_THREAD_CREATE_04}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 9. If `part->nthreads_used` is equal to `part->nthreads`:
 *    \llrref{LLR_THREAD_POK_THREAD_CREATE_05}
 *    1. Return `POK_ERRNO_TOOMANY`.
 * 10. Set `t` to the address of `part->threads[part->nthreads_used]`.
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_06}
 * 11. Set `t->entry` to `entry`. \llrref{LLR_THREAD_POK_THREAD_CREATE_06}
 * 12. Set `t->base_priority` to `k_attr->priority`.
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_06}
 * 13. Set `t->period` to `k_attr->period`.
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_06}
 * 14. Set `t->time_capacity` to `k_attr->time_capacity`.
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_06}
 * 15. Set `t->deadline` to `k_attr->deadline`.
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_06}
 * 16. If `t->base_priority` is greater than `MAX_PRIORITY_VALUE` or
 *     `t->base_priority` is less than `MIN_PRIORITY_VALUE`:
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_07}
 *     1. Return `POK_ERRNO_PARAM`.
 * 17. If `t->period` is not equal to `POK_TIME_INFINITY` and `t->period` is
 *     less than or equal to `0`: \llrref{LLR_THREAD_POK_THREAD_CREATE_08}
 *     1. Return `POK_ERRNO_EINVAL`.
 * 18. If `t->time_capacity` is not equal to `POK_TIME_INFINITY` and
 *     `t->time_capacity` is less than or equal to `0`:
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_09}
 *     1. Return `POK_ERRNO_PARAM`.
 * 19. If `pok_time_is_infinity(t->period)` is false:
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_10},
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_11}
 *     1. If `pok_time_is_infinity(t->time_capacity)` is true:
 *        \llrref{LLR_THREAD_POK_THREAD_CREATE_10}
 *        1. Return `POK_ERRNO_PARAM`.
 *     2. If `t->time_capacity` is greater than `t->period`:
 *        \llrref{LLR_THREAD_POK_THREAD_CREATE_11}
 *        1. Return `POK_ERRNO_PARAM`.
 * 20. If `jet_check_access_exec(t->entry)` is false:
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_12}
 *     1. Return `POK_ERRNO_PARAM`.
 * 21. Call `memcpy()` with the arguments `t->name`, `k_name` and
 *     `MAX_NAME_LENGTH`. \llrref{LLR_THREAD_POK_THREAD_CREATE_13}
 * 22. If `find_thread(t->name)` is not NULL:
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_13}
 *     1. Return `POK_ERRNO_EXISTS`.
 * 23. Set `t->user_stack_size` to `k_attr->stack_size`.
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_14}
 * 24. If `thread_create(t)` is false: \llrref{LLR_THREAD_POK_THREAD_CREATE_14},
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_15},
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_16}
 *     1. Return `POK_ERRNO_UNAVAILABLE`.
 *        \llrref{LLR_THREAD_POK_THREAD_CREATE_15}
 * 25. Set `*k_thread_id` to `part->nthreads_used`.
 *     \llrref{LLR_THREAD_POK_THREAD_CREATE_16}
 * 26. Increment `part->nthreads_used`. \llrref{LLR_THREAD_POK_THREAD_CREATE_16}
 * 27. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_THREAD_CREATE_16}
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_user_to_kernel()`
 * - `pok_time_is_infinity()`
 * - `jet_check_access_exec()`
 * - `memcpy()`
 * - `find_thread()`
 * - `thread_create()`
 */
pok_ret_e pok_thread_create (const char* JET_USER name,
    void* JET_USER                    entry,
    const pok_thread_attr_t* JET_USER attr,
    pok_thread_id_t* JET_USER         thread_id)
{
    pok_thread_t* t;
    pok_partition_arinc_t* part = current_partition_arinc;

    /*
     * We can create a thread only if the partition is in INIT mode
     */
    if (part->mode == POK_PARTITION_MODE_NORMAL) {
        return POK_ERRNO_PARTITION_MODE;
    }

    const pok_thread_attr_t* JET_KUSER k_attr =
        jet_user_to_kernel_typed_ro(attr);
    if(k_attr == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_thread_id_t* JET_KUSER k_thread_id =
        jet_user_to_kernel_typed(thread_id);
    if(k_thread_id == NULL) {
        return POK_ERRNO_EFAULT;
    }

    const char* JET_KUSER k_name = jet_user_to_kernel_ro(name, MAX_NAME_LENGTH);
    if(k_name == NULL) {
        return POK_ERRNO_EFAULT;
    }

    if (part->nthreads_used == part->nthreads) {
        return POK_ERRNO_TOOMANY;
    }

    t = &part->threads[part->nthreads_used];

    t->entry = entry;
    t->base_priority = k_attr->priority;
    t->period = k_attr->period;
    t->time_capacity = k_attr->time_capacity;
    t->deadline = k_attr->deadline;

    /*
     * Error codes follow ARINC CREATE_PROCESS: POK_ERRNO_PARAM becomes
     * INVALID_PARAM (priority, time capacity) and POK_ERRNO_EINVAL becomes
     * INVALID_CONFIG (period inconsistent with the configuration).
     */
    if ((t->base_priority > MAX_PRIORITY_VALUE) ||
        (t->base_priority < MIN_PRIORITY_VALUE)) {
        return POK_ERRNO_PARAM;
    }

    /*
     * INFINITE_TIME_VALUE (-1) is the only valid non-positive period or
     * time capacity. pok_time_is_infinity() treats every negative value as
     * infinite, so compare with POK_TIME_INFINITY explicitly.
     */
    if ((t->period != POK_TIME_INFINITY) && (t->period <= 0)) {
        return POK_ERRNO_EINVAL;
    }
    if ((t->time_capacity != POK_TIME_INFINITY) && (t->time_capacity <= 0)) {
        return POK_ERRNO_PARAM;
    }

    if(!pok_time_is_infinity(t->period))
    {
        if(pok_time_is_infinity(t->time_capacity)) {
            // periodic process must have definite time capacity
            return POK_ERRNO_PARAM;
        }

        if(t->time_capacity > t->period) {
            // for periodic process, time capacity <= period
            return POK_ERRNO_PARAM;
        }

        /*
         * The period is not checked against the partition period: the
         * configuration cannot set a partition period (base_part.period is
         * always the major frame), and a partition with several windows per
         * major frame has a shorter real period.
         */
   }

    // do at least basic check of entry point
    if (!jet_check_access_exec(t->entry)) {
        return POK_ERRNO_PARAM;
    }

    (void) memcpy(t->name, k_name, MAX_NAME_LENGTH);

    if(find_thread(t->name) != NULL) {
        return POK_ERRNO_EXISTS;
    }

    t->user_stack_size = k_attr->stack_size;

    if(!thread_create(t)) {
        return POK_ERRNO_UNAVAILABLE;
    }

    *k_thread_id = part->nthreads_used;

    part->nthreads_used++;

    return POK_ERRNO_OK;
}

#ifdef POK_NEEDS_THREAD_SLEEP

/**
 * @funcname `pok_thread_sleep`
 *
 * @description
 * Makes the current thread sleep for a duration (TIMED_WAIT); a duration of 0
 * yields.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `duration` | `const pok_time_t * JET_USER` | [in] Partition address of the duration. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_MODE if the thread may not wait;
 *         POK_ERRNO_EFAULT for a bad address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_time` | `const pok_time_t *` | Kernel address of the duration |
 * | `kernel_time` | `pok_time_t` | The duration |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_SLEEP_01}: `pok_thread_sleep()` shall make the
 *   current process wait for a duration, a duration of 0 making it yield to the
 *   ready processes of its priority.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *   - \srqref{SRQ_JET_THR_016}
 * - \hlrid{HLR_THREAD_POK_THREAD_SLEEP_02}: `pok_thread_sleep()` shall refuse
 *   to make the process wait where it may not wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_007}
 * - \hlrid{HLR_THREAD_POK_THREAD_SLEEP_03}: `pok_thread_sleep()` shall refuse a
 *   duration address that the partition may not read with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_SLEEP_01}: If `thread_is_waiting_allowed()`
 *   returns false, `pok_thread_sleep()` shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SLEEP_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SLEEP_02}: If `thread_is_waiting_allowed()`
 *   returns true and `jet_user_to_kernel_typed_ro()` returns NULL for
 *   `duration`, `pok_thread_sleep()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SLEEP_03}
 * - \llrid{LLR_THREAD_POK_THREAD_SLEEP_03}: If both checks pass,
 *   `pok_thread_sleep()` shall call `pok_preemption_local_disable()`, then the
 *   function below, then `pok_preemption_local_enable()`, and return
 *   `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SLEEP_01}
 * - \llrid{LLR_THREAD_POK_THREAD_SLEEP_04}: If the duration (the value at
 *   `duration`) is 0, `pok_thread_sleep()` shall call `thread_yield()` with the
 *   current thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SLEEP_01}
 * - \llrid{LLR_THREAD_POK_THREAD_SLEEP_05}: If the duration is not 0,
 *   `pok_thread_sleep()` shall call `thread_wait_common()` with the current
 *   thread and the duration.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SLEEP_01}
 *
 * @design
 * 1. If `thread_is_waiting_allowed()` is false:
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_01}
 *    1. Return `POK_ERRNO_MODE`.
 * 2. Set `k_time` to `jet_user_to_kernel_typed_ro(duration)`.
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_02}
 * 3. If `k_time` is NULL: \llrref{LLR_THREAD_POK_THREAD_SLEEP_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `kernel_time` to the value at `k_time`.
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_04},
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_05}
 * 5. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_03}
 * 6. If `kernel_time` is equal to `0`: \llrref{LLR_THREAD_POK_THREAD_SLEEP_04}
 *    1. Call `thread_yield()` with the argument `current_thread`.
 * 7. Otherwise: \llrref{LLR_THREAD_POK_THREAD_SLEEP_05}
 *    1. Call `thread_wait_common()` with the arguments `current_thread` and
 *       `kernel_time`.
 * 8. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_03}
 * 9. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_THREAD_SLEEP_03}
 *
 * @calls
 * - `thread_is_waiting_allowed()`
 * - `jet_user_to_kernel_ro()`
 * - `pok_preemption_local_disable()`
 * - `thread_yield()`
 * - `thread_wait_common()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_sleep(const pok_time_t* JET_USER duration)
{
    if(!thread_is_waiting_allowed()) {
        return POK_ERRNO_MODE;
    }

    const pok_time_t* JET_KUSER k_time = jet_user_to_kernel_typed_ro(duration);
    if(k_time == NULL) {
        return POK_ERRNO_EFAULT;
    }
    pok_time_t kernel_time = *k_time;

    pok_preemption_local_disable();

    if(kernel_time == 0) {
        thread_yield(current_thread);
    }
    else {
        thread_wait_common(current_thread, kernel_time);
    }

	pok_preemption_local_enable();

	return POK_ERRNO_OK;
}
#endif

#ifdef POK_NEEDS_THREAD_SLEEP_UNTIL
/**
 * @funcname `pok_thread_sleep_until`
 *
 * @description
 * Makes the current thread sleep until a time.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `until` | `const pok_time_t * JET_USER` | [in] Partition address of the time. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_MODE if the thread may not wait;
 *         POK_ERRNO_EFAULT for a bad address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_time` | `const pok_time_t *` | Kernel address of the time |
 * | `kernel_time` | `pok_time_t` | The time |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_SLEEP_UNTIL_01}: `pok_thread_sleep_until()`
 *   shall make the current process wait until a time.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 * - \hlrid{HLR_THREAD_POK_THREAD_SLEEP_UNTIL_02}: `pok_thread_sleep_until()`
 *   shall refuse to make the process wait where it may not wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_007}
 * - \hlrid{HLR_THREAD_POK_THREAD_SLEEP_UNTIL_03}: `pok_thread_sleep_until()`
 *   shall refuse a time address that the partition may not read with
 *   `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_01}: If
 *   `thread_is_waiting_allowed()` returns false, `pok_thread_sleep_until()`
 *   shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SLEEP_UNTIL_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_02}: If
 *   `thread_is_waiting_allowed()` returns true and
 *   `jet_user_to_kernel_typed_ro()` returns NULL for `until`,
 *   `pok_thread_sleep_until()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SLEEP_UNTIL_03}
 * - \llrid{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_03}: If both checks pass,
 *   `pok_thread_sleep_until()` shall call `pok_preemption_local_disable()`,
 *   then `thread_wait_timed()` with the current thread and the time (the value
 *   at `until`), then `pok_preemption_local_enable()`, and return
 *   `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SLEEP_UNTIL_01}
 *
 * @design
 * 1. If `thread_is_waiting_allowed()` is false:
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_01}
 *    1. Return `POK_ERRNO_MODE`.
 * 2. Set `k_time` to `jet_user_to_kernel_typed_ro(until)`.
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_02}
 * 3. If `k_time` is NULL: \llrref{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `kernel_time` to the value at `k_time`.
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_03}
 * 5. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_03}
 * 6. Call `thread_wait_timed()` with the arguments `current_thread` and
 *    `kernel_time`. \llrref{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_03}
 * 7. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_03}
 * 8. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_THREAD_SLEEP_UNTIL_03}
 *
 * @calls
 * - `thread_is_waiting_allowed()`
 * - `jet_user_to_kernel_ro()`
 * - `pok_preemption_local_disable()`
 * - `thread_wait_timed()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_sleep_until(const pok_time_t* JET_USER until)
{
	if(!thread_is_waiting_allowed()) {
		return POK_ERRNO_MODE;
	}

    const pok_time_t* JET_KUSER k_time = jet_user_to_kernel_typed_ro(until);
    if(k_time == NULL) {
        return POK_ERRNO_EFAULT;
    }
    pok_time_t kernel_time = *k_time;

	pok_preemption_local_disable();

	thread_wait_timed(current_thread, kernel_time);
	pok_preemption_local_enable();

	return POK_ERRNO_OK;
}
#endif

/**
 * @funcname `pok_thread_yield`
 *
 * @description
 * Moves the current thread after the other ready threads of its priority.
 *
 * @return POK_ERRNO_OK.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_YIELD_01}: `pok_thread_yield()` shall move the
 *   current process after the other ready processes of its priority.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_YIELD_01}: `pok_thread_yield()` shall call
 *   `pok_preemption_local_disable()`, then `thread_yield()` with the current
 *   thread, then `pok_preemption_local_enable()`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_YIELD_01}
 *
 * @design
 * 1. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_YIELD_01}
 * 2. Call `thread_yield()` with the argument `current_thread`.
 *    \llrref{LLR_THREAD_POK_THREAD_YIELD_01}
 * 3. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_YIELD_01}
 * 4. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_THREAD_YIELD_01}
 *
 * @calls
 * - `pok_preemption_local_disable()`
 * - `thread_yield()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_yield (void)
{
	pok_preemption_local_disable();
	thread_yield(current_thread);
	pok_preemption_local_enable();

    return POK_ERRNO_OK;
}


/**
 * @funcname `thread_delayed_start_internal`
 *
 * @description
 * Starts a stopped thread after a delay: resets its state and shared data;
 * outside NORMAL mode it waits for NORMAL mode; in NORMAL mode an aperiodic
 * thread starts after the delay, a periodic one at its next periodic processing
 * start plus the delay, with its deadline set. Called with local preemption
 * disabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_t *` | [in,out] The thread. |
 * | `delay` | `pok_time_t` | [in] The delay (not infinite). |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_UNAVAILABLE if the thread isn't stopped;
 *         POK_ERRNO_EINVAL if the delay isn't shorter than the period.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `thread_start_time` | `pok_time_t` | When the thread starts |
 * | `tshd` | `jet_thread_shared_data_t *` | The thread's shared data |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_01}:
 *   `thread_delayed_start_internal()` shall start a stopped process after a
 *   delay, with its base priority and fresh state.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 * - \hlrid{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_02}:
 *   `thread_delayed_start_internal()` shall refuse a process that is not
 *   stopped and a delay that is not shorter than the period.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 * - \hlrid{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}:
 *   `thread_delayed_start_internal()` shall, outside `NORMAL` mode, keep the
 *   process waiting for `NORMAL` mode with its delay.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_PART_006}
 * - \hlrid{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_04}:
 *   `thread_delayed_start_internal()` shall start a periodic process at the
 *   next start of periodic processing of the partition plus the delay, with its
 *   next activation one period later.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SCHED_006}
 *   - \srqref{SRQ_JET_THR_011}
 * - \hlrid{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_05}:
 *   `thread_delayed_start_internal()` shall set the deadline of a process with
 *   a finite time capacity to its start time plus the capacity.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_012}
 *
 * @llr
 * - \llrid{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_01}: If `state` of `thread`
 *   is not `POK_STATE_STOPPED`, `thread_delayed_start_internal()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_02}
 * - \llrid{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_02}: If `state` is
 *   `POK_STATE_STOPPED`, `pok_time_is_infinity()` returns false for `period` of
 *   `thread` and `delay` is not less than `period`,
 *   `thread_delayed_start_internal()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_02}
 * - \llrid{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}: If these checks pass,
 *   `thread_delayed_start_internal()` shall set `priority` of `thread` to its
 *   `base_priority` and its `sp` to NULL, and set, in its shared data
 *   (`kshd->tshd` plus its index), `msection_count` to 0, `msection_entering`
 *   to NULL, `priority` to the priority of the thread and `thread_kernel_flags`
 *   to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_01}
 * - \llrid{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_04}: If these checks pass
 *   and `mode` of the partition is not `POK_PARTITION_MODE_NORMAL`,
 *   `thread_delayed_start_internal()` shall set `delayed_time` of `thread` to
 *   `delay` and its `state` to `POK_STATE_WAITING`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}
 * - \llrid{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_05}: If `mode` is
 *   `POK_PARTITION_MODE_NORMAL` and `pok_time_is_infinity()` returns true for
 *   `period`, `thread_delayed_start_internal()` shall take as the start time
 *   the result of `pok_time_add()` for the result of `jet_system_time()` and
 *   `delay`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_01}
 * - \llrid{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_06}: If `mode` is
 *   `POK_PARTITION_MODE_NORMAL` and `period` is finite,
 *   `thread_delayed_start_internal()` shall take as the start time the result
 *   of `pok_time_add()` for the result of
 *   `get_next_periodic_processing_start()` and `delay`, and set
 *   `next_activation` of `thread` to the result of `pok_time_add()` for the
 *   start time and `period`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_04}
 * - \llrid{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_07}: If `mode` is
 *   `POK_PARTITION_MODE_NORMAL`, and if and only if `pok_time_is_infinity()`
 *   returns false for `time_capacity`, `thread_delayed_start_internal()` shall
 *   call `thread_set_deadline()` with `thread` and the result of
 *   `pok_time_add()` for the start time and `time_capacity`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_05}
 * - \llrid{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_08}: If `mode` is
 *   `POK_PARTITION_MODE_NORMAL`, `delay` is 0 and `period` is infinite,
 *   `thread_delayed_start_internal()` shall call `thread_start()` with `thread`
 *   and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_01}
 * - \llrid{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_09}: If `mode` is
 *   `POK_PARTITION_MODE_NORMAL`, and `delay` is not 0 or `period` is finite,
 *   `thread_delayed_start_internal()` shall call `thread_wait_timed()` with
 *   `thread` and the start time, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_THREAD_DELAYED_START_INTERNAL_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}
 * 2. Set `tshd` to `part->kshd->tshd + (thread - part->threads)`.
 *    \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}
 * 3. If `thread->state` is not equal to `POK_STATE_STOPPED`:
 *    \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_01}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 4. If `pok_time_is_infinity(thread->period)` is false and `delay` is greater
 *    than or equal to `thread->period`:
 *    \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_02}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 5. Set `thread->priority` to `thread->base_priority`.
 *    \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}
 * 6. Set `thread->sp` to `NULL`.
 *    \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}
 * 7. Set `tshd->msection_count` to `0`.
 *    \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}
 * 8. Set `tshd->msection_entering` to `NULL`.
 *    \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}
 * 9. Set `tshd->priority` to `thread->priority`.
 *    \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}
 * 10. Set `tshd->thread_kernel_flags` to `0`.
 *     \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_03}
 * 11. If `part->mode` is not equal to `POK_PARTITION_MODE_NORMAL`:
 *     \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_04}
 *     1. Set `thread->delayed_time` to `delay`.
 *     2. Set `thread->state` to `POK_STATE_WAITING`.
 *     3. Return `POK_ERRNO_OK`.
 * 12. If `pok_time_is_infinity(thread->period)` is true:
 *     \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_05}
 *     1. Call `pok_time_add()` with the arguments `jet_system_time()` and
 *        `delay`, and store the result in `thread_start_time`.
 * 13. Otherwise: \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_06}
 *     1. Call `pok_time_add()` with the arguments
 *        `get_next_periodic_processing_start()` and `delay`, and store the
 *        result in `thread_start_time`.
 *     2. Call `pok_time_add()` with the arguments `thread_start_time` and
 *        `thread->period`, and store the result in `thread->next_activation`.
 * 14. If `pok_time_is_infinity(thread->time_capacity)` is false:
 *     \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_07}
 *     1. Call `thread_set_deadline()` with the arguments `thread` and
 *        `pok_time_add(thread_start_time, thread->time_capacity)`.
 * 15. If `delay` is equal to `0` and `pok_time_is_infinity(thread->period)` is
 *     true: \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_08},
 *     \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_09}
 *     1. Call `thread_start()` with the argument `thread`.
 *        \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_08}
 * 16. Otherwise: \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_09}
 *     1. Call `thread_wait_timed()` with the arguments `thread` and
 *        `thread_start_time`.
 * 17. Return `POK_ERRNO_OK`.
 *     \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_08},
 *     \llrref{LLR_THREAD_THREAD_DELAYED_START_INTERNAL_09}
 *
 * @calls
 * - `pok_time_is_infinity()`
 * - `pok_time_add()`
 * - `ja_system_time()`
 * - `get_next_periodic_processing_start()`
 * - `thread_set_deadline()`
 * - `thread_start()`
 * - `thread_wait_timed()`
 */
static pok_ret_e thread_delayed_start_internal (pok_thread_t* thread,
												pok_time_t delay)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_time_t thread_start_time;

    jet_thread_shared_data_t* tshd = part->kshd->tshd
        + (thread - part->threads);

    assert(!pok_time_is_infinity(delay));

    if (thread->state != POK_STATE_STOPPED) {
        return POK_ERRNO_UNAVAILABLE;
    }

    if (!pok_time_is_infinity(thread->period) && (delay >= thread->period)) {
        return POK_ERRNO_EINVAL;
    }

    thread->priority = thread->base_priority;
	thread->sp = NULL;

    tshd->msection_count = 0;
    tshd->msection_entering = NULL;
    tshd->priority = thread->priority;
    tshd->thread_kernel_flags = 0;

	if(part->mode != POK_PARTITION_MODE_NORMAL)
	{
		/* Delay thread's starting until normal mode. */
		thread->delayed_time = delay;
		thread->state = POK_STATE_WAITING;

		return POK_ERRNO_OK;
	}

    // Normal mode.
    if (pok_time_is_infinity(thread->period)) {
        // aperiodic process
        thread_start_time = pok_time_add(jet_system_time(), delay);
    }
    else {
		// periodic process
		thread_start_time = pok_time_add(
		    get_next_periodic_processing_start(), delay);
		thread->next_activation =
		    pok_time_add(thread_start_time, thread->period);

	}

	if(!pok_time_is_infinity(thread->time_capacity))
    {
		thread_set_deadline(thread,
		    pok_time_add(thread_start_time, thread->time_capacity));
    }

	/* Only non-delayed aperiodic process starts immediately */
	if((delay == 0) && pok_time_is_infinity(thread->period)) {
        thread_start(thread);
	} else {
		thread_wait_timed(thread, thread_start_time);
	}

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_thread_delayed_start`
 *
 * @description
 * Starts a thread after a delay (DELAYED_START).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_thread_id_t` | [in] The thread ID. |
 * | `delay_time` | `const pok_time_t * JET_USER` | [in] Partition address of the delay. |
 *
 * @return As thread_delayed_start_internal(); POK_ERRNO_PARAM for a bad ID;
 *         POK_ERRNO_EFAULT for a bad address; POK_ERRNO_EINVAL for an infinite
 *         delay.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_delay_time` | `const pok_time_t *` | Kernel address of the delay |
 * | `kernel_delay_time` | `pok_time_t` | The delay |
 * | `ret` | `pok_ret_e` | The result |
 * | `thread` | `pok_thread_t *` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_DELAYED_START_01}:
 *   `pok_thread_delayed_start()` shall start a process of the current partition
 *   after a delay, as `thread_delayed_start_internal()` does, and refuse an
 *   infinite delay.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 * - \hlrid{HLR_THREAD_POK_THREAD_DELAYED_START_02}:
 *   `pok_thread_delayed_start()` shall refuse an identifier of no process and a
 *   delay address that the partition may not read.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_DELAYED_START_01}: If `get_thread_by_id()`
 *   returns NULL for `id`, `pok_thread_delayed_start()` shall return
 *   `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_DELAYED_START_02}
 * - \llrid{LLR_THREAD_POK_THREAD_DELAYED_START_02}: If the thread exists and
 *   `jet_user_to_kernel_typed_ro()` returns NULL for `delay_time`,
 *   `pok_thread_delayed_start()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_DELAYED_START_02}
 * - \llrid{LLR_THREAD_POK_THREAD_DELAYED_START_03}: If both give an address and
 *   `pok_time_is_infinity()` returns true for the delay (the value at
 *   `delay_time`), `pok_thread_delayed_start()` shall return
 *   `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_DELAYED_START_01}
 * - \llrid{LLR_THREAD_POK_THREAD_DELAYED_START_04}: If the delay is finite,
 *   `pok_thread_delayed_start()` shall call `pok_preemption_local_disable()`,
 *   then `thread_delayed_start_internal()` with the thread and the delay, then
 *   `pok_preemption_local_enable()`, and return the result of
 *   `thread_delayed_start_internal()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_DELAYED_START_01}
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `thread`. \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_01}
 * 2. If `thread` is NULL: \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_01}
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. Set `k_delay_time` to `jet_user_to_kernel_typed_ro(delay_time)`.
 *    \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_02}
 * 4. If `k_delay_time` is NULL: \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `kernel_delay_time` to the value at `k_delay_time`.
 *    \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_03},
 *    \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_04}
 * 6. If `pok_time_is_infinity(kernel_delay_time)` is true:
 *    \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_03}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 7. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_04}
 * 8. Call `thread_delayed_start_internal()` with the arguments `thread` and
 *    `kernel_delay_time`, and store the result in `ret`.
 *    \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_04}
 * 9. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_04}
 * 10. Return `ret`. \llrref{LLR_THREAD_POK_THREAD_DELAYED_START_04}
 *
 * @calls
 * - `get_thread_by_id()`
 * - `jet_user_to_kernel_ro()`
 * - `pok_time_is_infinity()`
 * - `pok_preemption_local_disable()`
 * - `thread_delayed_start_internal()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_delayed_start (pok_thread_id_t id,
    const pok_time_t* JET_USER delay_time)
{
    pok_ret_e ret;

    pok_thread_t *thread = get_thread_by_id(id);
    if(thread == NULL) {
        return POK_ERRNO_PARAM;
    }

    const pok_time_t* JET_KUSER k_delay_time =
        jet_user_to_kernel_typed_ro(delay_time);
    if(k_delay_time == NULL) {
        return POK_ERRNO_EFAULT;
    }
    pok_time_t kernel_delay_time = *k_delay_time;

    if (pok_time_is_infinity(kernel_delay_time)) {
        return POK_ERRNO_EINVAL;
    }

    pok_preemption_local_disable();
    ret = thread_delayed_start_internal(thread, kernel_delay_time);
    pok_preemption_local_enable();

    return ret;
}

/**
 * @funcname `pok_thread_start`
 *
 * @description
 * Starts a thread (START).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_thread_id_t` | [in] The thread ID. |
 *
 * @return As thread_delayed_start_internal(); POK_ERRNO_PARAM for a bad ID.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `pok_ret_e` | The result |
 * | `thread` | `pok_thread_t *` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_START_01}: `pok_thread_start()` shall start a
 *   process of the current partition with no delay, as
 *   `thread_delayed_start_internal()` does.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 * - \hlrid{HLR_THREAD_POK_THREAD_START_02}: `pok_thread_start()` shall refuse
 *   an identifier of no process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_START_01}: If `get_thread_by_id()` returns
 *   NULL for `id`, `pok_thread_start()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_START_02}
 * - \llrid{LLR_THREAD_POK_THREAD_START_02}: If `get_thread_by_id()` returns a
 *   thread, `pok_thread_start()` shall call `pok_preemption_local_disable()`,
 *   then `thread_delayed_start_internal()` with the thread and 0, then
 *   `pok_preemption_local_enable()`, and return the result of
 *   `thread_delayed_start_internal()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_START_01}
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `thread`. \llrref{LLR_THREAD_POK_THREAD_START_01}
 * 2. If `thread` is NULL: \llrref{LLR_THREAD_POK_THREAD_START_01}
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_START_02}
 * 4. Call `thread_delayed_start_internal()` with the arguments `thread` and
 *    `0`, and store the result in `ret`.
 *    \llrref{LLR_THREAD_POK_THREAD_START_02}
 * 5. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_START_02}
 * 6. Return `ret`. \llrref{LLR_THREAD_POK_THREAD_START_02}
 *
 * @calls
 * - `get_thread_by_id()`
 * - `pok_preemption_local_disable()`
 * - `thread_delayed_start_internal()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_start (pok_thread_id_t id)
{
	pok_ret_e ret;

    pok_thread_t *thread = get_thread_by_id(id);
    if(thread == NULL) {
        return POK_ERRNO_PARAM;
    }

	pok_preemption_local_disable();
	ret = thread_delayed_start_internal(thread, 0);
	pok_preemption_local_enable();

	return ret;
}


/**
 * @funcname `pok_thread_get_status`
 *
 * @description
 * Gives the name, entry point and status of a thread (GET_PROCESS_STATUS):
 * attributes, current priority, state (a suspended thread is WAITING, the
 * current one RUNNING) and deadline time.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_thread_id_t` | [in] The thread ID. |
 * | `name` | `char * JET_USER` | [out] Partition address of the name. |
 * | `entry` | `void * JET_USER *` | [out] Partition address of the entry point. |
 * | `status` | `pok_thread_status_t * JET_USER` | [out] Partition address of the status. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARAM for a bad ID; POK_ERRNO_EFAULT for a
 *         bad address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_entry` | `void **` | Kernel address of the entry point |
 * | `k_name` | `char *` | Kernel address of the name |
 * | `k_status` | `pok_thread_status_t *` | Kernel address of the status |
 * | `t` | `pok_thread_t *` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_GET_STATUS_01}: `pok_thread_get_status()`
 *   shall give the name, entry point, attributes, current priority, state and
 *   deadline time of a process of the current partition: a suspended process
 *   shows as waiting, the current one as running, and a process without a time
 *   capacity has an infinite deadline time.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_003}
 * - \hlrid{HLR_THREAD_POK_THREAD_GET_STATUS_02}: `pok_thread_get_status()`
 *   shall refuse an identifier of no process and addresses that the partition
 *   may not write.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_01}: If `get_thread_by_id()`
 *   returns NULL for `id`, `pok_thread_get_status()` shall return
 *   `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_02}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_02}: If the thread exists and
 *   `jet_user_to_kernel_typed()` returns NULL for `status`,
 *   `pok_thread_get_status()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_02}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_03}: If `status` is writable and
 *   `jet_user_to_kernel_typed()` returns NULL for `entry`,
 *   `pok_thread_get_status()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_02}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_04}: If `entry` is writable and
 *   `jet_user_to_kernel()` returns NULL for `name` and `MAX_NAME_LENGTH`,
 *   `pok_thread_get_status()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_02}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_05}: If these conversions give
 *   addresses, `pok_thread_get_status()` shall copy the `MAX_NAME_LENGTH` bytes
 *   of `name` of the thread to the name with `memcpy()`, store its `entry` at
 *   `entry`, and set `priority`, `period`, `deadline`, `time_capacity` and
 *   `stack_size` of `attributes` of the status to its `base_priority`,
 *   `period`, `deadline`, `time_capacity` and `user_stack_size`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_01}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_06}: `pok_thread_get_status()`
 *   shall then call `pok_preemption_local_disable()`, set `current_priority` of
 *   the status to `priority` of the thread, set its `state` and `deadline_time`
 *   as below, call `pok_preemption_local_enable()`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_01}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_07}: If `state` of the thread is
 *   `POK_STATE_RUNNABLE` and its `suspended` is true, `pok_thread_get_status()`
 *   shall set `state` of the status to `POK_STATE_WAITING`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_01}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_08}: If `state` of the thread is
 *   `POK_STATE_RUNNABLE`, its `suspended` is false and it is `thread_current`
 *   of the partition, `pok_thread_get_status()` shall set `state` of the status
 *   to `POK_STATE_RUNNING`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_01}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_09}: If `state` of the thread is
 *   `POK_STATE_RUNNABLE`, its `suspended` is false and it is not
 *   `thread_current`, `pok_thread_get_status()` shall set `state` of the status
 *   to `state` of the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_01}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_10}: If `state` of the thread is
 *   not `POK_STATE_RUNNABLE`, `pok_thread_get_status()` shall set `state` of
 *   the status to `state` of the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_01}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_11}: If `pok_time_is_infinity()`
 *   returns true for `time_capacity` of the thread, `pok_thread_get_status()`
 *   shall set `deadline_time` of the status to `POK_TIME_INFINITY`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_01}
 * - \llrid{LLR_THREAD_POK_THREAD_GET_STATUS_12}: If `pok_time_is_infinity()`
 *   returns false for `time_capacity`, `pok_thread_get_status()` shall set
 *   `deadline_time` of the status to `timepoint` of `thread_deadline_event` of
 *   the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_GET_STATUS_01}
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`. \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_01}
 * 2. If `t` is NULL: \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_01}
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 *    \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_02}
 * 4. If `k_status` is NULL: \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `k_entry` to `jet_user_to_kernel_typed(entry)`.
 *    \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_03}
 * 6. If `k_entry` is NULL: \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_03}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 7. Call `jet_user_to_kernel()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 *    \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_04}
 * 8. If `k_name` is NULL: \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_04}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 9. Call `memcpy()` with the arguments `k_name`, `t->name` and
 *    `MAX_NAME_LENGTH`. \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_05}
 * 10. Set `*k_entry` to `t->entry`.
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_05}
 * 11. Set `k_status->attributes.priority` to `t->base_priority`.
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_05}
 * 12. Set `k_status->attributes.period` to `t->period`.
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_05}
 * 13. Set `k_status->attributes.deadline` to `t->deadline`.
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_05}
 * 14. Set `k_status->attributes.time_capacity` to `t->time_capacity`.
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_05}
 * 15. Set `k_status->attributes.stack_size` to `t->user_stack_size`.
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_05}
 * 16. Call `pok_preemption_local_disable()`.
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_06}
 * 17. Set `k_status->current_priority` to `t->priority`.
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_06}
 * 18. If `t->state` is equal to `POK_STATE_RUNNABLE`:
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_07},
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_08},
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_09}
 *     1. If `t->suspended` is true:
 *        \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_07}
 *        1. Set `k_status->state` to `POK_STATE_WAITING`.
 *     2. Otherwise, if `current_partition_arinc->thread_current` is equal to
 *        `t`: \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_08}
 *        1. Set `k_status->state` to `POK_STATE_RUNNING`.
 *     3. Otherwise: \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_09}
 *        1. Set `k_status->state` to `t->state`.
 * 19. Otherwise: \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_10}
 *     1. Set `k_status->state` to `t->state`.
 * 20. If `pok_time_is_infinity(t->time_capacity)` is true:
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_11}
 *     1. Set `k_status->deadline_time` to `POK_TIME_INFINITY`.
 * 21. Otherwise: \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_12}
 *     1. Set `k_status->deadline_time` to `t->thread_deadline_event.timepoint`.
 * 22. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_06}
 * 23. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_THREAD_GET_STATUS_06}
 *
 * @calls
 * - `get_thread_by_id()`
 * - `jet_user_to_kernel()`
 * - `memcpy()`
 * - `pok_preemption_local_disable()`
 * - `pok_time_is_infinity()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_get_status (pok_thread_id_t id,
    char* JET_USER name,
    void* JET_USER *entry,
    pok_thread_status_t* JET_USER status)
{
    pok_thread_t *t = get_thread_by_id(id);
    if(t == NULL) {
        return POK_ERRNO_PARAM;
    }

    pok_thread_status_t* JET_KUSER k_status = jet_user_to_kernel_typed(status);
    if(k_status == NULL) {
        return POK_ERRNO_EFAULT;
    }

    void* JET_KUSER *k_entry = jet_user_to_kernel_typed(entry);
    if(k_entry == NULL) {
        return POK_ERRNO_EFAULT;
    }

    char* JET_KUSER k_name = jet_user_to_kernel(name, MAX_NAME_LENGTH);
    if(k_name == NULL) {
        return POK_ERRNO_EFAULT;
    }

    (void) memcpy(k_name, t->name, MAX_NAME_LENGTH);
    *k_entry = t->entry;

    k_status->attributes.priority = t->base_priority;
	k_status->attributes.period = t->period;
	k_status->attributes.deadline = t->deadline;
	k_status->attributes.time_capacity = t->time_capacity;
	k_status->attributes.stack_size = t->user_stack_size;

    pok_preemption_local_disable();

	k_status->current_priority = t->priority;

	if(t->state == POK_STATE_RUNNABLE)
    {
        if(t->suspended) {
            k_status->state = POK_STATE_WAITING;
        } else if(current_partition_arinc->thread_current == t) {
            k_status->state = POK_STATE_RUNNING;
        } else {
            k_status->state = t->state;
        }
	}
    else
    {
        k_status->state = t->state;
    }

	if(pok_time_is_infinity(t->time_capacity)) {
		k_status->deadline_time = POK_TIME_INFINITY;
	} else {
		k_status->deadline_time = t->thread_deadline_event.timepoint;
	}

	pok_preemption_local_enable();

	return POK_ERRNO_OK;
}

/**
 * @funcname `pok_thread_set_priority`
 *
 * @description
 * Sets the current priority of a thread (SET_PRIORITY); it moves after the
 * ready threads of its new priority.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_thread_id_t` | [in] The thread ID. |
 * | `priority` | `uint32_t` | [in] The priority. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARAM for a bad ID or priority;
 *         POK_ERRNO_UNAVAILABLE for a stopped thread.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `ret` | `pok_ret_e` | The result |
 * | `t` | `pok_thread_t *` | The thread |
 * | `tshd` | `jet_thread_shared_data_t *` | The thread's shared data |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_SET_PRIORITY_01}: `pok_thread_set_priority()`
 *   shall set the current priority of a process that is not stopped to a value
 *   from 1 to 239, the process taking its place after the ready processes of
 *   the new priority.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_010}
 *   - \srqref{SRQ_JET_THR_021}
 *   - \srqref{SRQ_JET_THR_002}
 * - \hlrid{HLR_THREAD_POK_THREAD_SET_PRIORITY_02}: `pok_thread_set_priority()`
 *   shall refuse an identifier of no process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_SET_PRIORITY_01}: If `get_thread_by_id()`
 *   returns NULL for `id`, `pok_thread_set_priority()` shall return
 *   `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SET_PRIORITY_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SET_PRIORITY_02}: If the thread exists and
 *   `priority` is greater than `MAX_PRIORITY_VALUE`,
 *   `pok_thread_set_priority()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SET_PRIORITY_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SET_PRIORITY_03}: If `priority` is not greater
 *   than `MAX_PRIORITY_VALUE` and is less than `MIN_PRIORITY_VALUE`,
 *   `pok_thread_set_priority()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SET_PRIORITY_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SET_PRIORITY_04}: If these checks pass,
 *   `pok_thread_set_priority()` shall call `pok_preemption_local_disable()`
 *   before it reads the state of the thread and `pok_preemption_local_enable()`
 *   before it returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SET_PRIORITY_01}
 * - \llrid{LLR_THREAD_POK_THREAD_SET_PRIORITY_05}: If `state` of the thread is
 *   `POK_STATE_STOPPED`, `pok_thread_set_priority()` shall return
 *   `POK_ERRNO_UNAVAILABLE` and leave its priority as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SET_PRIORITY_01}
 * - \llrid{LLR_THREAD_POK_THREAD_SET_PRIORITY_06}: If `state` of the thread is
 *   not `POK_STATE_STOPPED`, `pok_thread_set_priority()` shall set `priority`
 *   of the thread and `priority` of its shared data (`kshd->tshd` plus its
 *   index) to `priority`, call `thread_yield()` with the thread, and return
 *   `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SET_PRIORITY_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_06}
 * 2. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`. \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_01}
 * 3. If `t` is NULL: \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_01}
 *    1. Return `POK_ERRNO_PARAM`.
 * 4. If `priority` is greater than `MAX_PRIORITY_VALUE`:
 *    \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_02}
 *    1. Return `POK_ERRNO_PARAM`.
 * 5. If `priority` is less than `MIN_PRIORITY_VALUE`:
 *    \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_03}
 *    1. Return `POK_ERRNO_PARAM`.
 * 6. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_04}
 * 7. Set `ret` to `POK_ERRNO_UNAVAILABLE`.
 *    \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_05}
 * 8. If `t->state` is equal to `POK_STATE_STOPPED`:
 *    \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_05}
 *    1. Go to the label `out`.
 * 9. Set `t->priority` to `priority`.
 *    \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_06}
 * 10. Set `tshd` to `part->kshd->tshd + (t - part->threads)`.
 *     \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_06}
 * 11. Set `tshd->priority` to `priority`.
 *     \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_06}
 * 12. Call `thread_yield()` with the argument `t`.
 *     \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_06}
 * 13. Set `ret` to `POK_ERRNO_OK`.
 *     \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_06}
 * 14. Label `out`: \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_04}
 * 15. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_04}
 * 16. Return `ret`. \llrref{LLR_THREAD_POK_THREAD_SET_PRIORITY_04}
 *
 * @calls
 * - `get_thread_by_id()`
 * - `pok_preemption_local_disable()`
 * - `thread_yield()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_set_priority(pok_thread_id_t id, uint32_t priority)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_ret_e ret;

    pok_thread_t *t = get_thread_by_id(id);
    if(t == NULL) {
        return POK_ERRNO_PARAM;
    }

    if(priority > MAX_PRIORITY_VALUE) {
        return POK_ERRNO_PARAM;
    }
    if(priority < MIN_PRIORITY_VALUE) {
        return POK_ERRNO_PARAM;
    }

    pok_preemption_local_disable();

    ret = POK_ERRNO_UNAVAILABLE;
    if (t->state == POK_STATE_STOPPED) {
        goto out;
    }

    t->priority = priority;

    jet_thread_shared_data_t* tshd = part->kshd->tshd
        + (t - part->threads);
    tshd->priority = priority;

    thread_yield(t);

	ret = POK_ERRNO_OK;
out:
	pok_preemption_local_enable();

    return ret;
}

/**
 * @funcname `pok_thread_resume`
 *
 * @description
 * Resumes a suspended thread (RESUME).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_thread_id_t` | [in] The thread ID. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARAM for a bad ID; POK_ERRNO_THREADATTR for
 *         the current thread; POK_ERRNO_MODE for a periodic or stopped thread;
 *         POK_ERRNO_UNAVAILABLE if it isn't suspended.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `pok_ret_e` | The result |
 * | `t` | `pok_thread_t *` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_RESUME_01}: `pok_thread_resume()` shall resume
 *   a suspended aperiodic process of the current partition other than the
 *   caller.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_008}
 * - \hlrid{HLR_THREAD_POK_THREAD_RESUME_02}: `pok_thread_resume()` shall refuse
 *   an identifier of no process, the caller itself, a periodic or stopped
 *   process, and a process that is not suspended.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_RESUME_01}: If `get_thread_by_id()` returns
 *   NULL for `id`, `pok_thread_resume()` shall return `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_RESUME_02}
 * - \llrid{LLR_THREAD_POK_THREAD_RESUME_02}: If the thread exists and is the
 *   current thread, `pok_thread_resume()` shall return `POK_ERRNO_THREADATTR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_RESUME_02}
 * - \llrid{LLR_THREAD_POK_THREAD_RESUME_03}: If the thread is another one and
 *   `pok_thread_is_periodic()` returns true for it, `pok_thread_resume()` shall
 *   return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_RESUME_02}
 * - \llrid{LLR_THREAD_POK_THREAD_RESUME_04}: If these checks pass,
 *   `pok_thread_resume()` shall call `pok_preemption_local_disable()` before it
 *   reads the state of the thread and `pok_preemption_local_enable()` before it
 *   returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_RESUME_02}
 * - \llrid{LLR_THREAD_POK_THREAD_RESUME_05}: If `state` of the thread is
 *   `POK_STATE_STOPPED`, `pok_thread_resume()` shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_RESUME_02}
 * - \llrid{LLR_THREAD_POK_THREAD_RESUME_06}: If the thread is not stopped and
 *   its `suspended` is false, `pok_thread_resume()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_RESUME_02}
 * - \llrid{LLR_THREAD_POK_THREAD_RESUME_07}: If the thread is not stopped and
 *   its `suspended` is true, `pok_thread_resume()` shall call `thread_resume()`
 *   with it and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_RESUME_01}
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`. \llrref{LLR_THREAD_POK_THREAD_RESUME_01}
 * 2. If `t` is NULL: \llrref{LLR_THREAD_POK_THREAD_RESUME_01}
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. If `t` is equal to `current_thread`:
 *    \llrref{LLR_THREAD_POK_THREAD_RESUME_02}
 *    1. Return `POK_ERRNO_THREADATTR`.
 * 4. If `pok_thread_is_periodic(t)` is true:
 *    \llrref{LLR_THREAD_POK_THREAD_RESUME_03}
 *    1. Return `POK_ERRNO_MODE`.
 * 5. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_RESUME_04}
 * 6. Set `ret` to `POK_ERRNO_MODE`. \llrref{LLR_THREAD_POK_THREAD_RESUME_05}
 * 7. If `t->state` is equal to `POK_STATE_STOPPED`:
 *    \llrref{LLR_THREAD_POK_THREAD_RESUME_05}
 *    1. Go to the label `out`.
 * 8. Set `ret` to `POK_ERRNO_UNAVAILABLE`.
 *    \llrref{LLR_THREAD_POK_THREAD_RESUME_06}
 * 9. If `t->suspended` is false: \llrref{LLR_THREAD_POK_THREAD_RESUME_06}
 *    1. Go to the label `out`.
 * 10. Call `thread_resume()` with the argument `t`.
 *     \llrref{LLR_THREAD_POK_THREAD_RESUME_07}
 * 11. Set `ret` to `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_THREAD_RESUME_07}
 * 12. Label `out`: \llrref{LLR_THREAD_POK_THREAD_RESUME_04}
 * 13. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_POK_THREAD_RESUME_04}
 * 14. Return `ret`. \llrref{LLR_THREAD_POK_THREAD_RESUME_04}
 *
 * @calls
 * - `get_thread_by_id()`
 * - `pok_thread_is_periodic()`
 * - `pok_preemption_local_disable()`
 * - `thread_resume()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_resume(pok_thread_id_t id)
{
    pok_ret_e ret;

    pok_thread_t *t = get_thread_by_id(id);
    if(t == NULL) {
        return POK_ERRNO_PARAM;
    }

    // can't resume self, lol
    if (t == current_thread) {
        return POK_ERRNO_THREADATTR;
    }

	// although periodic process can never be suspended anyway,
	// ARINC-653 requires different error code
    if (pok_thread_is_periodic(t)) {
        return POK_ERRNO_MODE;
    }

    pok_preemption_local_disable();

    ret = POK_ERRNO_MODE;
    if (t->state == POK_STATE_STOPPED) {
        goto out;
    }

	ret = POK_ERRNO_UNAVAILABLE;
    if (!t->suspended) {
        goto out;
    }

    thread_resume(t);

	ret = POK_ERRNO_OK;
out:
	pok_preemption_local_enable();

    return ret;
}

/**
 * @funcname `pok_thread_suspend_target`
 *
 * @description
 * Suspends another thread (SUSPEND).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_thread_id_t` | [in] The thread ID. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARAM for a bad ID; POK_ERRNO_THREADATTR for
 *         the current thread; POK_ERRNO_MODE for a periodic or stopped thread,
 *         or the thread holding the partition's lock; POK_ERRNO_UNAVAILABLE if
 *         it is suspended already.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `pok_ret_e` | The result |
 * | `t` | `pok_thread_t *` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_01}:
 *   `pok_thread_suspend_target()` shall suspend an aperiodic process of the
 *   current partition other than the caller.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_008}
 * - \hlrid{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}:
 *   `pok_thread_suspend_target()` shall refuse an identifier of no process, the
 *   caller itself, a periodic or stopped process, the process that holds the
 *   lock of preemption, and a process that is already suspended.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_01}: If `get_thread_by_id()`
 *   returns NULL for `id`, `pok_thread_suspend_target()` shall return
 *   `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}: If the thread exists and
 *   is the current thread, `pok_thread_suspend_target()` shall return
 *   `POK_ERRNO_THREADATTR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_03}: If the thread is another
 *   one and `pok_thread_is_periodic()` returns true for it,
 *   `pok_thread_suspend_target()` shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_04}: If these checks pass,
 *   `pok_thread_suspend_target()` shall call `pok_preemption_local_disable()`
 *   before it reads the state of the thread and `pok_preemption_local_enable()`
 *   before it returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_05}: If `state` of the thread
 *   is `POK_STATE_STOPPED`, `pok_thread_suspend_target()` shall return
 *   `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_06}: If the thread is not
 *   stopped, `lock_level` of the partition is greater than 0 and
 *   `thread_locked` is the thread, `pok_thread_suspend_target()` shall return
 *   `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_07}: If the thread is not
 *   stopped, is not the locking thread (or the lock level is 0) and its
 *   `suspended` is true, `pok_thread_suspend_target()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_08}: If the thread is not
 *   stopped, is not the locking thread (or the lock level is 0) and its
 *   `suspended` is false, `pok_thread_suspend_target()` shall call
 *   `thread_suspend()` with it and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_TARGET_01}
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`. \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_01}
 * 2. If `t` is NULL: \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_01}
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. If `t` is equal to `current_thread`:
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_02}
 *    1. Return `POK_ERRNO_THREADATTR`.
 * 4. If `pok_thread_is_periodic(t)` is true:
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_03}
 *    1. Return `POK_ERRNO_MODE`.
 * 5. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_04}
 * 6. Set `ret` to `POK_ERRNO_MODE`.
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_05}
 * 7. If `t->state` is equal to `POK_STATE_STOPPED`:
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_05}
 *    1. Go to the label `out`.
 * 8. If `current_partition_arinc->lock_level` is greater than `0U` and
 *    `current_partition_arinc->thread_locked` is equal to `t`:
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_06}
 *    1. Go to the label `out`.
 * 9. Set `ret` to `POK_ERRNO_UNAVAILABLE`.
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_07}
 * 10. If `t->suspended` is true:
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_07}
 *     1. Go to the label `out`.
 * 11. Call `thread_suspend()` with the argument `t`.
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_08}
 * 12. Set `ret` to `POK_ERRNO_OK`.
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_08}
 * 13. Label `out`: \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_04}
 * 14. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_04}
 * 15. Return `ret`. \llrref{LLR_THREAD_POK_THREAD_SUSPEND_TARGET_04}
 *
 * @calls
 * - `get_thread_by_id()`
 * - `pok_thread_is_periodic()`
 * - `pok_preemption_local_disable()`
 * - `thread_suspend()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_suspend_target(pok_thread_id_t id)
{
    pok_ret_e ret;

    pok_thread_t *t = get_thread_by_id(id);
    if(t == NULL) {
        return POK_ERRNO_PARAM;
    }

	// can't suspend current process
	// _using this function_
	// (use pok_thread_suspend instead)
    if (t == current_thread) {
        return POK_ERRNO_THREADATTR;
    }

	// although periodic process can never be suspended anyway,
	// ARINC-653 requires different error code
    if (pok_thread_is_periodic(t)) {
        return POK_ERRNO_MODE;
    }

    pok_preemption_local_disable();

    ret = POK_ERRNO_MODE;
    // can't suspend stopped (dormant) process
    if (t->state == POK_STATE_STOPPED) {
        goto out;
    }

    // Cannot suspend process holded preemption lock.
    if ((current_partition_arinc->lock_level > 0U)
		&& (current_partition_arinc->thread_locked == t)) {
        goto out;
    }

	ret = POK_ERRNO_UNAVAILABLE;
    if (t->suspended) {
        goto out;
    }

    thread_suspend(t);

	ret = POK_ERRNO_OK;
out:
	pok_preemption_local_enable();

    return ret;
}

/**
 * @funcname `pok_thread_suspend`
 *
 * @description
 * Suspends the current thread, for a time or forever (SUSPEND_SELF); a timeout
 * of 0 does nothing.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timeout` | `const pok_time_t * JET_USER` | [in] Partition address of the timeout. |
 *
 * @return POK_ERRNO_OK (resumed, or timeout 0), the wait result of a timed
 *         suspension (POK_ERRNO_TIMEOUT); POK_ERRNO_EFAULT for a bad address;
 *         POK_ERRNO_MODE for a periodic thread or if it may not wait.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_time` | `const pok_time_t *` | Kernel address of the timeout |
 * | `kernel_time` | `pok_time_t` | The timeout |
 * | `t` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_SUSPEND_01}: `pok_thread_suspend()` shall
 *   suspend the current aperiodic process until it is resumed, or until a
 *   time-out, and do nothing for a time-out of 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *   - \srqref{SRQ_JET_THR_017}
 * - \hlrid{HLR_THREAD_POK_THREAD_SUSPEND_02}: `pok_thread_suspend()` shall
 *   refuse to suspend a periodic process or one that may not wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_007}
 * - \hlrid{HLR_THREAD_POK_THREAD_SUSPEND_03}: `pok_thread_suspend()` shall
 *   refuse a time-out address that the partition may not read with
 *   `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_01}: If
 *   `jet_user_to_kernel_typed_ro()` returns NULL for `timeout`,
 *   `pok_thread_suspend()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_03}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_02}: If `timeout` is readable and
 *   `pok_thread_is_periodic()` returns true for the current thread,
 *   `pok_thread_suspend()` shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_03}: If the thread is aperiodic and
 *   `thread_is_waiting_allowed()` returns false, `pok_thread_suspend()` shall
 *   return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_02}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_04}: If these checks pass,
 *   `pok_thread_suspend()` shall call `pok_preemption_local_disable()` before
 *   it suspends the thread and `pok_preemption_local_enable()` before it
 *   returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_01}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_05}: If the time-out (the value at
 *   `timeout`) is 0, `pok_thread_suspend()` shall return `POK_ERRNO_OK` without
 *   suspending the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_01}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_06}: If `pok_time_is_infinity()`
 *   returns true for the time-out, `pok_thread_suspend()` shall call
 *   `thread_suspend()` with the current thread and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_01}
 * - \llrid{LLR_THREAD_POK_THREAD_SUSPEND_07}: If the time-out is neither 0 nor
 *   infinite, `pok_thread_suspend()` shall call `thread_suspend()` and then
 *   `thread_suspend_timed()` with the current thread and the time-out, and
 *   return `wait_result` of the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_SUSPEND_01}
 *
 * @design
 * 1. Set `t` to `current_thread`. \llrref{LLR_THREAD_POK_THREAD_SUSPEND_04},
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_05},
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_06},
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_07}
 * 2. Set `k_time` to `jet_user_to_kernel_typed_ro(timeout)`.
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_01}
 * 3. If `k_time` is NULL: \llrref{LLR_THREAD_POK_THREAD_SUSPEND_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `kernel_time` to the value at `k_time`.
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_01}
 * 5. If `pok_thread_is_periodic(t)` is true:
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_02}
 *    1. Return `POK_ERRNO_MODE`.
 * 6. If `thread_is_waiting_allowed()` is false:
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_03}
 *    1. Return `POK_ERRNO_MODE`.
 * 7. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_04}
 * 8. If `kernel_time` is equal to `0`:
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_05}
 *    1. Go to the label `out`.
 * 9. Call `thread_suspend()` with the argument `t`.
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_06},
 *    \llrref{LLR_THREAD_POK_THREAD_SUSPEND_07}
 * 10. If `pok_time_is_infinity(kernel_time)` is false:
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_06},
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_07}
 *     1. Go to the label `suspend_timed`.
 *        \llrref{LLR_THREAD_POK_THREAD_SUSPEND_07}
 * 11. Label `out`: \llrref{LLR_THREAD_POK_THREAD_SUSPEND_05},
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_06}
 * 12. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_04}
 * 13. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_THREAD_SUSPEND_05},
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_06}
 * 14. Label `suspend_timed`: \llrref{LLR_THREAD_POK_THREAD_SUSPEND_07}
 * 15. Call `thread_suspend_timed()` with the arguments `t` and `kernel_time`.
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_07}
 * 16. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_POK_THREAD_SUSPEND_04}
 * 17. Return `t->wait_result`. \llrref{LLR_THREAD_POK_THREAD_SUSPEND_07}
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `pok_thread_is_periodic()`
 * - `thread_is_waiting_allowed()`
 * - `pok_preemption_local_disable()`
 * - `thread_suspend()`
 * - `pok_time_is_infinity()`
 * - `pok_preemption_local_enable()`
 * - `thread_suspend_timed()`
 */
pok_ret_e pok_thread_suspend(const pok_time_t* JET_USER timeout)
{
    pok_thread_t *t = current_thread;

	const pok_time_t* JET_KUSER k_time =
	    jet_user_to_kernel_typed_ro(timeout);
    if(k_time == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_time_t kernel_time = *k_time;

    // although periodic process can never be suspended anyway,
	// ARINC-653 requires different error code
    if (pok_thread_is_periodic(t)) {
        return POK_ERRNO_MODE;
    }

    if (!thread_is_waiting_allowed()) {
        return POK_ERRNO_MODE;
    }

    pok_preemption_local_disable();

	if(kernel_time == 0) {
	    goto out; // Nothing to do with 0 timeout.
	}

    thread_suspend(t);

	if(!pok_time_is_infinity(kernel_time)) {
	    goto suspend_timed;
	}

out:
	pok_preemption_local_enable();

    return POK_ERRNO_OK;

suspend_timed:
    thread_suspend_timed(t, kernel_time);

    pok_preemption_local_enable();

    return t->wait_result;
}

/**
 * @funcname `pok_thread_stop_target`
 *
 * @description
 * Stops another thread (STOP). A thread in an msection can't be stopped at
 * once: the caller waits for it (donation) until it leaves its msections, or
 * joins the threads already waiting for it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_thread_id_t` | [in] The thread ID. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PARAM for a bad ID; POK_ERRNO_THREADATTR for
 *         the current thread; POK_ERRNO_UNAVAILABLE for a stopped thread.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `ret` | `pok_ret_e` | The result |
 * | `t` | `pok_thread_t *` | The target thread |
 * | `thread_current` | `pok_thread_t *` | The current thread |
 * | `tshd_target` | `jet_thread_shared_data_t *` | The target's shared data |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_STOP_TARGET_01}: `pok_thread_stop_target()`
 *   shall stop another process of the current partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 * - \hlrid{HLR_THREAD_POK_THREAD_STOP_TARGET_02}: `pok_thread_stop_target()`
 *   shall delay the stop of a process that is in a critical section until it
 *   has left it, the caller waiting for it and giving it its time.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_005}
 *   - \srqref{SRQ_JET_THR_020}
 * - \hlrid{HLR_THREAD_POK_THREAD_STOP_TARGET_03}: `pok_thread_stop_target()`
 *   shall wake a waiting process whose stop it delays, with its wait cancelled.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_005}
 * - \hlrid{HLR_THREAD_POK_THREAD_STOP_TARGET_04}: `pok_thread_stop_target()`
 *   shall refuse an identifier of no process, the caller itself, and a stopped
 *   process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_01}: If `get_thread_by_id()`
 *   returns NULL for `id`, `pok_thread_stop_target()` shall return
 *   `POK_ERRNO_PARAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_04}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_02}: If the thread (the target)
 *   exists and is `thread_current` of the partition, `pok_thread_stop_target()`
 *   shall return `POK_ERRNO_THREADATTR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_04}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_03}: If these checks pass,
 *   `pok_thread_stop_target()` shall call `pok_preemption_local_disable()`
 *   before it reads the state of the target and `pok_preemption_local_enable()`
 *   before it returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_01}
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_04}: If `state` of the target is
 *   `POK_STATE_STOPPED`, `pok_thread_stop_target()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_04}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_05}: If the target is not stopped,
 *   its `donate_target` is not NULL and its `first_donator` is NULL,
 *   `pok_thread_stop_target()` shall set its `first_donator` to the current
 *   thread, set `donate_target` and `next_donator` of the current thread to
 *   `donate_target` and `next_donator` of the target, set `next_donator` of the
 *   target to the current thread, call `pok_sched_local_invalidate()` and
 *   return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_06}: If the target is not stopped,
 *   its `donate_target` is not NULL and its `first_donator` is not NULL,
 *   `pok_thread_stop_target()` shall set `donate_target` and `next_donator` of
 *   the current thread to `donate_target` and `next_donator` of the target, set
 *   `next_donator` of the target to the current thread, call
 *   `pok_sched_local_invalidate()` and return `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_07}: If the target is not stopped,
 *   its `donate_target` is NULL and its `first_donator` is not NULL,
 *   `pok_thread_stop_target()` shall set `donate_target` of the current thread
 *   to the target and its `next_donator` to `first_donator` of the target, set
 *   `first_donator` of the target to the current thread, call
 *   `pok_sched_local_invalidate()` and return `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_08}: If the target is not stopped,
 *   has no `donate_target` and no `first_donator`, and `msection_count` of its
 *   shared data is not 0, `pok_thread_stop_target()` shall set `donate_target`
 *   of the current thread to the target and its `next_donator` to NULL, set
 *   `first_donator` of the target to the current thread and
 *   `thread_kernel_flags` of its shared data to `THREAD_KERNEL_FLAG_KILLED`,
 *   call `pok_sched_local_invalidate()` and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_02}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_09}: In that case, if and only if
 *   `state` of the target is `POK_STATE_WAITING`, `pok_thread_stop_target()`
 *   shall first call `thread_wake_up()` with it and set its `wait_result` to
 *   `POK_ERRNO_CANCELLED`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_03}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_TARGET_10}: If the target is not stopped,
 *   has no `donate_target` and no `first_donator`, and `msection_count` of its
 *   shared data is 0, `pok_thread_stop_target()` shall call `thread_stop()`
 *   with it and return `POK_ERRNO_OK`, without calling
 *   `pok_sched_local_invalidate()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_TARGET_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_05},
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_06},
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_07},
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_08}
 * 2. Set `ret` to `POK_ERRNO_PARAM`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_01}
 * 3. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`. \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_01}
 * 4. If `t` is NULL: \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_01}
 *    1. Return `POK_ERRNO_PARAM`.
 * 5. Set `thread_current` to `part->thread_current`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_02}
 * 6. If `t` is equal to `thread_current`:
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_02}
 *    1. Return `POK_ERRNO_THREADATTR`.
 * 7. Set `tshd_target` to `part->kshd->tshd + (t - part->threads)`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_08}
 * 8. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_03}
 * 9. Set `ret` to `POK_ERRNO_UNAVAILABLE`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_04},
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_06},
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_07}
 * 10. If `t->state` is equal to `POK_STATE_STOPPED`:
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_04}
 *     1. Go to the label `out`.
 * 11. If `t->relations_stop.donate_target` is not NULL:
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_05},
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_06}
 *     1. If `t->relations_stop.first_donator` is NULL:
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_05}
 *        1. Set `t->relations_stop.first_donator` to `thread_current`.
 *        2. Set `ret` to `POK_ERRNO_OK`.
 *     2. Set `thread_current->relations_stop.donate_target` to
 *        `t->relations_stop.donate_target`.
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_05},
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_06}
 *     3. Set `thread_current->relations_stop.next_donator` to
 *        `t->relations_stop.next_donator`.
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_05},
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_06}
 *     4. Set `t->relations_stop.next_donator` to `thread_current`.
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_05},
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_06}
 * 12. Otherwise, if `t->relations_stop.first_donator` is not NULL:
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_07}
 *     1. Set `thread_current->relations_stop.donate_target` to `t`.
 *     2. Set `thread_current->relations_stop.next_donator` to
 *        `t->relations_stop.first_donator`.
 *     3. Set `t->relations_stop.first_donator` to `thread_current`.
 * 13. Otherwise, if `tshd_target->msection_count` is not equal to `0U`:
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_08},
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_09}
 *     1. If `t->state` is equal to `POK_STATE_WAITING`:
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_09}
 *        1. Call `thread_wake_up()` with the argument `t`.
 *        2. Set `t->wait_result` to `POK_ERRNO_CANCELLED`.
 *     2. Set `thread_current->relations_stop.donate_target` to `t`.
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_08}
 *     3. Set `thread_current->relations_stop.next_donator` to `NULL`.
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_08}
 *     4. Set `t->relations_stop.first_donator` to `thread_current`.
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_08}
 *     5. Set `ret` to `POK_ERRNO_OK`.
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_08}
 *     6. Set `tshd_target->thread_kernel_flags` to `THREAD_KERNEL_FLAG_KILLED`.
 *        \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_08}
 * 14. Otherwise: \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_10}
 *     1. Set `ret` to `POK_ERRNO_OK`.
 *     2. Call `thread_stop()` with the argument `t`.
 *     3. Go to the label `out`.
 * 15. Call `pok_sched_local_invalidate()`.
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_05},
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_06},
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_07},
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_08}
 * 16. Label `out`: \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_03}
 * 17. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_03}
 * 18. Return `ret`. \llrref{LLR_THREAD_POK_THREAD_STOP_TARGET_03}
 *
 * @calls
 * - `get_thread_by_id()`
 * - `pok_preemption_local_disable()`
 * - `thread_wake_up()`
 * - `thread_stop()`
 * - `pok_sched_local_invalidate()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_stop_target(pok_thread_id_t id)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_ret_e ret = POK_ERRNO_PARAM;

    pok_thread_t *t = get_thread_by_id(id);
    if(t == NULL) {
        return POK_ERRNO_PARAM;
    }

	pok_thread_t* thread_current = part->thread_current;

    // can's stop self
	// use pok_thread_stop to do that
    if (t == thread_current) {
        return POK_ERRNO_THREADATTR;
    }

    jet_thread_shared_data_t* tshd_target = part->kshd->tshd
        + (t - part->threads);

    pok_preemption_local_disable();

    ret = POK_ERRNO_UNAVAILABLE;
    if (t->state == POK_STATE_STOPPED)
    {
        // Target process is already stopped.
        goto out;
    }

    if(t->relations_stop.donate_target != NULL)
    {
        /* target waits other thread to stop. */
        if(t->relations_stop.first_donator == NULL)
        {
            t->relations_stop.first_donator = thread_current;
            ret = POK_ERRNO_OK;
        }

        // Add ourselves into the list of "donators" *after* the target.
        thread_current->relations_stop.donate_target = t->relations_stop.donate_target;
        thread_current->relations_stop.next_donator = t->relations_stop.next_donator;
        t->relations_stop.next_donator = thread_current;
    }
    else if(t->relations_stop.first_donator != NULL)
    {
        /* Someone else waits for the target. */
        // Add ourselves into the beginning of the list of "donators".
        thread_current->relations_stop.donate_target = t;
        thread_current->relations_stop.next_donator = t->relations_stop.first_donator;
        t->relations_stop.first_donator = thread_current;
    }
    else if(tshd_target->msection_count != 0U)
    {
        /* target currently owners the section. Cannot kill it immediately. */
        if(t->state == POK_STATE_WAITING)
        {
            // Interrupt waiting on msection.
            thread_wake_up(t);
            t->wait_result = POK_ERRNO_CANCELLED;
        }

        thread_current->relations_stop.donate_target = t;
        thread_current->relations_stop.next_donator = NULL;
        t->relations_stop.first_donator = thread_current;

        ret = POK_ERRNO_OK;

        tshd_target->thread_kernel_flags = THREAD_KERNEL_FLAG_KILLED;
    }
    else
    {
        // Target thread can be stopped immediately.
        ret = POK_ERRNO_OK;
        thread_stop(t);

        goto out;
    }
    // Notify scheduler that threads cannot continue its execution now.
    pok_sched_local_invalidate();

out:
	pok_preemption_local_enable();

    return ret;
}

/**
 * @funcname `pok_thread_stop`
 *
 * @description
 * Stops the current thread (STOP_SELF); for the error handler, finishes the
 * handling of the error.
 *
 * @return POK_ERRNO_OK.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `t` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_STOP_01}: `pok_thread_stop()` shall stop the
 *   current process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 * - \hlrid{HLR_THREAD_POK_THREAD_STOP_02}: `pok_thread_stop()` shall, for the
 *   error handler, end the handling of the current error and handle the errors
 *   that remain.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_007}
 *   - \srqref{SRQ_JET_HM_009}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_01}: `pok_thread_stop()` shall call
 *   `pok_preemption_local_disable()`, then `thread_stop()` with
 *   `thread_current` of the partition, then `pok_sched_local_invalidate()` and
 *   `pok_preemption_local_enable()`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_01}
 * - \llrid{LLR_THREAD_POK_THREAD_STOP_02}: With `POK_NEEDS_ERROR_HANDLING`, if
 *   and only if the thread is `thread_error` of the partition,
 *   `pok_thread_stop()` shall call `error_check_after_handler()` and then
 *   `error_ignore_sync()` after `thread_stop()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_STOP_02}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_01}
 * 2. Set `t` to `part->thread_current`. \llrref{LLR_THREAD_POK_THREAD_STOP_01}
 * 3. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_01}
 * 4. Call `thread_stop()` with the argument `t`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_01}
 * 5. If `t` is equal to `part->thread_error`:
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_02}
 *    1. Call `error_check_after_handler()`.
 *    2. Call `error_ignore_sync()`.
 * 6. Call `pok_sched_local_invalidate()`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_01}
 * 7. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_THREAD_POK_THREAD_STOP_01}
 * 8. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_THREAD_STOP_01}
 *
 * @calls
 * - `pok_preemption_local_disable()`
 * - `thread_stop()`
 * - `error_check_after_handler()`
 * - `error_ignore_sync()`
 * - `pok_sched_local_invalidate()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_thread_stop(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* t = part->thread_current;

    pok_preemption_local_disable();

    // Thread cannot executed anything in donation state.
    assert(t->relations_stop.donate_target == NULL);
    // While already stopped, thread shouldn't stop itself.
    assert_os(t->relations_stop.first_donator == NULL);
    /*
     * It is *possible* for thread to be stopped while in msection.
     * But this cannot hurt kernel.
     *
     * TODO: Should additional os-check to be added?
     */
    thread_stop(t);

#ifdef POK_NEEDS_ERROR_HANDLING
    if(t == part->thread_error)
    {
        error_check_after_handler();

        error_ignore_sync();
    }
#endif
    // Stopping current thread always change scheduling.
    pok_sched_local_invalidate();
	pok_preemption_local_enable();

	return POK_ERRNO_OK;
}

/**
 * @funcname `pok_thread_find`
 *
 * @description
 * Gives the ID of a thread by its name (GET_PROCESS_ID).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char * JET_USER` | [in] Partition address of the name. |
 * | `id` | `pok_thread_id_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EFAULT for a bad address; POK_ERRNO_EINVAL if
 *         there is no such thread.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_id` | `pok_thread_id_t *` | Kernel address of the result |
 * | `k_name` | `const char *` | Kernel address of the name |
 * | `kernel_name` | `char[30]` | A copy of the name |
 * | `t` | `pok_thread_t *` | The thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_FIND_01}: `pok_thread_find()` shall give the
 *   identifier of the process of the current partition that has a name.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_003}
 * - \hlrid{HLR_THREAD_POK_THREAD_FIND_02}: `pok_thread_find()` shall refuse a
 *   name or an identifier address that the partition may not access.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_FIND_01}: If `jet_user_to_kernel_ro()` returns
 *   NULL for `name` and `MAX_NAME_LENGTH`, `pok_thread_find()` shall return
 *   `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_FIND_02}
 * - \llrid{LLR_THREAD_POK_THREAD_FIND_02}: If the name is readable and
 *   `jet_user_to_kernel_typed()` returns NULL for `id`, `pok_thread_find()`
 *   shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_FIND_02}
 * - \llrid{LLR_THREAD_POK_THREAD_FIND_03}: If both give an address,
 *   `pok_thread_find()` shall copy the `MAX_NAME_LENGTH` bytes of the name to a
 *   kernel buffer with `memcpy()` and return `POK_ERRNO_EINVAL` if
 *   `find_thread()` returns NULL for it.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_FIND_01}
 * - \llrid{LLR_THREAD_POK_THREAD_FIND_04}: If `find_thread()` returns a thread,
 *   `pok_thread_find()` shall store its index in `threads` of the partition at
 *   `id` and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_FIND_01}
 *
 * @design
 * 1. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 *    \llrref{LLR_THREAD_POK_THREAD_FIND_01}
 * 2. If `k_name` is NULL: \llrref{LLR_THREAD_POK_THREAD_FIND_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 *    \llrref{LLR_THREAD_POK_THREAD_FIND_02}
 * 4. If `k_id` is NULL: \llrref{LLR_THREAD_POK_THREAD_FIND_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `memcpy()` with the arguments `kernel_name`, `k_name` and
 *    `MAX_NAME_LENGTH`. \llrref{LLR_THREAD_POK_THREAD_FIND_03},
 *    \llrref{LLR_THREAD_POK_THREAD_FIND_04}
 * 6. Call `find_thread()` with the argument `kernel_name`, and store the result
 *    in `t`. \llrref{LLR_THREAD_POK_THREAD_FIND_03},
 *    \llrref{LLR_THREAD_POK_THREAD_FIND_04}
 * 7. If `t` is NULL: \llrref{LLR_THREAD_POK_THREAD_FIND_03}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 8. Set `*k_id` to `t - current_partition_arinc->threads`.
 *    \llrref{LLR_THREAD_POK_THREAD_FIND_04}
 * 9. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_THREAD_FIND_04}
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_user_to_kernel()`
 * - `memcpy()`
 * - `find_thread()`
 */
pok_ret_e pok_thread_find(const char* JET_USER name,
                          pok_thread_id_t* JET_USER id)
{
    char kernel_name[MAX_NAME_LENGTH];
    pok_thread_t* t;

    const char* JET_KUSER k_name = jet_user_to_kernel_ro(name, MAX_NAME_LENGTH);
    if(k_name == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_thread_id_t* JET_KUSER k_id = jet_user_to_kernel_typed(id);
    if(k_id == NULL) {
        return POK_ERRNO_EFAULT;
    }

    (void) memcpy(kernel_name, k_name, MAX_NAME_LENGTH);

    t = find_thread(kernel_name);

    if(t == NULL) {
        return POK_ERRNO_EINVAL; // TODO: INVALID_CONFIG for ARINC
    }

    *k_id = t - current_partition_arinc->threads;

	return POK_ERRNO_OK;
}

/**
 * @funcname `pok_sched_end_period`
 *
 * @description
 * Ends the current activation of a periodic thread (PERIODIC_WAIT): it waits
 * for its next activation, with its deadline moved.
 *
 * @return POK_ERRNO_OK; POK_ERRNO_MODE for an aperiodic thread or if it may not
 *         wait.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_SCHED_END_PERIOD_01}: `pok_sched_end_period()` shall
 *   make the current periodic process wait for its next activation.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_011}
 * - \hlrid{HLR_THREAD_POK_SCHED_END_PERIOD_02}: `pok_sched_end_period()` shall
 *   move the deadline of the process to its next activation plus its time
 *   capacity.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_011}
 * - \hlrid{HLR_THREAD_POK_SCHED_END_PERIOD_03}: `pok_sched_end_period()` shall
 *   refuse an aperiodic process and one that may not wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_007}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_SCHED_END_PERIOD_01}: If `pok_thread_is_periodic()`
 *   returns false for the current thread, `pok_sched_end_period()` shall return
 *   `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_END_PERIOD_03}
 * - \llrid{LLR_THREAD_POK_SCHED_END_PERIOD_02}: If the thread is periodic and
 *   `thread_is_waiting_allowed()` returns false, `pok_sched_end_period()` shall
 *   return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_END_PERIOD_03}
 * - \llrid{LLR_THREAD_POK_SCHED_END_PERIOD_03}: If both checks pass,
 *   `pok_sched_end_period()` shall call `pok_preemption_local_disable()`, set
 *   `next_activation` of the thread to the result of `pok_time_add()` for it
 *   and `period`, call `thread_wait_timed()` with the thread and
 *   `next_activation`, then `thread_set_deadline()` with the thread and the
 *   result of `pok_time_add()` for `next_activation` and `time_capacity`, then
 *   `pok_preemption_local_enable()`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_END_PERIOD_01}
 *   - \hlrref{HLR_THREAD_POK_SCHED_END_PERIOD_02}
 *
 * @design
 * 1. Set `t` to `current_thread`. \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_01},
 *    \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_02},
 *    \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_03}
 * 2. If `pok_thread_is_periodic(t)` is false:
 *    \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_01}
 *    1. Return `POK_ERRNO_MODE`.
 * 3. If `thread_is_waiting_allowed()` is false:
 *    \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_02}
 *    1. Return `POK_ERRNO_MODE`.
 * 4. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_03}
 * 5. Call `pok_time_add()` with the arguments `t->next_activation` and
 *    `t->period`, and store the result in `t->next_activation`.
 *    \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_03}
 * 6. Call `thread_wait_timed()` with the arguments `t` and
 *    `t->next_activation`. \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_03}
 * 7. Call `thread_set_deadline()` with the arguments `t` and
 *    `pok_time_add(t->next_activation, t->time_capacity)`.
 *    \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_03}
 * 8. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_03}
 * 9. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_POK_SCHED_END_PERIOD_03}
 *
 * @calls
 * - `pok_thread_is_periodic()`
 * - `thread_is_waiting_allowed()`
 * - `pok_preemption_local_disable()`
 * - `pok_time_add()`
 * - `thread_wait_timed()`
 * - `thread_set_deadline()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_sched_end_period(void)
{
    pok_thread_t* t = current_thread;

    if(!pok_thread_is_periodic(t)) {
        return POK_ERRNO_MODE;
    }

	if(!thread_is_waiting_allowed()) {
	    return POK_ERRNO_MODE;
	}

    pok_preemption_local_disable();

    t->next_activation = pok_time_add(t->next_activation, t->period);
	thread_wait_timed(t, t->next_activation);
	thread_set_deadline(t,
	    pok_time_add(t->next_activation, t->time_capacity));

	pok_preemption_local_enable();

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_sched_replenish`
 *
 * @description
 * Moves the current thread's deadline to now plus a budget (REPLENISH); an
 * infinite budget removes the deadline of an aperiodic thread.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `budget` | `const pok_time_t * JET_USER` | [in] Partition address of the budget. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EFAULT for a bad address;
 *         POK_ERRNO_UNAVAILABLE in the error handler or outside NORMAL mode;
 *         POK_ERRNO_MODE if the deadline would pass the next activation (or an
 *         infinite budget of a periodic thread).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `calculated_deadline` | `pok_time_t` | The new deadline |
 * | `k_budget` | `const pok_time_t *` | Kernel address of the budget |
 * | `kernel_budget` | `pok_time_t` | The budget |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `ret` | `pok_ret_e` | The result |
 * | `t` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_SCHED_REPLENISH_01}: `pok_sched_replenish()` shall
 *   move the deadline of the current process to the present time plus a budget,
 *   an infinite budget removing the deadline of an aperiodic process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_022}
 * - \hlrid{HLR_THREAD_POK_SCHED_REPLENISH_02}: `pok_sched_replenish()` shall
 *   refuse it for the error handler, outside `NORMAL` mode, for a periodic
 *   process with an infinite budget or a deadline not before its next
 *   activation, and do nothing for a process without a time capacity.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_022}
 * - \hlrid{HLR_THREAD_POK_SCHED_REPLENISH_03}: `pok_sched_replenish()` shall
 *   refuse a budget address that the partition may not read with
 *   `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_SCHED_REPLENISH_01}: If
 *   `jet_user_to_kernel_typed_ro()` returns NULL for `budget`,
 *   `pok_sched_replenish()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_03}
 * - \llrid{LLR_THREAD_POK_SCHED_REPLENISH_02}: If `budget` is readable and
 *   (with `POK_NEEDS_ERROR_HANDLING`) the current thread is `thread_error`,
 *   `pok_sched_replenish()` shall return `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_02}
 * - \llrid{LLR_THREAD_POK_SCHED_REPLENISH_03}: If the current thread is not
 *   `thread_error` and `mode` of the partition is not
 *   `POK_PARTITION_MODE_NORMAL`, `pok_sched_replenish()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_02}
 * - \llrid{LLR_THREAD_POK_SCHED_REPLENISH_04}: If `mode` is
 *   `POK_PARTITION_MODE_NORMAL` and `pok_time_is_infinity()` returns true for
 *   `time_capacity` of the thread, `pok_sched_replenish()` shall return
 *   `POK_ERRNO_OK` without changing the deadline.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_02}
 * - \llrid{LLR_THREAD_POK_SCHED_REPLENISH_05}: If `time_capacity` is finite,
 *   `pok_sched_replenish()` shall call `pok_preemption_local_disable()` before
 *   it changes the deadline and `pok_preemption_local_enable()` before it
 *   returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_01}
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_02}
 * - \llrid{LLR_THREAD_POK_SCHED_REPLENISH_06}: If the budget (the value at
 *   `budget`) is infinite and `period` of the thread is finite,
 *   `pok_sched_replenish()` shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_01}
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_02}
 * - \llrid{LLR_THREAD_POK_SCHED_REPLENISH_07}: If the budget and `period` are
 *   infinite, `pok_sched_replenish()` shall call `delayed_event_remove()` with
 *   the delayed events of the partition and `thread_deadline_event` of the
 *   thread, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_01}
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_02}
 * - \llrid{LLR_THREAD_POK_SCHED_REPLENISH_08}: If the budget is finite,
 *   `period` is finite and the result of `pok_time_add()` for the result of
 *   `jet_system_time()` and the budget is not less than `next_activation` of
 *   the thread, `pok_sched_replenish()` shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_01}
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_02}
 * - \llrid{LLR_THREAD_POK_SCHED_REPLENISH_09}: If the budget is finite, and
 *   `period` is infinite or that deadline is less than `next_activation`,
 *   `pok_sched_replenish()` shall call `thread_set_deadline()` with the thread
 *   and the deadline, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_01}
 *   - \hlrref{HLR_THREAD_POK_SCHED_REPLENISH_02}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_02},
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_03},
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_07}
 * 2. Set `k_budget` to `jet_user_to_kernel_typed_ro(budget)`.
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_01}
 * 3. If `k_budget` is NULL: \llrref{LLR_THREAD_POK_SCHED_REPLENISH_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `kernel_budget` to the value at `k_budget`.
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_01}
 * 5. Set `t` to `current_thread`. \llrref{LLR_THREAD_POK_SCHED_REPLENISH_02},
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_03},
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_04},
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_05},
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_06},
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_07},
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_08},
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_09}
 * 6. If `t` is equal to `part->thread_error`:
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_02}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 7. If `part->mode` is not equal to `POK_PARTITION_MODE_NORMAL`:
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_03}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 8. If `pok_time_is_infinity(t->time_capacity)` is true:
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_04}
 *    1. Return `POK_ERRNO_OK`.
 * 9. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_POK_SCHED_REPLENISH_05}
 * 10. If `pok_time_is_infinity(kernel_budget)` is true:
 *     \llrref{LLR_THREAD_POK_SCHED_REPLENISH_06},
 *     \llrref{LLR_THREAD_POK_SCHED_REPLENISH_07}
 *     1. If `pok_time_is_infinity(t->period)` is false:
 *        \llrref{LLR_THREAD_POK_SCHED_REPLENISH_06}
 *        1. Set `ret` to `POK_ERRNO_MODE`.
 *        2. Go to the label `out`.
 *     2. Call `delayed_event_remove()` with the arguments the address of
 *        `part->partition_delayed_events` and the address of
 *        `t->thread_deadline_event`. \llrref{LLR_THREAD_POK_SCHED_REPLENISH_07}
 *     3. Set `ret` to `POK_ERRNO_OK`.
 *        \llrref{LLR_THREAD_POK_SCHED_REPLENISH_07}
 * 11. Otherwise: \llrref{LLR_THREAD_POK_SCHED_REPLENISH_08},
 *     \llrref{LLR_THREAD_POK_SCHED_REPLENISH_09}
 *     1. Call `pok_time_add()` with the arguments `jet_system_time()` and
 *        `kernel_budget`, and store the result in `calculated_deadline`.
 *     2. If `pok_time_is_infinity(t->period)` is false and
 *        `calculated_deadline` is greater than or equal to
 *        `t->next_activation`: \llrref{LLR_THREAD_POK_SCHED_REPLENISH_08}
 *        1. Set `ret` to `POK_ERRNO_MODE`.
 *        2. Go to the label `out`.
 *     3. Call `thread_set_deadline()` with the arguments `t` and
 *        `calculated_deadline`. \llrref{LLR_THREAD_POK_SCHED_REPLENISH_09}
 *     4. Set `ret` to `POK_ERRNO_OK`.
 *        \llrref{LLR_THREAD_POK_SCHED_REPLENISH_09}
 * 12. Label `out`: \llrref{LLR_THREAD_POK_SCHED_REPLENISH_06},
 *     \llrref{LLR_THREAD_POK_SCHED_REPLENISH_08}
 * 13. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_POK_SCHED_REPLENISH_05}
 * 14. Return `ret`. \llrref{LLR_THREAD_POK_SCHED_REPLENISH_06},
 *     \llrref{LLR_THREAD_POK_SCHED_REPLENISH_07},
 *     \llrref{LLR_THREAD_POK_SCHED_REPLENISH_08},
 *     \llrref{LLR_THREAD_POK_SCHED_REPLENISH_09}
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `pok_time_is_infinity()`
 * - `pok_preemption_local_disable()`
 * - `delayed_event_remove()`
 * - `pok_time_add()`
 * - `ja_system_time()`
 * - `thread_set_deadline()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_sched_replenish(const pok_time_t* JET_USER budget)
{
    pok_ret_e ret;

    pok_partition_arinc_t* part = current_partition_arinc;

    const pok_time_t* JET_KUSER k_budget = jet_user_to_kernel_typed_ro(budget);
    if(k_budget == NULL) {
        return POK_ERRNO_EFAULT;
    }
    pok_time_t kernel_budget = *k_budget;

    pok_thread_t* t = current_thread;

#ifdef POK_NEEDS_ERROR_HANDLING
    if(t == part->thread_error) {
        return POK_ERRNO_UNAVAILABLE;
    }
#endif

    if(part->mode != POK_PARTITION_MODE_NORMAL) {
		return POK_ERRNO_UNAVAILABLE;
    }

    if(pok_time_is_infinity(t->time_capacity)) {
        return POK_ERRNO_OK; //nothing to do
    }

    pok_preemption_local_disable();

    if(pok_time_is_infinity(kernel_budget))
    {
        if(!pok_time_is_infinity(t->period))
        {
            ret = POK_ERRNO_MODE;
            goto out;
        }

        delayed_event_remove(&part->partition_delayed_events,
            &t->thread_deadline_event);
        ret = POK_ERRNO_OK;
    }
    else
    {
        pok_time_t calculated_deadline =
            pok_time_add(jet_system_time(), kernel_budget);

        if(!pok_time_is_infinity(t->period)
            && (calculated_deadline >= t->next_activation))
        {
            ret = POK_ERRNO_MODE;
            goto out;
        }
        thread_set_deadline(t, calculated_deadline);
        ret = POK_ERRNO_OK;
    }
out:
    pok_preemption_local_enable();

    return ret;
}


/**
 * @funcname `assert_thread_id`
 *
 * @description
 * Checks that a thread ID read from shared memory is a thread of the partition
 * (a partition error otherwise).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_thread_id_t` | [in] The thread ID. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_ASSERT_THREAD_ID_01}: `assert_thread_id()` shall treat a
 *   process identifier read from the memory of the partition that is not that
 *   of a created process as an error of the partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_001}
 *   - \srqref{SRQ_JET_HM_008}
 *
 * @llr
 * - \llrid{LLR_THREAD_ASSERT_THREAD_ID_01}: `assert_thread_id()` shall return
 *   without changing any object.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_ASSERT_THREAD_ID_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_ASSERT_THREAD_ID_01}
 *
 * @calls
 * None.
 */
static void assert_thread_id(pok_thread_id_t id)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    assert_os(id < part->nthreads_used);
}

/**
 * @funcname `jet_msection_enter_helper`
 *
 * @description
 * Helps the current thread enter an msection that another thread owns: marks it
 * entering, so that the scheduler runs the owner until it leaves.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t * JET_USER` | [in,out] Partition address of the msection. |
 *
 * @return POK_ERRNO_OK.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msection_entering` | `msection_t *` | Kernel address of the msection |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `thread_current` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_JET_MSECTION_ENTER_HELPER_01}:
 *   `jet_msection_enter_helper()` shall make the current process, which wants
 *   to enter a critical section that another process owns, an entering process,
 *   so that the owner runs until it leaves the section.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_001}
 *
 * @llr
 * - \llrid{LLR_THREAD_JET_MSECTION_ENTER_HELPER_01}:
 *   `jet_msection_enter_helper()` shall call `jet_user_to_kernel_typed()` for
 *   `section`, then `pok_preemption_local_disable()`, and
 *   `pok_preemption_local_enable()` at the end, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_ENTER_HELPER_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_ENTER_HELPER_02}: If `owner` of the section
 *   is neither `JET_THREAD_ID_NONE` nor the index of `thread_current` of the
 *   partition, `jet_msection_enter_helper()` shall set `msection_entering` of
 *   the current thread to the section and call `pok_sched_local_invalidate()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_ENTER_HELPER_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_ENTER_HELPER_03}: If `owner` of the section
 *   is `JET_THREAD_ID_NONE` or the index of the current thread,
 *   `jet_msection_enter_helper()` shall leave `msection_entering` of the thread
 *   as it is and not call `pok_sched_local_invalidate()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_ENTER_HELPER_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_01}
 * 2. Set `thread_current` to `part->thread_current`.
 *    \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_01}
 * 3. Set `msection_entering` to `jet_user_to_kernel_typed(section)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_01}
 * 4. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_01}
 * 5. If `msection_entering->owner` is not equal to `JET_THREAD_ID_NONE` and
 *    `msection_entering->owner` is not equal to
 *    `(thread_current - part->threads)`:
 *    \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_02},
 *    \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_03}
 *    1. Set `thread_current->msection_entering` to `msection_entering`.
 *       \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_02}
 *    2. Call `pok_sched_local_invalidate()`.
 *       \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_02}
 * 6. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_01}
 * 7. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_JET_MSECTION_ENTER_HELPER_01}
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `pok_preemption_local_disable()`
 * - `pok_sched_local_invalidate()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e jet_msection_enter_helper(msection_t* JET_USER section)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread_current = part->thread_current;

    msection_t* JET_KUSER msection_entering =
        jet_user_to_kernel_typed(section);

    assert_os(msection_entering != NULL);

    pok_preemption_local_disable();

    if((msection_entering->owner != JET_THREAD_ID_NONE)
        && (msection_entering->owner != (thread_current - part->threads)))
    {
        // Set thread as entering into the section ...
        thread_current->msection_entering = msection_entering;
        // And let scheduler to do all the work.
        pok_sched_local_invalidate();
    }

    pok_preemption_local_enable();

    return POK_ERRNO_OK;
}

/**
 * @funcname `jet_msection_wait`
 *
 * @description
 * Makes the current thread, owner of an msection, release it and wait (with a
 * timeout) until it is notified; it then enters the msection again.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t * JET_USER` | [in,out] Partition address of the msection. |
 * | `timeout` | `const pok_time_t * JET_USER` | [in] Partition address of the timeout. |
 *
 * @return The wait result (POK_ERRNO_OK when notified, POK_ERRNO_TIMEOUT,
 *         POK_ERRNO_CANCELLED if the thread is being stopped); POK_ERRNO_EFAULT
 *         for a bad address; POK_ERRNO_MODE if the thread may not wait.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kernel_timeout` | `const pok_time_t *` | Kernel address of the timeout |
 * | `msection_entering` | `msection_t *` | Kernel address of the msection |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `thread_current` | `pok_thread_t *` | The current thread |
 * | `tshd_current` | `jet_thread_shared_data_t *` | The current thread's shared data |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_JET_MSECTION_WAIT_01}: `jet_msection_wait()` shall make
 *   the current process, owner of a critical section, release it and wait, up
 *   to a time-out, until another process wakes it, and give the result of the
 *   wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_002}
 * - \hlrid{HLR_THREAD_JET_MSECTION_WAIT_02}: `jet_msection_wait()` shall make
 *   the process enter the section again when it wakes up.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_007}
 * - \hlrid{HLR_THREAD_JET_MSECTION_WAIT_03}: `jet_msection_wait()` shall refuse
 *   it while the partition is locked or in the error handler, and cancel it for
 *   a process that another one is stopping.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_007}
 *   - \srqref{SRQ_JET_SYNC_005}
 * - \hlrid{HLR_THREAD_JET_MSECTION_WAIT_04}: `jet_msection_wait()` shall refuse
 *   a time-out address that the partition may not read with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_THREAD_JET_MSECTION_WAIT_01}: If `jet_user_to_kernel_typed_ro()`
 *   returns NULL for `timeout`, `jet_msection_wait()` shall return
 *   `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WAIT_04}
 * - \llrid{LLR_THREAD_JET_MSECTION_WAIT_02}: If `timeout` is readable, and
 *   `lock_level` of the partition is not 0 or (with `POK_NEEDS_ERROR_HANDLING`)
 *   `thread_error` is the current thread, `jet_msection_wait()` shall return
 *   `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WAIT_03}
 * - \llrid{LLR_THREAD_JET_MSECTION_WAIT_03}: If these checks pass and
 *   `first_donator` of the current thread is not NULL, `jet_msection_wait()`
 *   shall set its `wait_result` to `POK_ERRNO_CANCELLED` and return it, without
 *   waiting.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WAIT_03}
 * - \llrid{LLR_THREAD_JET_MSECTION_WAIT_04}: If these checks pass and
 *   `first_donator` of the current thread is NULL, `jet_msection_wait()` shall
 *   set its `msection_entering` to the section (the result of
 *   `jet_user_to_kernel_typed()` for `section`), the `owner` of the section to
 *   `JET_THREAD_ID_NONE`, call `thread_wait_common()` with the thread and the
 *   time-out (the value at `timeout`), then `pok_sched_local_invalidate()`, and
 *   return `wait_result` of the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WAIT_01}
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WAIT_02}
 * - \llrid{LLR_THREAD_JET_MSECTION_WAIT_05}: If these checks pass,
 *   `jet_msection_wait()` shall call `pok_preemption_local_disable()` before it
 *   reads `first_donator` and `pok_preemption_local_enable()` before it
 *   returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WAIT_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_02},
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_03},
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 * 2. Set `thread_current` to `part->thread_current`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_02},
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_03},
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 * 3. Set `kernel_timeout` to `jet_user_to_kernel_typed_ro(timeout)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_01}
 * 4. If `kernel_timeout` is NULL: \llrref{LLR_THREAD_JET_MSECTION_WAIT_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `msection_entering` to `jet_user_to_kernel_typed(section)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 * 6. Set `tshd_current` to
 *    `part->kshd->tshd + (thread_current - part->threads)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 * 7. If `(part->lock_level != 0U) #ifdef POK_NEEDS_ERROR_HANDLING ||`
 *    `(part->thread_error == thread_current)` is true:
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_02}
 *    1. Return `POK_ERRNO_MODE`.
 * 8. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_05}
 * 9. If `thread_current->relations_stop.first_donator` is not NULL:
 *    \llrref{LLR_THREAD_JET_MSECTION_WAIT_03}
 *    1. Set `thread_current->wait_result` to `POK_ERRNO_CANCELLED`.
 *    2. Go to the label `out`.
 * 10. Set `thread_current->msection_entering` to `msection_entering`.
 *     \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 * 11. Set `msection_entering->owner` to `JET_THREAD_ID_NONE`.
 *     \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 * 12. Call `thread_wait_common()` with the arguments `thread_current` and the
 *     value at `kernel_timeout`. \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 * 13. Call `pok_sched_local_invalidate()`.
 *     \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 * 14. Label `out`: \llrref{LLR_THREAD_JET_MSECTION_WAIT_03},
 *     \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 * 15. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_JET_MSECTION_WAIT_05}
 * 16. Return `thread_current->wait_result`.
 *     \llrref{LLR_THREAD_JET_MSECTION_WAIT_03},
 *     \llrref{LLR_THREAD_JET_MSECTION_WAIT_04}
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_user_to_kernel()`
 * - `pok_preemption_local_disable()`
 * - `thread_wait_common()`
 * - `pok_sched_local_invalidate()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e jet_msection_wait(msection_t* JET_USER section,
    const pok_time_t* JET_USER timeout)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread_current = part->thread_current;

    const pok_time_t* JET_KUSER kernel_timeout =
        jet_user_to_kernel_typed_ro(timeout);

    if(kernel_timeout == NULL) {
        return POK_ERRNO_EFAULT;
    }

    msection_t* JET_KUSER msection_entering =
        jet_user_to_kernel_typed(section);

    // Passing wrong section is OS error, not a user.
    assert_os(msection_entering != NULL);
    // The thread should be a section's owner.
    assert_os(msection_entering->owner == (thread_current - part->threads));

    jet_thread_shared_data_t* tshd_current = part->kshd->tshd
        + (thread_current - part->threads);
    // The only section should be entered.
    assert_os(tshd_current->msection_count == 1U);

    // Cannot use thread_is_waiting_allowed for wait on msection.
    // In the INIT_* mode lock level is positive, no need to check it
    // explicitely.
    if((part->lock_level != 0U)
#ifdef POK_NEEDS_ERROR_HANDLING
        || (part->thread_error == thread_current) /* error thread cannot wait */
#endif
    ) {
        return POK_ERRNO_MODE;
    }

    pok_preemption_local_disable();

    if(thread_current->relations_stop.first_donator != NULL)
    {
        thread_current->wait_result = POK_ERRNO_CANCELLED;
        goto out;
    }

    thread_current->msection_entering = msection_entering;
    // Release section...
    msection_entering->owner = JET_THREAD_ID_NONE;
    // ... And use common wait.
    thread_wait_common(thread_current, *kernel_timeout);

    /*
     * It is possible, that current thread wasn't the highest-priority
     * thread. Because of that, `thread_wait_common` may do not cause
     * scheduling invalidation.
     *
     * From the other side, waiting on msection and leaving msection
     * are the only possible state-modifications for non-highest-priority
     * thread. Normal msection leaving is followed by jet_resched(),
     * which invalidates scheduling.
     *
     * So, explicitely invalidate scheduling here.
     */
    pok_sched_local_invalidate();
    // After the releasing we will be in common 'msection_entering' state.
out:
    pok_preemption_local_enable();

    return thread_current->wait_result;
}

/**
 * @funcname `jet_msection_notify`
 *
 * @description
 * Wakes up a thread that waits on the msection the current thread owns.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t * JET_USER` | [in] Partition address of the msection. |
 * | `thread_id` | `pok_thread_id_t` | [in] The thread to wake up. |
 *
 * @return POK_ERRNO_OK, or POK_ERRNO_EXISTS if the thread doesn't wait.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msection_entering` | `msection_t *` | The msection the thread waits on |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `ret` | `pok_ret_e` | The result |
 * | `section_kernel` | `msection_t *` | Kernel address of the msection |
 * | `t` | `pok_thread_t *` | The thread to wake up |
 * | `thread_current` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_JET_MSECTION_NOTIFY_01}: `jet_msection_notify()` shall
 *   wake a process that waits on the critical section that the current process
 *   owns, with the result OK, and report a process that does not wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_003}
 *
 * @llr
 * - \llrid{LLR_THREAD_JET_MSECTION_NOTIFY_01}: `jet_msection_notify()` shall
 *   call `jet_user_to_kernel_typed()` for `section` and `assert_thread_id()`
 *   with `thread_id`, then `pok_preemption_local_disable()`, and
 *   `pok_preemption_local_enable()` before it returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_NOTIFY_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_NOTIFY_02}: If `state` of
 *   `threads[thread_id]` of the partition is not `POK_STATE_WAITING`,
 *   `jet_msection_notify()` shall return `POK_ERRNO_EXISTS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_NOTIFY_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_NOTIFY_03}: If `state` of
 *   `threads[thread_id]` is `POK_STATE_WAITING`, `jet_msection_notify()` shall
 *   call `thread_wake_up()` with that thread, set its `wait_result` to
 *   `POK_ERRNO_OK` and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_NOTIFY_01}
 *
 * @design
 * 1. Set `ret` to `POK_ERRNO_EXISTS`.
 *    \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_02}
 * 2. Set `section_kernel` to `jet_user_to_kernel_typed(section)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_01}
 * 3. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_01}
 * 4. Set `thread_current` to `part->thread_current`.
 *    \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_01}
 * 5. Call `assert_thread_id()` with the argument `thread_id`.
 *    \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_01}
 * 6. Set `t` to the address of `part->threads[thread_id]`.
 *    \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_01}
 * 7. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_01}
 * 8. If `t->state` is not equal to `POK_STATE_WAITING`:
 *    \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_02}
 *    1. Go to the label `out`.
 * 9. Set `msection_entering` to `t->msection_entering`.
 *    \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_03}
 * 10. Call `thread_wake_up()` with the argument `t`.
 *     \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_03}
 * 11. Set `t->wait_result` to `POK_ERRNO_OK`.
 *     \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_03}
 * 12. Set `ret` to `POK_ERRNO_OK`. \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_03}
 * 13. Label `out`: \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_02},
 *     \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_03}
 * 14. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_01}
 * 15. Return `ret`. \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_02},
 *     \llrref{LLR_THREAD_JET_MSECTION_NOTIFY_03}
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `assert_thread_id()`
 * - `pok_preemption_local_disable()`
 * - `thread_wake_up()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e jet_msection_notify(msection_t* JET_USER section,
    pok_thread_id_t thread_id)
{
    pok_ret_e ret = POK_ERRNO_EXISTS;

    msection_t* JET_KUSER section_kernel =
        jet_user_to_kernel_typed(section);

    assert_os(section_kernel != NULL);

    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread_current = part->thread_current;

    assert_thread_id(thread_id);

    pok_thread_t* t = &part->threads[thread_id];

    pok_preemption_local_disable();

    if(t->state != POK_STATE_WAITING) {
        goto out;
    }

    msection_t* JET_KUSER msection_entering = t->msection_entering;

    /*
     * Currently we require that msection should corresponds to
     * both current and awoken thread.
     */
    assert_os(section_kernel == msection_entering);
    assert_os(section_kernel->owner == (thread_current - part->threads));

    thread_wake_up(t);

    t->wait_result = POK_ERRNO_OK;
    ret = POK_ERRNO_OK;

out:
    pok_preemption_local_enable();

    return ret;
}

/**
 * @funcname `msection_wq_del`
 *
 * @description
 * Removes a thread from an msection wait queue (a list of thread IDs in shared
 * memory).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `msection_wq_t *` | [out] The wait queue. |
 * | `tshd_t` | `jet_thread_shared_data_t *` | [in,out] The thread's shared data. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `next_id` | `pok_thread_id_t` | The next thread in the queue |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `pnext` | `pok_thread_id_t *` | The link that points back to the thread |
 * | `pprev` | `pok_thread_id_t *` | The link that points to the thread |
 * | `prev_id` | `pok_thread_id_t` | The previous thread in the queue |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_MSECTION_WQ_DEL_01}: `msection_wq_del()` shall remove a
 *   process from the wait queue of a critical section kept in the memory of the
 *   partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_003}
 *
 * @llr
 * - \llrid{LLR_THREAD_MSECTION_WQ_DEL_01}: If `wq_next` of `tshd_t` is
 *   `JET_THREAD_ID_NONE`, `msection_wq_del()` shall set `last` of `wq` to
 *   `wq_prev` of `tshd_t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_MSECTION_WQ_DEL_01}
 * - \llrid{LLR_THREAD_MSECTION_WQ_DEL_02}: If `wq_next` of `tshd_t` is not
 *   `JET_THREAD_ID_NONE`, `msection_wq_del()` shall call `assert_thread_id()`
 *   with it and set `wq_prev` of the shared data of that thread to `wq_prev` of
 *   `tshd_t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_MSECTION_WQ_DEL_01}
 * - \llrid{LLR_THREAD_MSECTION_WQ_DEL_03}: If `wq_prev` of `tshd_t` is
 *   `JET_THREAD_ID_NONE`, `msection_wq_del()` shall set `first` of `wq` to
 *   `wq_next` of `tshd_t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_MSECTION_WQ_DEL_01}
 * - \llrid{LLR_THREAD_MSECTION_WQ_DEL_04}: If `wq_prev` of `tshd_t` is not
 *   `JET_THREAD_ID_NONE`, `msection_wq_del()` shall call `assert_thread_id()`
 *   with it and set `wq_next` of the shared data of that thread to `wq_next` of
 *   `tshd_t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_MSECTION_WQ_DEL_01}
 * - \llrid{LLR_THREAD_MSECTION_WQ_DEL_05}: `msection_wq_del()` shall then set
 *   `wq_prev` and `wq_next` of `tshd_t` to `JET_THREAD_ID_NONE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_MSECTION_WQ_DEL_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_01},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_02},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_03},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_04}
 * 2. Set `next_id` to `tshd_t->wq_next`.
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_01},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_02},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_03},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_04}
 * 3. Set `prev_id` to `tshd_t->wq_prev`.
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_01},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_02},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_03},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_04}
 * 4. If `next_id` is equal to `JET_THREAD_ID_NONE`:
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_01}
 *    1. Set `pnext` to the address of `wq->last`.
 * 5. Otherwise: \llrref{LLR_THREAD_MSECTION_WQ_DEL_02}
 *    1. Call `assert_thread_id()` with the argument `next_id`.
 *    2. Set `pnext` to the address of `part->kshd->tshd[next_id].wq_prev`.
 * 6. If `prev_id` is equal to `JET_THREAD_ID_NONE`:
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_03}
 *    1. Set `pprev` to the address of `wq->first`.
 * 7. Otherwise: \llrref{LLR_THREAD_MSECTION_WQ_DEL_04}
 *    1. Call `assert_thread_id()` with the argument `prev_id`.
 *    2. Set `pprev` to the address of `part->kshd->tshd[prev_id].wq_next`.
 * 8. Set `*pnext` to `prev_id`. \llrref{LLR_THREAD_MSECTION_WQ_DEL_01},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_02}
 * 9. Set `*pprev` to `next_id`. \llrref{LLR_THREAD_MSECTION_WQ_DEL_03},
 *    \llrref{LLR_THREAD_MSECTION_WQ_DEL_04}
 * 10. Set `tshd_t->wq_prev` to `JET_THREAD_ID_NONE`.
 *     \llrref{LLR_THREAD_MSECTION_WQ_DEL_05}
 * 11. Set `tshd_t->wq_next` to `JET_THREAD_ID_NONE`.
 *     \llrref{LLR_THREAD_MSECTION_WQ_DEL_05}
 *
 * @calls
 * - `assert_thread_id()`
 */
static void msection_wq_del(msection_wq_t* wq,
    jet_thread_shared_data_t* tshd_t)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_thread_id_t next_id = tshd_t->wq_next;
    pok_thread_id_t prev_id = tshd_t->wq_prev;

    pok_thread_id_t *pnext;
    pok_thread_id_t *pprev;

    if(next_id == JET_THREAD_ID_NONE)
    {
        pnext = &wq->last;
    }
    else
    {
        assert_thread_id(next_id);
        pnext = &part->kshd->tshd[next_id].wq_prev;
    }

    if(prev_id == JET_THREAD_ID_NONE)
    {
        pprev = &wq->first;
    }
    else
    {
        assert_thread_id(prev_id);
        pprev = &part->kshd->tshd[prev_id].wq_next;
    }

    *pnext = prev_id;
    *pprev = next_id;

    tshd_t->wq_prev = JET_THREAD_ID_NONE;
    tshd_t->wq_next = JET_THREAD_ID_NONE;
}

/**
 * @funcname `jet_msection_wq_notify`
 *
 * @description
 * Wakes up the waiting threads of an msection wait queue (the first one, or
 * all); threads in the queue that don't wait are removed from it. Only the
 * owner of the msection may call it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t * JET_USER` | [in] Partition address of the msection. |
 * | `wq` | `msection_wq_t * JET_USER` | [in,out] Partition address of the wait queue. |
 * | `is_all` | `pok_bool_t` | [in] TRUE to wake up every waiting thread. |
 *
 * @return POK_ERRNO_OK if a thread was woken up, POK_ERRNO_EMPTY otherwise.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `ret` | `pok_ret_e` | The result |
 * | `section_kernel` | `msection_t *` | Kernel address of the msection |
 * | `t` | `pok_thread_t *` | The thread looked at |
 * | `thread_current` | `pok_thread_t *` | The current thread |
 * | `thread_id` | `pok_thread_id_t` | The thread looked at |
 * | `tshd_t` | `jet_thread_shared_data_t *` | Its shared data |
 * | `wq_kernel` | `msection_wq_t *` | Kernel address of the wait queue |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}: `jet_msection_wq_notify()`
 *   shall wake the first, or all, of the processes that wait in a wait queue of
 *   the critical section that the current process owns, removing from the queue
 *   those that no longer wait, and tell whether one was woken.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_003}
 *
 * @llr
 * - \llrid{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}: `jet_msection_wq_notify()`
 *   shall call `jet_user_to_kernel_typed()` for `section` and for `wq`, then
 *   `pok_preemption_local_disable()` before it walks the queue and
 *   `pok_preemption_local_enable()` after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_02}: For each thread of the queue
 *   (from `first` of the queue on, following `wq_next` of the shared data of
 *   each thread, with `assert_thread_id()` for each) whose `state` is not
 *   `POK_STATE_WAITING`, `jet_msection_wq_notify()` shall call
 *   `msection_wq_del()` with the queue and the shared data of the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_03}: For each thread of the queue
 *   whose `state` is `POK_STATE_WAITING`, `jet_msection_wq_notify()` shall call
 *   `thread_wake_up()` with it and set its `wait_result` to `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_04}: If `is_all` is false,
 *   `jet_msection_wq_notify()` shall stop the walk after the first thread that
 *   it wakes.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_05}: If `is_all` is true,
 *   `jet_msection_wq_notify()` shall walk the queue to its end.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_06}: `jet_msection_wq_notify()`
 *   shall return `POK_ERRNO_OK` if it woke a thread, and `POK_ERRNO_EMPTY` if
 *   it woke none.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 *
 * @design
 * 1. Set `ret` to `POK_ERRNO_EMPTY`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_06}
 * 2. Set `section_kernel` to `jet_user_to_kernel_typed(section)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * 3. Set `wq_kernel` to `jet_user_to_kernel_typed(wq)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * 4. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * 5. Set `thread_current` to `part->thread_current`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * 6. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * 7. Set `thread_id` to `wq_kernel->first`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_02},
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_03}
 * 8. While `thread_id` is not equal to `JET_THREAD_ID_NONE`:
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_02},
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_03},
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_04},
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_05}
 *    1. Call `assert_thread_id()` with the argument `thread_id`.
 *    2. Set `t` to the address of `part->threads[thread_id]`.
 *    3. Set `tshd_t` to the address of `part->kshd->tshd[thread_id]`.
 *    4. Set `thread_id` to `tshd_t->wq_next`.
 *    5. If `t->state` is not equal to `POK_STATE_WAITING`:
 *       \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_02}
 *       1. Call `msection_wq_del()` with the arguments `wq_kernel` and
 *          `tshd_t`.
 *    6. Otherwise: \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_03}
 *       1. Call `thread_wake_up()` with the argument `t`.
 *       2. Set `t->wait_result` to `POK_ERRNO_OK`.
 *       3. Set `ret` to `POK_ERRNO_OK`.
 *       4. If `is_all` is false: \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_04},
 *          \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_05}
 *          1. Leave the loop. \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_04}
 * 9. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_01}
 * 10. Return `ret`. \llrref{LLR_THREAD_JET_MSECTION_WQ_NOTIFY_06}
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `pok_preemption_local_disable()`
 * - `assert_thread_id()`
 * - `msection_wq_del()`
 * - `thread_wake_up()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e jet_msection_wq_notify(msection_t* JET_USER section,
   msection_wq_t* JET_USER wq,
   pok_bool_t is_all)
{
    pok_ret_e ret = POK_ERRNO_EMPTY;

    msection_t* JET_KUSER section_kernel =
        jet_user_to_kernel_typed(section);
    assert_os(section_kernel != NULL);

    msection_wq_t* JET_KUSER wq_kernel = jet_user_to_kernel_typed(wq);
    assert_os(wq_kernel != NULL);

    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread_current = part->thread_current;

    assert_os(section_kernel->owner == (thread_current - part->threads));

    pok_preemption_local_disable();

    pok_thread_id_t thread_id = wq_kernel->first;

    /* TODO: Assert that linkage is correct, so there is no loops in it. */
    while(thread_id != JET_THREAD_ID_NONE)
    {
        assert_thread_id(thread_id);

        pok_thread_t* t = &part->threads[thread_id];
        // The queue is in shared memory, written by the user.
        assert_os(t->msection_entering == section_kernel);

        jet_thread_shared_data_t* tshd_t = &part->kshd->tshd[thread_id];

        thread_id = tshd_t->wq_next;

        if(t->state != POK_STATE_WAITING)
        {
            msection_wq_del(wq_kernel, tshd_t);
        }
        else
        {
            thread_wake_up(t);
            t->wait_result = POK_ERRNO_OK;
            ret = POK_ERRNO_OK;

            if(!is_all) {
                break;
            }
        }
    }

    pok_preemption_local_enable();

    return ret;
}

/**
 * @funcname `jet_msection_wq_size`
 *
 * @description
 * Counts the waiting threads of an msection wait queue; threads in the queue
 * that don't wait are removed from it. Only the owner of the msection may call
 * it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t * JET_USER` | [in] Partition address of the msection. |
 * | `wq` | `msection_wq_t * JET_USER` | [in,out] Partition address of the wait queue. |
 * | `size` | `size_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `size_t` | The number of waiting threads |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `section_kernel` | `msection_t *` | Kernel address of the msection |
 * | `size_kernel` | `size_t *` | Kernel address of the result |
 * | `t` | `pok_thread_t *` | The thread looked at |
 * | `thread_current` | `pok_thread_t *` | The current thread |
 * | `thread_id` | `pok_thread_id_t` | The thread looked at |
 * | `tshd_t` | `jet_thread_shared_data_t *` | Its shared data |
 * | `wq_kernel` | `msection_wq_t *` | Kernel address of the wait queue |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_JET_MSECTION_WQ_SIZE_01}: `jet_msection_wq_size()` shall
 *   tell how many processes wait in a wait queue of the critical section that
 *   the current process owns, removing from the queue those that no longer
 *   wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYNC_003}
 *
 * @llr
 * - \llrid{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}: `jet_msection_wq_size()` shall
 *   call `jet_user_to_kernel_typed()` for `section`, `wq` and `size`, then
 *   `pok_preemption_local_disable()` before it walks the queue and
 *   `pok_preemption_local_enable()` after it, store the number of waiting
 *   threads at `size` and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_WQ_SIZE_02}: For each thread of the queue
 *   (from `first` of the queue on, following `wq_next` of the shared data of
 *   each thread, with `assert_thread_id()` for each) whose `state` is not
 *   `POK_STATE_WAITING`, `jet_msection_wq_size()` shall call
 *   `msection_wq_del()` with the queue and the shared data of the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * - \llrid{LLR_THREAD_JET_MSECTION_WQ_SIZE_03}: For each thread of the queue
 *   whose `state` is `POK_STATE_WAITING`, `jet_msection_wq_size()` shall count
 *   it.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 *
 * @design
 * 1. Set `count` to `0`. \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 2. Set `section_kernel` to `jet_user_to_kernel_typed(section)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 3. Set `wq_kernel` to `jet_user_to_kernel_typed(wq)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 4. Set `size_kernel` to `jet_user_to_kernel_typed(size)`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 5. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 6. Set `thread_current` to `part->thread_current`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 7. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 8. Set `thread_id` to `wq_kernel->first`.
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 9. While `thread_id` is not equal to `JET_THREAD_ID_NONE`:
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_02},
 *    \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_03}
 *    1. Call `assert_thread_id()` with the argument `thread_id`.
 *    2. Set `t` to the address of `part->threads[thread_id]`.
 *    3. Set `tshd_t` to the address of `part->kshd->tshd[thread_id]`.
 *    4. Set `thread_id` to `tshd_t->wq_next`.
 *    5. If `t->state` is not equal to `POK_STATE_WAITING`:
 *       \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_02}
 *       1. Call `msection_wq_del()` with the arguments `wq_kernel` and
 *          `tshd_t`.
 *    6. Otherwise: \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_03}
 *       1. Increment `count`.
 * 10. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 11. Set `*size_kernel` to `count`.
 *     \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 * 12. Return `POK_ERRNO_OK`. \llrref{LLR_THREAD_JET_MSECTION_WQ_SIZE_01}
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `pok_preemption_local_disable()`
 * - `assert_thread_id()`
 * - `msection_wq_del()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e jet_msection_wq_size(msection_t* JET_USER section,
   msection_wq_t* JET_USER wq,
   size_t* JET_USER size)
{
    size_t count = 0;

    msection_t* JET_KUSER section_kernel =
        jet_user_to_kernel_typed(section);
    assert_os(section_kernel != NULL);

    msection_wq_t* JET_KUSER wq_kernel = jet_user_to_kernel_typed(wq);
    assert_os(wq_kernel != NULL);

    size_t* size_kernel = jet_user_to_kernel_typed(size);
    assert_os(size_kernel != NULL);

    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread_current = part->thread_current;

    assert_os(section_kernel->owner == (thread_current - part->threads));

    pok_preemption_local_disable();

    pok_thread_id_t thread_id = wq_kernel->first;

    /* TODO: Assert that linkage is correct, so there is no loops in it. */
    while(thread_id != JET_THREAD_ID_NONE)
    {
        assert_thread_id(thread_id);

        pok_thread_t* t = &part->threads[thread_id];
        // The queue is in shared memory, written by the user.
        assert_os(t->msection_entering == section_kernel);

        jet_thread_shared_data_t* tshd_t = &part->kshd->tshd[thread_id];

        thread_id = tshd_t->wq_next;

        if(t->state != POK_STATE_WAITING)
        {
            msection_wq_del(wq_kernel, tshd_t);
        }
        else
        {
            count++;
        }
    }

    pok_preemption_local_enable();

    *size_kernel = count;

    return POK_ERRNO_OK;
}


/**
 * @funcname `pok_thread_wq_init`
 *
 * @description
 * Initializes a thread wait queue (of a port), empty.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `pok_thread_wq_t *` | [out] The wait queue. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_WQ_INIT_01}: `pok_thread_wq_init()` shall make
 *   a wait queue of a port empty.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_012}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_WQ_INIT_01}: `pok_thread_wq_init()` shall call
 *   `INIT_LIST_HEAD()` once with the address of `waits` of `wq`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_WQ_INIT_01}
 *
 * @design
 * 1. Call `INIT_LIST_HEAD()` with the address of `wq->waits`.
 *    \llrref{LLR_THREAD_POK_THREAD_WQ_INIT_01}
 *
 * @calls
 * - `INIT_LIST_HEAD()`
 */
void pok_thread_wq_init(pok_thread_wq_t* wq)
{
    INIT_LIST_HEAD(&wq->waits);
}

/**
 * @funcname `pok_thread_wq_add`
 *
 * @description
 * Adds a thread at the end of a wait queue (FIFO order).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `pok_thread_wq_t *` | [in,out] The wait queue. |
 * | `t` | `pok_thread_t *` | [out] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_WQ_ADD_01}: `pok_thread_wq_add()` shall add a
 *   process at the end of a wait queue, for the order in which processes began
 *   to wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_012}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_WQ_ADD_01}: `pok_thread_wq_add()` shall call
 *   `list_add_tail()` once with the address of `wait_elem` of `t` and the
 *   address of `waits` of `wq`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_WQ_ADD_01}
 *
 * @design
 * 1. Call `list_add_tail()` with the arguments the address of `t->wait_elem`
 *    and the address of `wq->waits`. \llrref{LLR_THREAD_POK_THREAD_WQ_ADD_01}
 *
 * @calls
 * - `list_add_tail()`
 */
void pok_thread_wq_add(pok_thread_wq_t* wq, pok_thread_t* t)
{
    list_add_tail(&t->wait_elem, &wq->waits);
}

/**
 * @funcname `pok_thread_wq_add_prio`
 *
 * @description
 * Adds a thread to a wait queue in priority order, after the threads of the
 * same or a higher priority.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `pok_thread_wq_t *` | [in,out] The wait queue. |
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `next` | `list_head_t *` | The queue element the thread goes before |
 * | `other_thread` | `const pok_thread_t *` | A thread of the queue |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_WQ_ADD_PRIO_01}: `pok_thread_wq_add_prio()`
 *   shall add a process to a wait queue in the order of priority, after the
 *   processes of the same or a higher priority.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_012}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_WQ_ADD_PRIO_01}: `pok_thread_wq_add_prio()`
 *   shall set `wait_priority` of `t` to its `priority` and call
 *   `list_add_tail()` once with the address of `wait_elem` of `t` and the
 *   element of `waits` of `wq` before which it goes: that of the first thread
 *   whose `wait_priority` is less than that of `t`, or the head of `waits` if
 *   there is none.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_WQ_ADD_PRIO_01}
 *
 * @design
 * 1. Set `next` to `wq->waits.next`.
 *    \llrref{LLR_THREAD_POK_THREAD_WQ_ADD_PRIO_01}
 * 2. Set `t->wait_priority` to `t->priority`.
 *    \llrref{LLR_THREAD_POK_THREAD_WQ_ADD_PRIO_01}
 * 3. While `next` is not equal to the address of `wq->waits`:
 *    \llrref{LLR_THREAD_POK_THREAD_WQ_ADD_PRIO_01}
 *    1. Set `other_thread` to `list_entry(next, pok_thread_t, wait_elem)`.
 *    2. If `other_thread->wait_priority` is less than `t->wait_priority`:
 *       1. Leave the loop.
 *    3. Set `next` to `next->next`.
 * 4. Call `list_add_tail()` with the arguments the address of `t->wait_elem`
 *    and `next`. \llrref{LLR_THREAD_POK_THREAD_WQ_ADD_PRIO_01}
 *
 * @calls
 * - `list_add_tail()`
 */
void pok_thread_wq_add_prio(pok_thread_wq_t* wq, pok_thread_t* t)
{
    /* t is inserted before this element (the list head: at the end). */
    list_head_t* next = wq->waits.next;
    t->wait_priority = t->priority;
    while(next != &wq->waits)
    {
        const pok_thread_t* other_thread =
            list_entry(next, pok_thread_t, wait_elem);

        if(other_thread->wait_priority < t->wait_priority)
        {
            break;
        }
        next = next->next;
    }

    list_add_tail(&t->wait_elem, next);
}

/**
 * @funcname `pok_thread_wq_remove`
 *
 * @description
 * Removes a thread from the wait queue it is in, if any.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_WQ_REMOVE_01}: `pok_thread_wq_remove()` shall
 *   remove a process from the wait queue that it is in, if any.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_012}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_WQ_REMOVE_01}: `pok_thread_wq_remove()` shall
 *   call `list_del_init()` once with the address of `wait_elem` of `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_WQ_REMOVE_01}
 *
 * @design
 * 1. Call `list_del_init()` with the address of `t->wait_elem`.
 *    \llrref{LLR_THREAD_POK_THREAD_WQ_REMOVE_01}
 *
 * @calls
 * - `list_del_init()`
 */
void pok_thread_wq_remove(pok_thread_t* t)
{
    list_del_init(&t->wait_elem);
}

/**
 * @funcname `pok_thread_wq_wake_up`
 *
 * @description
 * Wakes up the first thread of a wait queue.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `pok_thread_wq_t *` | [in] The wait queue. |
 *
 * @return The thread woken up, or NULL if the queue is empty.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | The first thread |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_WQ_WAKE_UP_01}: `pok_thread_wq_wake_up()`
 *   shall wake the first process of a wait queue and give it, or none if the
 *   queue is empty.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_012}
 *   - \srqref{SRQ_JET_IPC_013}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_WQ_WAKE_UP_01}: If `list_empty()` returns
 *   false for `waits` of `wq`, `pok_thread_wq_wake_up()` shall call
 *   `list_del_init()` for `wait_elem` of the first thread of the list and
 *   `thread_wake_up()` with it, and return it.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_WQ_WAKE_UP_01}
 * - \llrid{LLR_THREAD_POK_THREAD_WQ_WAKE_UP_02}: If `list_empty()` returns true
 *   for `waits` of `wq`, `pok_thread_wq_wake_up()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_WQ_WAKE_UP_01}
 *
 * @design
 * 1. If `list_empty(&wq->waits)` is false:
 *    \llrref{LLR_THREAD_POK_THREAD_WQ_WAKE_UP_01},
 *    \llrref{LLR_THREAD_POK_THREAD_WQ_WAKE_UP_02}
 *    1. Set `t` to `list_first_entry(&wq->waits, pok_thread_t, wait_elem)`.
 *       \llrref{LLR_THREAD_POK_THREAD_WQ_WAKE_UP_01}
 *    2. Call `list_del_init()` with the address of `t->wait_elem`.
 *       \llrref{LLR_THREAD_POK_THREAD_WQ_WAKE_UP_01}
 *    3. Call `thread_wake_up()` with the argument `t`.
 *       \llrref{LLR_THREAD_POK_THREAD_WQ_WAKE_UP_01}
 *    4. Return `t`. \llrref{LLR_THREAD_POK_THREAD_WQ_WAKE_UP_01}
 * 2. Return `NULL`. \llrref{LLR_THREAD_POK_THREAD_WQ_WAKE_UP_02}
 *
 * @calls
 * - `list_empty()`
 * - `list_del_init()`
 * - `thread_wake_up()`
 */
pok_thread_t* pok_thread_wq_wake_up(pok_thread_wq_t* wq)
{
    if(!list_empty(&wq->waits))
    {
        pok_thread_t* t = list_first_entry(&wq->waits, pok_thread_t, wait_elem);
        /*
         * First, remove thread from the waiters list.
         *
         * So futher thread_wake_up() will not interpret it as timeouted.
         */
        list_del_init(&t->wait_elem);
        thread_wake_up(t);

        return t;
    }

    return NULL;
}


/**
 * @funcname `pok_thread_wq_get_nwaits`
 *
 * @description
 * Returns the number of threads in a wait queue.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `pok_thread_wq_t *` | [in] The wait queue. |
 *
 * @return The number of threads.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `int32_t` | The number counted |
 * | `elem` | `list_head_t *` | The queue element looked at |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_WQ_GET_NWAITS_01}:
 *   `pok_thread_wq_get_nwaits()` shall give the number of processes in a wait
 *   queue.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_003}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_WQ_GET_NWAITS_01}:
 *   `pok_thread_wq_get_nwaits()` shall count the elements of the list and
 *   return the count.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_WQ_GET_NWAITS_01}
 *
 * @design
 * 1. Set `count` to `0`. \llrref{LLR_THREAD_POK_THREAD_WQ_GET_NWAITS_01}
 * 2. Repeat forever (after each pass: use the macro `list_for_each()` with the
 *    arguments `elem` and `&wq->waits`):
 *    \llrref{LLR_THREAD_POK_THREAD_WQ_GET_NWAITS_01}
 *    1. Increment `count`.
 * 3. Return `count`. \llrref{LLR_THREAD_POK_THREAD_WQ_GET_NWAITS_01}
 *
 * @calls
 * None.
 */
int32_t pok_thread_wq_get_nwaits(pok_thread_wq_t* wq)
{
    int32_t count = 0;
    list_head_t* elem;
    list_for_each(elem, &wq->waits)
    {
        count++;
    }

    return count;
}


/**
 * @funcname `pok_thread_wq_is_empty`
 *
 * @description
 * Tells whether a wait queue is empty.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `pok_thread_wq_t *` | [in] The wait queue. |
 *
 * @return TRUE if no thread waits.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_THREAD_POK_THREAD_WQ_IS_EMPTY_01}: `pok_thread_wq_is_empty()`
 *   shall tell whether a wait queue is empty.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_010}
 *   - \srqref{SRQ_JET_IPC_020}
 *   - \srqref{SRQ_JET_IPC_011}
 *   - \srqref{SRQ_JET_IPC_021}
 *
 * @llr
 * - \llrid{LLR_THREAD_POK_THREAD_WQ_IS_EMPTY_01}: `pok_thread_wq_is_empty()`
 *   shall return the result of `list_empty()` for the list.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_POK_THREAD_WQ_IS_EMPTY_01}
 *
 * @design
 * 1. Call `list_empty()` with the address of `wq->waits`, and return its
 *    result. \llrref{LLR_THREAD_POK_THREAD_WQ_IS_EMPTY_01}
 *
 * @calls
 * - `list_empty()`
 */
pok_bool_t pok_thread_wq_is_empty(pok_thread_wq_t* wq)
{
    return list_empty(&wq->waits);
}
