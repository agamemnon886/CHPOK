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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `t_end` to `part->threads + part->nthreads_used`.
 * 3. Set `t` to the address of
 *    `part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID + 1U]`; then while `t`
 *    is not equal to `t_end`, repeat (after each pass: increment `t`):
 *    1. If `part->thread_error` is equal to `t`:
 *       1. Go on with the next pass of the loop.
 *    2. If `pok_compare_names(t->name, name)` is equal to `0`:
 *       1. Return `t`.
 * 4. Return `NULL`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. If `(id == 0U) || (id >= part->nthreads_used) #ifdef`
 *    `POK_NEEDS_ERROR_HANDLING || (part->thread_error == &part->threads[id])`
 *    is true:
 *    1. Return `NULL`.
 * 3. Return the address of `part->threads[id]`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. If `part->mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Return `POK_ERRNO_PARTITION_MODE`.
 * 3. Set `k_attr` to `jet_user_to_kernel_typed_ro(attr)`.
 * 4. If `k_attr` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `k_thread_id` to `jet_user_to_kernel_typed(thread_id)`.
 * 6. If `k_thread_id` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 7. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 * 8. If `k_name` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 9. If `part->nthreads_used` is equal to `part->nthreads`:
 *    1. Return `POK_ERRNO_TOOMANY`.
 * 10. Set `t` to the address of `part->threads[part->nthreads_used]`.
 * 11. Set `t->entry` to `entry`.
 * 12. Set `t->base_priority` to `k_attr->priority`.
 * 13. Set `t->period` to `k_attr->period`.
 * 14. Set `t->time_capacity` to `k_attr->time_capacity`.
 * 15. Set `t->deadline` to `k_attr->deadline`.
 * 16. If `t->base_priority` is greater than `MAX_PRIORITY_VALUE` or
 *     `t->base_priority` is less than `MIN_PRIORITY_VALUE`:
 *     1. Return `POK_ERRNO_PARAM`.
 * 17. If `t->period` is not equal to `POK_TIME_INFINITY` and `t->period` is
 *     less than or equal to `0`:
 *     1. Return `POK_ERRNO_EINVAL`.
 * 18. If `t->time_capacity` is not equal to `POK_TIME_INFINITY` and
 *     `t->time_capacity` is less than or equal to `0`:
 *     1. Return `POK_ERRNO_PARAM`.
 * 19. If `pok_time_is_infinity(t->period)` is false:
 *     1. If `pok_time_is_infinity(t->time_capacity)` is true:
 *        1. Return `POK_ERRNO_PARAM`.
 *     2. If `t->time_capacity` is greater than `t->period`:
 *        1. Return `POK_ERRNO_PARAM`.
 * 20. If `jet_check_access_exec(t->entry)` is false:
 *     1. Return `POK_ERRNO_PARAM`.
 * 21. Call `memcpy()` with the arguments `t->name`, `k_name` and
 *     `MAX_NAME_LENGTH`.
 * 22. If `find_thread(t->name)` is not NULL:
 *     1. Return `POK_ERRNO_EXISTS`.
 * 23. Set `t->user_stack_size` to `k_attr->stack_size`.
 * 24. If `thread_create(t)` is false:
 *     1. Return `POK_ERRNO_UNAVAILABLE`.
 * 25. Set `*k_thread_id` to `part->nthreads_used`.
 * 26. Increment `part->nthreads_used`.
 * 27. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. If `thread_is_waiting_allowed()` is false:
 *    1. Return `POK_ERRNO_MODE`.
 * 2. Set `k_time` to `jet_user_to_kernel_typed_ro(duration)`.
 * 3. If `k_time` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `kernel_time` to the value at `k_time`.
 * 5. Call `pok_preemption_local_disable()`.
 * 6. If `kernel_time` is equal to `0`:
 *    1. Call `thread_yield()` with the argument `current_thread`.
 * 7. Otherwise:
 *    1. Call `thread_wait_common()` with the arguments `current_thread` and
 *       `kernel_time`.
 * 8. Call `pok_preemption_local_enable()`.
 * 9. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. If `thread_is_waiting_allowed()` is false:
 *    1. Return `POK_ERRNO_MODE`.
 * 2. Set `k_time` to `jet_user_to_kernel_typed_ro(until)`.
 * 3. If `k_time` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `kernel_time` to the value at `k_time`.
 * 5. Call `pok_preemption_local_disable()`.
 * 6. Call `thread_wait_timed()` with the arguments `current_thread` and
 *    `kernel_time`.
 * 7. Call `pok_preemption_local_enable()`.
 * 8. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `pok_preemption_local_disable()`.
 * 2. Call `thread_yield()` with the argument `current_thread`.
 * 3. Call `pok_preemption_local_enable()`.
 * 4. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `tshd` to `part->kshd->tshd + (thread - part->threads)`.
 * 3. Check that `!pok_time_is_infinity(delay)` holds (assert: stop otherwise).
 * 4. If `thread->state` is not equal to `POK_STATE_STOPPED`:
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 5. If `pok_time_is_infinity(thread->period)` is false and `delay` is greater
 *    than or equal to `thread->period`:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 6. Set `thread->priority` to `thread->base_priority`.
 * 7. Set `thread->sp` to `NULL`.
 * 8. Set `tshd->msection_count` to `0`.
 * 9. Set `tshd->msection_entering` to `NULL`.
 * 10. Set `tshd->priority` to `thread->priority`.
 * 11. Set `tshd->thread_kernel_flags` to `0`.
 * 12. If `part->mode` is not equal to `POK_PARTITION_MODE_NORMAL`:
 *     1. Set `thread->delayed_time` to `delay`.
 *     2. Set `thread->state` to `POK_STATE_WAITING`.
 *     3. Return `POK_ERRNO_OK`.
 * 13. If `pok_time_is_infinity(thread->period)` is true:
 *     1. Call `pok_time_add()` with the arguments `jet_system_time()` and
 *        `delay`, and store the result in `thread_start_time`.
 * 14. Otherwise:
 *     1. Call `pok_time_add()` with the arguments
 *        `get_next_periodic_processing_start()` and `delay`, and store the
 *        result in `thread_start_time`.
 *     2. Call `pok_time_add()` with the arguments `thread_start_time` and
 *        `thread->period`, and store the result in `thread->next_activation`.
 * 15. If `pok_time_is_infinity(thread->time_capacity)` is false:
 *     1. Call `thread_set_deadline()` with the arguments `thread` and
 *        `pok_time_add(thread_start_time, thread->time_capacity)`.
 * 16. If `delay` is equal to `0` and `pok_time_is_infinity(thread->period)` is
 *     true:
 *     1. Call `thread_start()` with the argument `thread`.
 * 17. Otherwise:
 *     1. Call `thread_wait_timed()` with the arguments `thread` and
 *        `thread_start_time`.
 * 18. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `pok_time_is_infinity()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `thread`.
 * 2. If `thread` is NULL:
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. Set `k_delay_time` to `jet_user_to_kernel_typed_ro(delay_time)`.
 * 4. If `k_delay_time` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `kernel_delay_time` to the value at `k_delay_time`.
 * 6. If `pok_time_is_infinity(kernel_delay_time)` is true:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 7. Call `pok_preemption_local_disable()`.
 * 8. Call `thread_delayed_start_internal()` with the arguments `thread` and
 *    `kernel_delay_time`, and store the result in `ret`.
 * 9. Call `pok_preemption_local_enable()`.
 * 10. Return `ret`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `thread`.
 * 2. If `thread` is NULL:
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. Call `pok_preemption_local_disable()`.
 * 4. Call `thread_delayed_start_internal()` with the arguments `thread` and
 *    `0`, and store the result in `ret`.
 * 5. Call `pok_preemption_local_enable()`.
 * 6. Return `ret`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`.
 * 2. If `t` is NULL:
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 * 4. If `k_status` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `k_entry` to `jet_user_to_kernel_typed(entry)`.
 * 6. If `k_entry` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 7. Call `jet_user_to_kernel()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 * 8. If `k_name` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 9. Call `memcpy()` with the arguments `k_name`, `t->name` and
 *    `MAX_NAME_LENGTH`.
 * 10. Set `*k_entry` to `t->entry`.
 * 11. Set `k_status->attributes.priority` to `t->base_priority`.
 * 12. Set `k_status->attributes.period` to `t->period`.
 * 13. Set `k_status->attributes.deadline` to `t->deadline`.
 * 14. Set `k_status->attributes.time_capacity` to `t->time_capacity`.
 * 15. Set `k_status->attributes.stack_size` to `t->user_stack_size`.
 * 16. Call `pok_preemption_local_disable()`.
 * 17. Set `k_status->current_priority` to `t->priority`.
 * 18. If `t->state` is equal to `POK_STATE_RUNNABLE`:
 *     1. If `t->suspended` is true:
 *        1. Set `k_status->state` to `POK_STATE_WAITING`.
 *     2. Otherwise, if `current_partition_arinc->thread_current` is equal to
 *        `t`:
 *        1. Set `k_status->state` to `POK_STATE_RUNNING`.
 *     3. Otherwise:
 *        1. Set `k_status->state` to `t->state`.
 * 19. Otherwise:
 *     1. Set `k_status->state` to `t->state`.
 * 20. If `pok_time_is_infinity(t->time_capacity)` is true:
 *     1. Set `k_status->deadline_time` to `POK_TIME_INFINITY`.
 * 21. Otherwise:
 *     1. Set `k_status->deadline_time` to `t->thread_deadline_event.timepoint`.
 * 22. Call `pok_preemption_local_enable()`.
 * 23. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`.
 * 3. If `t` is NULL:
 *    1. Return `POK_ERRNO_PARAM`.
 * 4. If `priority` is greater than `MAX_PRIORITY_VALUE`:
 *    1. Return `POK_ERRNO_PARAM`.
 * 5. If `priority` is less than `MIN_PRIORITY_VALUE`:
 *    1. Return `POK_ERRNO_PARAM`.
 * 6. Call `pok_preemption_local_disable()`.
 * 7. Set `ret` to `POK_ERRNO_UNAVAILABLE`.
 * 8. If `t->state` is equal to `POK_STATE_STOPPED`:
 *    1. Go to the label `out`.
 * 9. Set `t->priority` to `priority`.
 * 10. Set `tshd` to `part->kshd->tshd + (t - part->threads)`.
 * 11. Set `tshd->priority` to `priority`.
 * 12. Call `thread_yield()` with the argument `t`.
 * 13. Set `ret` to `POK_ERRNO_OK`.
 * 14. Label `out`:
 * 15. Call `pok_preemption_local_enable()`.
 * 16. Return `ret`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`.
 * 2. If `t` is NULL:
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. If `t` is equal to `current_thread`:
 *    1. Return `POK_ERRNO_THREADATTR`.
 * 4. If `pok_thread_is_periodic(t)` is true:
 *    1. Return `POK_ERRNO_MODE`.
 * 5. Call `pok_preemption_local_disable()`.
 * 6. Set `ret` to `POK_ERRNO_MODE`.
 * 7. If `t->state` is equal to `POK_STATE_STOPPED`:
 *    1. Go to the label `out`.
 * 8. Set `ret` to `POK_ERRNO_UNAVAILABLE`.
 * 9. If `t->suspended` is false:
 *    1. Go to the label `out`.
 * 10. Call `thread_resume()` with the argument `t`.
 * 11. Set `ret` to `POK_ERRNO_OK`.
 * 12. Label `out`:
 * 13. Call `pok_preemption_local_enable()`.
 * 14. Return `ret`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`.
 * 2. If `t` is NULL:
 *    1. Return `POK_ERRNO_PARAM`.
 * 3. If `t` is equal to `current_thread`:
 *    1. Return `POK_ERRNO_THREADATTR`.
 * 4. If `pok_thread_is_periodic(t)` is true:
 *    1. Return `POK_ERRNO_MODE`.
 * 5. Call `pok_preemption_local_disable()`.
 * 6. Set `ret` to `POK_ERRNO_MODE`.
 * 7. If `t->state` is equal to `POK_STATE_STOPPED`:
 *    1. Go to the label `out`.
 * 8. If `current_partition_arinc->lock_level` is greater than `0U` and
 *    `current_partition_arinc->thread_locked` is equal to `t`:
 *    1. Go to the label `out`.
 * 9. Set `ret` to `POK_ERRNO_UNAVAILABLE`.
 * 10. If `t->suspended` is true:
 *     1. Go to the label `out`.
 * 11. Call `thread_suspend()` with the argument `t`.
 * 12. Set `ret` to `POK_ERRNO_OK`.
 * 13. Label `out`:
 * 14. Call `pok_preemption_local_enable()`.
 * 15. Return `ret`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `t` to `current_thread`.
 * 2. Set `k_time` to `jet_user_to_kernel_typed_ro(timeout)`.
 * 3. If `k_time` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `kernel_time` to the value at `k_time`.
 * 5. If `pok_thread_is_periodic(t)` is true:
 *    1. Return `POK_ERRNO_MODE`.
 * 6. If `thread_is_waiting_allowed()` is false:
 *    1. Return `POK_ERRNO_MODE`.
 * 7. Call `pok_preemption_local_disable()`.
 * 8. If `kernel_time` is equal to `0`:
 *    1. Go to the label `out`.
 * 9. Call `thread_suspend()` with the argument `t`.
 * 10. If `pok_time_is_infinity(kernel_time)` is false:
 *     1. Go to the label `suspend_timed`.
 * 11. Label `out`:
 * 12. Call `pok_preemption_local_enable()`.
 * 13. Return `POK_ERRNO_OK`.
 * 14. Label `suspend_timed`:
 * 15. Call `thread_suspend_timed()` with the arguments `t` and `kernel_time`.
 * 16. Call `pok_preemption_local_enable()`.
 * 17. Return `t->wait_result`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `ret` to `POK_ERRNO_PARAM`.
 * 3. Call `get_thread_by_id()` with the argument `id`, and store the result in
 *    `t`.
 * 4. If `t` is NULL:
 *    1. Return `POK_ERRNO_PARAM`.
 * 5. Set `thread_current` to `part->thread_current`.
 * 6. If `t` is equal to `thread_current`:
 *    1. Return `POK_ERRNO_THREADATTR`.
 * 7. Set `tshd_target` to `part->kshd->tshd + (t - part->threads)`.
 * 8. Call `pok_preemption_local_disable()`.
 * 9. Set `ret` to `POK_ERRNO_UNAVAILABLE`.
 * 10. If `t->state` is equal to `POK_STATE_STOPPED`:
 *     1. Go to the label `out`.
 * 11. If `t->relations_stop.donate_target` is not NULL:
 *     1. If `t->relations_stop.first_donator` is NULL:
 *        1. Set `t->relations_stop.first_donator` to `thread_current`.
 *        2. Set `ret` to `POK_ERRNO_OK`.
 *     2. Set `thread_current->relations_stop.donate_target` to
 *        `t->relations_stop.donate_target`.
 *     3. Set `thread_current->relations_stop.next_donator` to
 *        `t->relations_stop.next_donator`.
 *     4. Set `t->relations_stop.next_donator` to `thread_current`.
 * 12. Otherwise, if `t->relations_stop.first_donator` is not NULL:
 *     1. Set `thread_current->relations_stop.donate_target` to `t`.
 *     2. Set `thread_current->relations_stop.next_donator` to
 *        `t->relations_stop.first_donator`.
 *     3. Set `t->relations_stop.first_donator` to `thread_current`.
 * 13. Otherwise, if `tshd_target->msection_count` is not equal to `0U`:
 *     1. If `t->state` is equal to `POK_STATE_WAITING`:
 *        1. Call `thread_wake_up()` with the argument `t`.
 *        2. Set `t->wait_result` to `POK_ERRNO_CANCELLED`.
 *     2. Set `thread_current->relations_stop.donate_target` to `t`.
 *     3. Set `thread_current->relations_stop.next_donator` to `NULL`.
 *     4. Set `t->relations_stop.first_donator` to `thread_current`.
 *     5. Set `ret` to `POK_ERRNO_OK`.
 *     6. Set `tshd_target->thread_kernel_flags` to `THREAD_KERNEL_FLAG_KILLED`.
 * 14. Otherwise:
 *     1. Set `ret` to `POK_ERRNO_OK`.
 *     2. Call `thread_stop()` with the argument `t`.
 *     3. Go to the label `out`.
 * 15. Call `pok_sched_local_invalidate()`.
 * 16. Label `out`:
 * 17. Call `pok_preemption_local_enable()`.
 * 18. Return `ret`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `t` to `part->thread_current`.
 * 3. Call `pok_preemption_local_disable()`.
 * 4. Check that `t->relations_stop.donate_target == NULL` holds (assert: stop
 *    otherwise).
 * 5. Check that `t->relations_stop.first_donator == NULL` holds (assert_os:
 *    stop otherwise).
 * 6. Call `thread_stop()` with the argument `t`.
 * 7. If `t` is equal to `part->thread_error`:
 *    1. Call `error_check_after_handler()`.
 *    2. Call `error_ignore_sync()`.
 * 8. Call `pok_sched_local_invalidate()`.
 * 9. Call `pok_preemption_local_enable()`.
 * 10. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `pok_preemption_local_disable()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 * 2. If `k_name` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 * 4. If `k_id` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `memcpy()` with the arguments `kernel_name`, `k_name` and
 *    `MAX_NAME_LENGTH`.
 * 6. Call `find_thread()` with the argument `kernel_name`, and store the result
 *    in `t`.
 * 7. If `t` is NULL:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 8. Set `*k_id` to `t - current_partition_arinc->threads`.
 * 9. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `t` to `current_thread`.
 * 2. If `pok_thread_is_periodic(t)` is false:
 *    1. Return `POK_ERRNO_MODE`.
 * 3. If `thread_is_waiting_allowed()` is false:
 *    1. Return `POK_ERRNO_MODE`.
 * 4. Call `pok_preemption_local_disable()`.
 * 5. Call `pok_time_add()` with the arguments `t->next_activation` and
 *    `t->period`, and store the result in `t->next_activation`.
 * 6. Call `thread_wait_timed()` with the arguments `t` and
 *    `t->next_activation`.
 * 7. Call `thread_set_deadline()` with the arguments `t` and
 *    `pok_time_add(t->next_activation, t->time_capacity)`.
 * 8. Call `pok_preemption_local_enable()`.
 * 9. Return `POK_ERRNO_OK`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `k_budget` to `jet_user_to_kernel_typed_ro(budget)`.
 * 3. If `k_budget` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `kernel_budget` to the value at `k_budget`.
 * 5. Set `t` to `current_thread`.
 * 6. If `t` is equal to `part->thread_error`:
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 7. If `part->mode` is not equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 8. If `pok_time_is_infinity(t->time_capacity)` is true:
 *    1. Return `POK_ERRNO_OK`.
 * 9. Call `pok_preemption_local_disable()`.
 * 10. If `pok_time_is_infinity(kernel_budget)` is true:
 *     1. If `pok_time_is_infinity(t->period)` is false:
 *        1. Set `ret` to `POK_ERRNO_MODE`.
 *        2. Go to the label `out`.
 *     2. Call `delayed_event_remove()` with the arguments the address of
 *        `part->partition_delayed_events` and the address of
 *        `t->thread_deadline_event`.
 *     3. Set `ret` to `POK_ERRNO_OK`.
 * 11. Otherwise:
 *     1. Call `pok_time_add()` with the arguments `jet_system_time()` and
 *        `kernel_budget`, and store the result in `calculated_deadline`.
 *     2. If `pok_time_is_infinity(t->period)` is false and
 *        `calculated_deadline` is greater than or equal to
 *        `t->next_activation`:
 *        1. Set `ret` to `POK_ERRNO_MODE`.
 *        2. Go to the label `out`.
 *     3. Call `thread_set_deadline()` with the arguments `t` and
 *        `calculated_deadline`.
 *     4. Set `ret` to `POK_ERRNO_OK`.
 * 12. Label `out`:
 * 13. Call `pok_preemption_local_enable()`.
 * 14. Return `ret`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Check that `id < part->nthreads_used` holds (assert_os: stop otherwise).
 *
 * @calls
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `thread_current` to `part->thread_current`.
 * 3. Set `msection_entering` to `jet_user_to_kernel_typed(section)`.
 * 4. Check that `msection_entering != NULL` holds (assert_os: stop otherwise).
 * 5. Call `pok_preemption_local_disable()`.
 * 6. If `msection_entering->owner` is not equal to `JET_THREAD_ID_NONE` and
 *    `msection_entering->owner` is not equal to
 *    `(thread_current - part->threads)`:
 *    1. Set `thread_current->msection_entering` to `msection_entering`.
 *    2. Call `pok_sched_local_invalidate()`.
 * 7. Call `pok_preemption_local_enable()`.
 * 8. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `thread_current` to `part->thread_current`.
 * 3. Set `kernel_timeout` to `jet_user_to_kernel_typed_ro(timeout)`.
 * 4. If `kernel_timeout` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `msection_entering` to `jet_user_to_kernel_typed(section)`.
 * 6. Check that `msection_entering != NULL` holds (assert_os: stop otherwise).
 * 7. Check that `msection_entering->owner == (thread_current - part->threads)`
 *    holds (assert_os: stop otherwise).
 * 8. Set `tshd_current` to
 *    `part->kshd->tshd + (thread_current - part->threads)`.
 * 9. Check that `tshd_current->msection_count == 1U` holds (assert_os: stop
 *    otherwise).
 * 10. If `(part->lock_level != 0U) #ifdef POK_NEEDS_ERROR_HANDLING ||`
 *     `(part->thread_error == thread_current)` is true:
 *     1. Return `POK_ERRNO_MODE`.
 * 11. Call `pok_preemption_local_disable()`.
 * 12. If `thread_current->relations_stop.first_donator` is not NULL:
 *     1. Set `thread_current->wait_result` to `POK_ERRNO_CANCELLED`.
 *     2. Go to the label `out`.
 * 13. Set `thread_current->msection_entering` to `msection_entering`.
 * 14. Set `msection_entering->owner` to `JET_THREAD_ID_NONE`.
 * 15. Call `thread_wait_common()` with the arguments `thread_current` and the
 *     value at `kernel_timeout`.
 * 16. Call `pok_sched_local_invalidate()`.
 * 17. Label `out`:
 * 18. Call `pok_preemption_local_enable()`.
 * 19. Return `thread_current->wait_result`.
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_user_to_kernel()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `ret` to `POK_ERRNO_EXISTS`.
 * 2. Set `section_kernel` to `jet_user_to_kernel_typed(section)`.
 * 3. Check that `section_kernel != NULL` holds (assert_os: stop otherwise).
 * 4. Set `part` to `current_partition_arinc`.
 * 5. Set `thread_current` to `part->thread_current`.
 * 6. Call `assert_thread_id()` with the argument `thread_id`.
 * 7. Set `t` to the address of `part->threads[thread_id]`.
 * 8. Call `pok_preemption_local_disable()`.
 * 9. If `t->state` is not equal to `POK_STATE_WAITING`:
 *    1. Go to the label `out`.
 * 10. Set `msection_entering` to `t->msection_entering`.
 * 11. Check that `section_kernel == msection_entering` holds (assert_os: stop
 *     otherwise).
 * 12. Check that `section_kernel->owner == (thread_current - part->threads)`
 *     holds (assert_os: stop otherwise).
 * 13. Call `thread_wake_up()` with the argument `t`.
 * 14. Set `t->wait_result` to `POK_ERRNO_OK`.
 * 15. Set `ret` to `POK_ERRNO_OK`.
 * 16. Label `out`:
 * 17. Call `pok_preemption_local_enable()`.
 * 18. Return `ret`.
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `next_id` to `tshd_t->wq_next`.
 * 3. Set `prev_id` to `tshd_t->wq_prev`.
 * 4. If `next_id` is equal to `JET_THREAD_ID_NONE`:
 *    1. Set `pnext` to the address of `wq->last`.
 * 5. Otherwise:
 *    1. Call `assert_thread_id()` with the argument `next_id`.
 *    2. Set `pnext` to the address of `part->kshd->tshd[next_id].wq_prev`.
 * 6. If `prev_id` is equal to `JET_THREAD_ID_NONE`:
 *    1. Set `pprev` to the address of `wq->first`.
 * 7. Otherwise:
 *    1. Call `assert_thread_id()` with the argument `prev_id`.
 *    2. Set `pprev` to the address of `part->kshd->tshd[prev_id].wq_next`.
 * 8. Set `*pnext` to `prev_id`.
 * 9. Set `*pprev` to `next_id`.
 * 10. Set `tshd_t->wq_prev` to `JET_THREAD_ID_NONE`.
 * 11. Set `tshd_t->wq_next` to `JET_THREAD_ID_NONE`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `ret` to `POK_ERRNO_EMPTY`.
 * 2. Set `section_kernel` to `jet_user_to_kernel_typed(section)`.
 * 3. Check that `section_kernel != NULL` holds (assert_os: stop otherwise).
 * 4. Set `wq_kernel` to `jet_user_to_kernel_typed(wq)`.
 * 5. Check that `wq_kernel != NULL` holds (assert_os: stop otherwise).
 * 6. Set `part` to `current_partition_arinc`.
 * 7. Set `thread_current` to `part->thread_current`.
 * 8. Check that `section_kernel->owner == (thread_current - part->threads)`
 *    holds (assert_os: stop otherwise).
 * 9. Call `pok_preemption_local_disable()`.
 * 10. Set `thread_id` to `wq_kernel->first`.
 * 11. While `thread_id` is not equal to `JET_THREAD_ID_NONE`:
 *     1. Call `assert_thread_id()` with the argument `thread_id`.
 *     2. Set `t` to the address of `part->threads[thread_id]`.
 *     3. Check that `t->msection_entering == section_kernel` holds (assert_os:
 *        stop otherwise).
 *     4. Set `tshd_t` to the address of `part->kshd->tshd[thread_id]`.
 *     5. Set `thread_id` to `tshd_t->wq_next`.
 *     6. If `t->state` is not equal to `POK_STATE_WAITING`:
 *        1. Call `msection_wq_del()` with the arguments `wq_kernel` and
 *           `tshd_t`.
 *     7. Otherwise:
 *        1. Call `thread_wake_up()` with the argument `t`.
 *        2. Set `t->wait_result` to `POK_ERRNO_OK`.
 *        3. Set `ret` to `POK_ERRNO_OK`.
 *        4. If `is_all` is false:
 *           1. Leave the loop.
 * 12. Call `pok_preemption_local_enable()`.
 * 13. Return `ret`.
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Set `count` to `0`.
 * 2. Set `section_kernel` to `jet_user_to_kernel_typed(section)`.
 * 3. Check that `section_kernel != NULL` holds (assert_os: stop otherwise).
 * 4. Set `wq_kernel` to `jet_user_to_kernel_typed(wq)`.
 * 5. Check that `wq_kernel != NULL` holds (assert_os: stop otherwise).
 * 6. Set `size_kernel` to `jet_user_to_kernel_typed(size)`.
 * 7. Check that `size_kernel != NULL` holds (assert_os: stop otherwise).
 * 8. Set `part` to `current_partition_arinc`.
 * 9. Set `thread_current` to `part->thread_current`.
 * 10. Check that `section_kernel->owner == (thread_current - part->threads)`
 *     holds (assert_os: stop otherwise).
 * 11. Call `pok_preemption_local_disable()`.
 * 12. Set `thread_id` to `wq_kernel->first`.
 * 13. While `thread_id` is not equal to `JET_THREAD_ID_NONE`:
 *     1. Call `assert_thread_id()` with the argument `thread_id`.
 *     2. Set `t` to the address of `part->threads[thread_id]`.
 *     3. Check that `t->msection_entering == section_kernel` holds (assert_os:
 *        stop otherwise).
 *     4. Set `tshd_t` to the address of `part->kshd->tshd[thread_id]`.
 *     5. Set `thread_id` to `tshd_t->wq_next`.
 *     6. If `t->state` is not equal to `POK_STATE_WAITING`:
 *        1. Call `msection_wq_del()` with the arguments `wq_kernel` and
 *           `tshd_t`.
 *     7. Otherwise:
 *        1. Increment `count`.
 * 14. Call `pok_preemption_local_enable()`.
 * 15. Set `*size_kernel` to `count`.
 * 16. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `pok_assertion_fail()`
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
 *
 * @llr
 *
 * @design
 * 1. Call `INIT_LIST_HEAD()` with the address of `wq->waits`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `list_add_tail()` with the arguments the address of `t->wait_elem`
 *    and the address of `wq->waits`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `next` to `wq->waits.next`.
 * 2. Set `t->wait_priority` to `t->priority`.
 * 3. While `next` is not equal to the address of `wq->waits`:
 *    1. Set `other_thread` to `list_entry(next, pok_thread_t, wait_elem)`.
 *    2. If `other_thread->wait_priority` is less than `t->wait_priority`:
 *       1. Leave the loop.
 *    3. Set `next` to `next->next`.
 * 4. Call `list_add_tail()` with the arguments the address of `t->wait_elem`
 *    and `next`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `list_del_init()` with the address of `t->wait_elem`.
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
 *
 * @llr
 *
 * @design
 * 1. If `list_empty(&wq->waits)` is false:
 *    1. Set `t` to `list_first_entry(&wq->waits, pok_thread_t, wait_elem)`.
 *    2. Call `list_del_init()` with the address of `t->wait_elem`.
 *    3. Call `thread_wake_up()` with the argument `t`.
 *    4. Return `t`.
 * 2. Return `NULL`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `count` to `0`.
 * 2. Repeat forever (after each pass: use the macro `list_for_each()` with the
 *    arguments `elem` and `&wq->waits`):
 *    1. Increment `count`.
 * 3. Return `count`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `list_empty()` with the address of `wq->waits`, and return its
 *    result.
 *
 * @calls
 * - `list_empty()`
 */
pok_bool_t pok_thread_wq_is_empty(pok_thread_wq_t* wq)
{
    return list_empty(&wq->waits);
}
