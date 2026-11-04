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


/*
 * Find thread by name.
 *
 * Note: Doesn't require disable local preemption.
 *
 * Note: Name should be located in kernel space.
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

/*
 * Return thread by id.
 *
 * Return NULL if no such thread created.
 *
 * Note: Doesn't require disable local preemption.
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


pok_ret_t pok_thread_create (const char* JET_USER name,
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

/*
 * Turn current thread into the sleep for a while.
 *
 * NOTE: It is possible to sleep forever(ARINC prohibits that).
 */
pok_ret_t pok_thread_sleep(const pok_time_t* JET_USER duration)
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
pok_ret_t pok_thread_sleep_until(const pok_time_t* JET_USER until)
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

pok_ret_t pok_thread_yield (void)
{
	pok_preemption_local_disable();
	thread_yield(current_thread);
	pok_preemption_local_enable();

    return POK_ERRNO_OK;
}


// Called with local preemption disabled
static pok_ret_t thread_delayed_start_internal (pok_thread_t* thread,
												pok_time_t delay)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_time_t thread_start_time;

    struct jet_thread_shared_data* tshd = part->kshd->tshd
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

pok_ret_t pok_thread_delayed_start (pok_thread_id_t id,
    const pok_time_t* JET_USER delay_time)
{
    pok_ret_t ret;

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

pok_ret_t pok_thread_start (pok_thread_id_t id)
{
	pok_ret_t ret;

    pok_thread_t *thread = get_thread_by_id(id);
    if(thread == NULL) {
        return POK_ERRNO_PARAM;
    }

	pok_preemption_local_disable();
	ret = thread_delayed_start_internal(thread, 0);
	pok_preemption_local_enable();

	return ret;
}


pok_ret_t pok_thread_get_status (pok_thread_id_t id,
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

pok_ret_t pok_thread_set_priority(pok_thread_id_t id, uint32_t priority)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_ret_t ret;

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

    struct jet_thread_shared_data* tshd = part->kshd->tshd
        + (t - part->threads);
    tshd->priority = priority;

    thread_yield(t);

	ret = POK_ERRNO_OK;
out:
	pok_preemption_local_enable();

    return ret;
}

pok_ret_t pok_thread_resume(pok_thread_id_t id)
{
    pok_ret_t ret;

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

pok_ret_t pok_thread_suspend_target(pok_thread_id_t id)
{
    pok_ret_t ret;

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

pok_ret_t pok_thread_suspend(const pok_time_t* JET_USER timeout)
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

pok_ret_t pok_thread_stop_target(pok_thread_id_t id)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_ret_t ret = POK_ERRNO_PARAM;

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

    struct jet_thread_shared_data* tshd_target = part->kshd->tshd
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

pok_ret_t pok_thread_stop(void)
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

pok_ret_t pok_thread_find(const char* JET_USER name,
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

// called by periodic process when it's done its work
// ARINC-653 PERIODIC_WAIT
pok_ret_t pok_sched_end_period(void)
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

pok_ret_t pok_sched_replenish(const pok_time_t* JET_USER budget)
{
    pok_ret_t ret;

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


/* Assert that thread id is correct.*/
static void assert_thread_id(pok_thread_id_t id)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    assert_os(id < part->nthreads_used);
}

pok_ret_t jet_msection_enter_helper(struct msection* JET_USER section)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread_current = part->thread_current;

    struct msection* JET_KUSER msection_entering =
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

pok_ret_t jet_msection_wait(struct msection* JET_USER section,
    const pok_time_t* JET_USER timeout)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread_current = part->thread_current;

    const pok_time_t* JET_KUSER kernel_timeout =
        jet_user_to_kernel_typed_ro(timeout);

    if(kernel_timeout == NULL) {
        return POK_ERRNO_EFAULT;
    }

    struct msection* JET_KUSER msection_entering =
        jet_user_to_kernel_typed(section);

    // Passing wrong section is OS error, not a user.
    assert_os(msection_entering != NULL);
    // The thread should be a section's owner.
    assert_os(msection_entering->owner == (thread_current - part->threads));

    struct jet_thread_shared_data* tshd_current = part->kshd->tshd
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

pok_ret_t jet_msection_notify(struct msection* JET_USER section,
    pok_thread_id_t thread_id)
{
    pok_ret_t ret = POK_ERRNO_EXISTS;

    struct msection* JET_KUSER section_kernel =
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

    struct msection* JET_KUSER msection_entering = t->msection_entering;

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

/******************* msection wait queue ******************************/
/* Remove thread from the queue. */
static void msection_wq_del(struct msection_wq* wq,
    struct jet_thread_shared_data* tshd_t)
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

/*
 * Awoke waiting threads in the waitqueue.
 *
 * Every thread in the queue which hasn't waited at the function's call
 * is removed from the queue.
 *
 * If 'first_only' is TRUE, the first waiting thread only. This thread
 * will be pointed by wq->first after the call.
 * If 'first_only' is FALSE, notify all waiting threads. List of the
 * awoken threads may be iterated directly from user space.
 *
 * May be called only by the owner of the section.
 *
 * Returns:
 *
 *     POK_ERRNO_OK - at least on thread has been notified.
 *     POK_ERRNO_EMPTY - there is no waiting threads in the waitqueue.
 */
pok_ret_t jet_msection_wq_notify(struct msection* JET_USER section,
   struct msection_wq* JET_USER wq,
   pok_bool_t is_all)
{
    pok_ret_t ret = POK_ERRNO_EMPTY;

    struct msection* JET_KUSER section_kernel =
        jet_user_to_kernel_typed(section);
    assert_os(section_kernel != NULL);

    struct msection_wq* JET_KUSER wq_kernel = jet_user_to_kernel_typed(wq);
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

        struct jet_thread_shared_data* tshd_t = &part->kshd->tshd[thread_id];

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

/*
 * Compute number of waiting threads in the waitqueue.
 *
 * Every thread in the queue which hasn't waited at the function's call
 * is removed from the queue.
 *
 * Returns: POK_ERRNO_OK.
 */
pok_ret_t jet_msection_wq_size(struct msection* JET_USER section,
   struct msection_wq* JET_USER wq,
   size_t* JET_USER size)
{
    size_t count = 0;

    struct msection* JET_KUSER section_kernel =
        jet_user_to_kernel_typed(section);
    assert_os(section_kernel != NULL);

    struct msection_wq* JET_KUSER wq_kernel = jet_user_to_kernel_typed(wq);
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

        struct jet_thread_shared_data* tshd_t = &part->kshd->tshd[thread_id];

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


/********************* wait queue for port*****************************/
void pok_thread_wq_init(pok_thread_wq_t* wq)
{
    INIT_LIST_HEAD(&wq->waits);
}

void pok_thread_wq_add(pok_thread_wq_t* wq, pok_thread_t* t)
{
    list_add_tail(&t->wait_elem, &wq->waits);
}

void pok_thread_wq_add_prio(pok_thread_wq_t* wq, pok_thread_t* t)
{
    /* t is inserted before this element (the list head: at the end). */
    struct list_head* next = wq->waits.next;
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

void pok_thread_wq_remove(pok_thread_t* t)
{
    list_del_init(&t->wait_elem);
}

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


int32_t pok_thread_wq_get_nwaits(pok_thread_wq_t* wq)
{
    int32_t count = 0;
    struct list_head* elem;
    list_for_each(elem, &wq->waits)
    {
        count++;
    }

    return count;
}


pok_bool_t pok_thread_wq_is_empty(pok_thread_wq_t* wq)
{
    return list_empty(&wq->waits);
}
