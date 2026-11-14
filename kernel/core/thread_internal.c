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
 * Modified by Kirill Brazhnikov, 2026-10-31: printf format fix for
 * 64-bit targets.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; if-else-if
 * chains end with else; deviations are documented where a guideline can't
 * be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of mixed
 * precedence are parenthesized; deviations are documented where a guideline
 * can't be followed; thread_set_eligible() finds the insertion point with a
 * while loop that doesn't change the list, without a goto; the deadline miss
 * message prints handler_id. Long lines are wrapped at 80 columns.
 * thread_suspend_timed() adds the time with pok_time_add(), which cannot
 * overflow.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: jet_thread_shared_data_t, list_head_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the string literals of the Design
 * steps are wrapped at their spaces, each in one code span (they were cut
 * inside names).
 */

/**
 * @file
 * The kernel's internal operations on ARINC 653 threads: creation, the
 * eligible (ready) queue ordered by priority, waiting, waking up,
 * suspension, delayed events and deadlines. Called with local preemption
 * disabled unless noted.
 */

#include "thread_internal.h"
#include <core/sched_arinc.h>
#include <core/space.h>

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): thread and object IDs are
 * the index of an element, computed as the difference between its address
 * and the start of its array.
 */

/**
 * @funcname `thread_create`
 *
 * @description
 * Initializes a thread whose name, entry, priority, period, time capacity,
 * deadline and stack size are set already: allocates its user stack and puts it
 * in the STOPPED state, in no queue.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
 *
 * @return TRUE on success, FALSE if the stack can't be allocated.
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
 * | `tshd_t` | `jet_thread_shared_data_t *` | The thread's shared data |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Call `ja_ustack_alloc()` with the arguments `part->base_part.space_id` and
 *    `t->user_stack_size`, and store the result in `t->init_stack_addr`.
 * 3. If `t->init_stack_addr` is equal to `0U`:
 *    1. Return `FALSE`.
 * 4. Set `tshd_t` to `part->kshd->tshd + (t - part->threads)`.
 * 5. Set `tshd_t->msection_count` to `0`.
 * 6. Set `tshd_t->msection_entering` to `NULL`.
 * 7. Set `t->sp` to `NULL`.
 * 8. Set `t->entry_sp_user` to `NULL`.
 * 9. Set `t->priority` to `t->base_priority`.
 * 10. Set `t->state` to `POK_STATE_STOPPED`.
 * 11. Set `t->suspended` to `FALSE`.
 * 12. Set `t->relations_stop.donate_target` to `NULL`.
 * 13. Set `t->relations_stop.first_donator` to `NULL`.
 * 14. Set `t->relations_stop.next_donator` to `NULL`.
 * 15. Set `t->msection_entering` to `NULL`.
 * 16. Call `delayed_event_init()` with the address of
 *     `t->thread_deadline_event`.
 * 17. Call `delayed_event_init()` with the address of
 *     `t->thread_delayed_event`.
 * 18. Call `INIT_LIST_HEAD()` with the address of `t->wait_elem`.
 * 19. Call `INIT_LIST_HEAD()` with the address of `t->eligible_elem`.
 * 20. Call `INIT_LIST_HEAD()` with the address of `t->error_elem`.
 * 21. Return `TRUE`.
 *
 * @calls
 * - `ja_ustack_alloc()`
 * - `delayed_event_init()`
 * - `INIT_LIST_HEAD()`
 */
pok_bool_t thread_create(pok_thread_t* t)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    t->init_stack_addr = ja_ustack_alloc(
        part->base_part.space_id,
        t->user_stack_size);

    if(t->init_stack_addr == 0U) {
        return FALSE;
    }

    // Initialize thread shared data
    jet_thread_shared_data_t* tshd_t = part->kshd->tshd
	+ (t - part->threads);
    tshd_t->msection_count = 0;
    tshd_t->msection_entering = NULL;

    /*
     * Do not modify stack here: it will be filled when thread will run.
     */
    t->sp = NULL;
#ifdef POK_NEEDS_GDB
    t->entry_sp_user = NULL;
#endif

    t->priority = t->base_priority;
    t->state = POK_STATE_STOPPED;

    t->suspended = FALSE;

    t->relations_stop.donate_target = NULL;
    t->relations_stop.first_donator = NULL;
    t->relations_stop.next_donator = NULL;

    t->msection_entering = NULL;

    delayed_event_init(&t->thread_deadline_event);
    delayed_event_init(&t->thread_delayed_event);
    INIT_LIST_HEAD(&t->wait_elem);
    INIT_LIST_HEAD(&t->eligible_elem);
    INIT_LIST_HEAD(&t->error_elem);

    return TRUE;
}

/**
 * @funcname `thread_is_eligible`
 *
 * @description
 * Tells whether a thread is in the eligible queue (the main and error threads
 * never are).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in] The thread. |
 *
 * @return TRUE if it is eligible.
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
 * 1. Return `!list_empty(&t->eligible_elem)`.
 *
 * @calls
 * - `list_empty()`
 */
static pok_bool_t thread_is_eligible(pok_thread_t* t)
{
    return !list_empty(&t->eligible_elem);
}

/**
 * @funcname `thread_set_eligible`
 *
 * @description
 * Puts a thread into the eligible queue, after the threads of the same or a
 * higher priority; asks for a reschedule if it becomes first. The thread
 * mustn't be eligible already (NORMAL mode only).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `next` | `list_head_t *` | The queue element the thread goes before |
 * | `other_thread` | `const pok_thread_t *` | A thread of the queue |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `next` to `part->eligible_threads.next`.
 * 3. Check that `part->mode == POK_PARTITION_MODE_NORMAL` holds (assert: stop
 *    otherwise).
 * 4. Check that `!thread_is_eligible(t)` holds (assert: stop otherwise).
 * 5. While `next` is not equal to the address of `part->eligible_threads`:
 *    1. Set `other_thread` to `list_entry(next, pok_thread_t, eligible_elem)`.
 *    2. If `other_thread->priority` is less than `t->priority`:
 *       1. Leave the loop.
 *    3. Set `next` to `next->next`.
 * 6. Call `list_add_tail()` with the arguments the address of
 *    `t->eligible_elem` and `next`.
 * 7. If `t->eligible_elem.prev` is equal to the address of
 *    `part->eligible_threads`:
 *    1. Call `pok_sched_local_invalidate()`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `thread_is_eligible()`
 * - `list_add_tail()`
 * - `pok_sched_local_invalidate()`
 */
static void thread_set_eligible(pok_thread_t* t)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    /* t is inserted before this element (the list head: at the end). */
    list_head_t* next = part->eligible_threads.next;

    assert(part->mode == POK_PARTITION_MODE_NORMAL);
    assert(!thread_is_eligible(t));

    while(next != &part->eligible_threads)
    {
        const pok_thread_t* other_thread =
            list_entry(next, pok_thread_t, eligible_elem);

        if(other_thread->priority < t->priority)
        {
            break;
        }
        next = next->next;
    }
    list_add_tail(&t->eligible_elem, next);

    if(t->eligible_elem.prev == &part->eligible_threads)
    {
        // Thread is inserted into the first position.
        pok_sched_local_invalidate();
    }
}

/**
 * @funcname `thread_yield`
 *
 * @description
 * Moves an eligible thread after the other threads of its priority (after a
 * priority change, or a yield).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
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
 * 2. If `thread_is_eligible(t)` is true:
 *    1. If `t->eligible_elem.prev` is equal to the address of
 *       `part->eligible_threads`:
 *       1. Call `pok_sched_local_invalidate()`.
 *    2. Call `list_del_init()` with the address of `t->eligible_elem`.
 *    3. Call `thread_set_eligible()` with the argument `t`.
 *
 * @calls
 * - `thread_is_eligible()`
 * - `pok_sched_local_invalidate()`
 * - `list_del_init()`
 * - `thread_set_eligible()`
 */
void thread_yield(pok_thread_t *t)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    if(thread_is_eligible(t))
    {
        // TODO: Improve performance
        if(t->eligible_elem.prev == &part->eligible_threads)
        {
            // Thread is removed from the first position.
            pok_sched_local_invalidate();
        }
        list_del_init(&t->eligible_elem);
        thread_set_eligible(t);
    }
}

/**
 * @funcname `thread_set_uneligible`
 *
 * @description
 * Removes a thread from the eligible queue, if it is there; asks for a
 * reschedule if it was first.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
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
 * 2. If `list_empty(&t->eligible_elem)` is false:
 *    1. If `t->eligible_elem.prev` is equal to the address of
 *       `part->eligible_threads`:
 *       1. Call `pok_sched_local_invalidate()`.
 *    2. Call `list_del_init()` with the address of `t->eligible_elem`.
 *
 * @calls
 * - `list_empty()`
 * - `pok_sched_local_invalidate()`
 * - `list_del_init()`
 */
static void thread_set_uneligible(pok_thread_t* t)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    if(!list_empty(&t->eligible_elem))
    {
        if(t->eligible_elem.prev == &part->eligible_threads)
        {
            // Thread is removed from the first position.
            pok_sched_local_invalidate();
        }
	list_del_init(&t->eligible_elem);
    }
}

/**
 * @funcname `thread_process_delayed_event`
 *
 * @description
 * The callback of a thread's delayed event: calls the function the event was
 * set for.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `handler_id` | `uint16_t` | [in] The thread's ID. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | The thread |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `t` to the address of `current_partition_arinc->threads[handler_id]`.
 * 2. Call the function `t->thread_delayed_func` with the argument `t`.
 *
 * @calls
 * - `t->thread_delayed_func()` (indirect)
 */
static void thread_process_delayed_event(uint16_t handler_id)
{
    pok_thread_t* t = &current_partition_arinc->threads[handler_id];
    t->thread_delayed_func(t);
}

/**
 * @funcname `thread_delay_event`
 *
 * @description
 * Sets a thread's delayed event: a function called for the thread at a time.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
 * | `delay_time` | `pok_time_t` | [in] The time. |
 * | `thread_delayed_func` | `void (*)(pok_thread_t* t)` | [in] The function. |
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
 * 2. Set `t->thread_delayed_func` to `thread_delayed_func`.
 * 3. Call `delayed_event_add()` with the arguments the address of
 *    `part->partition_delayed_events`, the address of
 *    `t->thread_delayed_event`, `delay_time`, `t - part->threads` and the
 *    address of `thread_process_delayed_event`.
 *
 * @calls
 * - `delayed_event_add()`
 */
void thread_delay_event(pok_thread_t* t, pok_time_t delay_time,
	void (*thread_delayed_func)(pok_thread_t* t))
{
    pok_partition_arinc_t* part = current_partition_arinc;

    t->thread_delayed_func = thread_delayed_func;

    delayed_event_add(&part->partition_delayed_events,
	&t->thread_delayed_event, delay_time,
	t - part->threads,
	&thread_process_delayed_event);
}

/**
 * @funcname `thread_deadline_occured`
 *
 * @description
 * The callback of a thread's deadline event: reports the missed deadline (debug
 * output) and raises the deadline missed error.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `handler_id` | `uint16_t` | [in] The thread's ID. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_t *` | The thread |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `thread` to the address of
 *    `current_partition_arinc->threads[handler_id]`.
 * 2. Evaluate
 *    `printf_debug("[%%lld] Deadline miss occured for thread %%s (Id: %%d,
 *    Part: %%d)\n", (long long)jet_system_time(), thread->name,
 *    (int)handler_id, current_partition->partition_id)`.
 * 3. Call `pok_thread_emit_deadline_missed()` with the argument `thread`.
 *
 * @calls
 * - `printf()`
 * - `ja_system_time()`
 * - `pok_thread_emit_deadline_missed()`
 */
static void thread_deadline_occured(uint16_t handler_id)
{
    pok_thread_t* thread = &current_partition_arinc->threads[handler_id];
    printf_debug("[%lld] Deadline miss occured for thread %s (Id: %d, Part: %d)\n",
                 (long long)jet_system_time(), thread->name,
                 (int)handler_id,
             current_partition->partition_id);
    pok_thread_emit_deadline_missed(thread);

    // TODO: if error was ignored, what to do?
}

/**
 * @funcname `thread_set_deadline`
 *
 * @description
 * Sets (or moves) a thread's deadline event.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
 * | `deadline_time` | `pok_time_t` | [in] The deadline. |
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
 * 2. Call `delayed_event_add()` with the arguments the address of
 *    `part->partition_delayed_events`, the address of
 *    `t->thread_deadline_event`, `deadline_time`, `t - part->threads` and the
 *    address of `thread_deadline_occured`.
 *
 * @calls
 * - `delayed_event_add()`
 */
void thread_set_deadline(pok_thread_t* t, pok_time_t deadline_time)
{

    pok_partition_arinc_t* part = current_partition_arinc;
    delayed_event_add(&part->partition_delayed_events,
	&t->thread_deadline_event, deadline_time,
        t - part->threads,
	&thread_deadline_occured);
}

/**
 * @funcname `thread_delay_event_cancel`
 *
 * @description
 * Cancels a thread's delayed event, if it is set.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
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
 * 2. Call `delayed_event_remove()` with the arguments the address of
 *    `part->partition_delayed_events` and the address of
 *    `t->thread_delayed_event`.
 *
 * @calls
 * - `delayed_event_remove()`
 */
void thread_delay_event_cancel(pok_thread_t* t)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    delayed_event_remove(&part->partition_delayed_events,
	&t->thread_delayed_event);
}


/**
 * @funcname `thread_stop`
 *
 * @description
 * Stops a thread in NORMAL mode: it leaves every queue (except the error
 * handler's), its events are cancelled, and it releases the partition's lock if
 * it held it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
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
 * 2. Check that `t->state != POK_STATE_STOPPED` holds (assert: stop otherwise).
 * 3. Set `t->state` to `POK_STATE_STOPPED`.
 * 4. Call `thread_set_uneligible()` with the argument `t`.
 * 5. Call `thread_delay_event_cancel()` with the argument `t`.
 * 6. Call `delayed_event_remove()` with the arguments the address of
 *    `part->partition_delayed_events` and the address of
 *    `t->thread_deadline_event`.
 * 7. Call `pok_thread_wq_remove()` with the argument `t`.
 * 8. If `part->lock_level` is not equal to `0U` and `part->thread_locked` is
 *    equal to `t`:
 *    1. Set `current_partition_arinc->lock_level` to `0`.
 *    2. Call `pok_sched_local_invalidate()`.
 * 9. If `t->is_unrecoverable` is true:
 *    1. Set `t->is_unrecoverable` to `FALSE`.
 *    2. Decrement `part->nthreads_unrecoverable`.
 * 10. Set `t->msection_entering` to `NULL`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `thread_set_uneligible()`
 * - `thread_delay_event_cancel()`
 * - `delayed_event_remove()`
 * - `pok_thread_wq_remove()`
 * - `pok_sched_local_invalidate()`
 */
void thread_stop(pok_thread_t* t)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    assert(t->state != POK_STATE_STOPPED);
    t->state = POK_STATE_STOPPED;

    thread_set_uneligible(t);

    // Remove thread from all queues except one for error handler.
    thread_delay_event_cancel(t);

    delayed_event_remove(&part->partition_delayed_events,
	&t->thread_deadline_event);

    pok_thread_wq_remove(t);

    if((part->lock_level != 0U) && (part->thread_locked == t))
    {
        current_partition_arinc->lock_level = 0;
	pok_sched_local_invalidate(); // Thread could be non-highest priority thread when stopped.
    }

    if(t->is_unrecoverable)
    {
        t->is_unrecoverable = FALSE;
        part->nthreads_unrecoverable--;
    }

    t->msection_entering = NULL;
}

/**
 * @funcname `thread_is_waiting_allowed`
 *
 * @description
 * Tells whether the current thread may wait: not while the partition is locked
 * (or in an init mode), not in the error handler. Waiting inside an msection,
 * or while another thread waits for this one to stop, is a partition error.
 *
 * @return TRUE if the current thread may wait.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd_current` | `jet_thread_shared_data_t *` | The current thread's shared data |
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
 * 3. If `(part->lock_level != 0U) #ifdef POK_NEEDS_ERROR_HANDLING ||`
 *    `(part->thread_error == thread_current)` is true:
 *    1. Return `FALSE`.
 * 4. Set `kshd_current` to
 *    `part->kshd->tshd + (thread_current - part->threads)`.
 * 5. Check that `kshd_current->msection_count == 0U` holds (assert_os: stop
 *    otherwise).
 * 6. Check that `thread_current->relations_stop.first_donator == NULL` holds
 *    (assert_os: stop otherwise).
 * 7. Return `TRUE`.
 *
 * @calls
 * - `pok_assertion_fail()`
 */
pok_bool_t thread_is_waiting_allowed(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread_current = part->thread_current;

    // In the INIT_* mode lock level is positive, no need to check it
    // explicitely.
    if((part->lock_level != 0U)
#ifdef POK_NEEDS_ERROR_HANDLING
	|| (part->thread_error == thread_current) /* error thread cannot wait */
#endif
    ) {
        return FALSE;
    }

    jet_thread_shared_data_t* kshd_current = part->kshd->tshd
	+ (thread_current - part->threads);

    // It is prohibited to call waiting function inside msection.
    assert_os(kshd_current->msection_count == 0U);

    /*
     * It is prohibited to call waiting function when someone waits us for terminate.
     *
     * Yes, we doesn't trust 'msection_count', which can be set from user space.
     */
    assert_os(thread_current->relations_stop.first_donator == NULL);

    return TRUE;
}


/**
 * @funcname `thread_start`
 *
 * @description
 * Makes a thread runnable in NORMAL mode, and eligible unless it is suspended.
 * The thread mustn't wait for anything.
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
 * 1. Set `t->state` to `POK_STATE_RUNNABLE`.
 * 2. If `t->suspended` is false:
 *    1. Call `thread_set_eligible()` with the argument `t`.
 *
 * @calls
 * - `thread_set_eligible()`
 */
void thread_start(pok_thread_t* t)
{
    t->state = POK_STATE_RUNNABLE;

    if(!t->suspended)
    {
        thread_set_eligible(t);
    }
}

/**
 * @funcname `thread_wait`
 *
 * @description
 * Makes a thread wait (for anything but suspension): it leaves the eligible
 * queue.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
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
 * 1. Check that `current_partition_arinc->mode == POK_PARTITION_MODE_NORMAL`
 *    holds (assert: stop otherwise).
 * 2. Set `t->state` to `POK_STATE_WAITING`.
 * 3. Call `thread_set_uneligible()` with the argument `t`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `thread_set_uneligible()`
 */
void thread_wait(pok_thread_t* t)
{
    assert(current_partition_arinc->mode == POK_PARTITION_MODE_NORMAL);

    t->state = POK_STATE_WAITING;
    thread_set_uneligible(t);
}

/**
 * @funcname `thread_wake_up`
 *
 * @description
 * Wakes a waiting thread up: when its sleep is over, when the object it waits
 * for is available (it left the object's queue already), when its timeout fires
 * (the wait result is then TIMEOUT), or when its partition goes to NORMAL mode.
 * Usable as a callback.
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
 * 1. Check that `t->state == POK_STATE_WAITING` holds (assert: stop otherwise).
 * 2. Set `t->state` to `POK_STATE_RUNNABLE`.
 * 3. Call `thread_delay_event_cancel()` with the argument `t`.
 * 4. If `list_empty(&t->wait_elem)` is false:
 *    1. Call `list_del_init()` with the address of `t->wait_elem`.
 *    2. Set `t->wait_result` to `POK_ERRNO_TIMEOUT`.
 * 5. Otherwise, if `t->msection_entering` is not NULL:
 *    1. Set `t->wait_result` to `POK_ERRNO_TIMEOUT`.
 * 6. Otherwise:
 *    1. Do nothing.
 * 7. If `t->suspended` is false:
 *    1. Call `thread_set_eligible()` with the argument `t`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `thread_delay_event_cancel()`
 * - `list_empty()`
 * - `list_del_init()`
 * - `thread_set_eligible()`
 */
void thread_wake_up(pok_thread_t* t)
{
    assert(t->state == POK_STATE_WAITING);

    t->state = POK_STATE_RUNNABLE;

    // For the case 3. Cancel possible timeout.
    thread_delay_event_cancel(t);

    if(!list_empty(&t->wait_elem))
    {
        // Case 2.

        // Cancel wait on object.
        list_del_init(&t->wait_elem);
        // Set flag that we has been interrupted by timeout.
        t->wait_result = POK_ERRNO_TIMEOUT;
    }
    else if(t->msection_entering != NULL)
    {
	t->wait_result = POK_ERRNO_TIMEOUT;
    }
    else
    {
        // Neither waiting on an object nor entering a section.
    }

    if(!t->suspended)
    {
        thread_set_eligible(t);
    }
}


/**
 * @funcname `thread_wait_timed`
 *
 * @description
 * Makes a thread wait until a time, when it is woken up.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_t *` | [in,out] The thread. |
 * | `time` | `pok_time_t` | [in] The time (not infinite). |
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
 * 1. Check that `thread != NULL` holds (assert: stop otherwise).
 * 2. Check that `!pok_time_is_infinity(time)` holds (assert: stop otherwise).
 * 3. Call `thread_wait()` with the argument `thread`.
 * 4. Call `thread_delay_event()` with the arguments `thread`, `time` and the
 *    address of `thread_wake_up`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `pok_time_is_infinity()`
 * - `thread_wait()`
 * - `thread_delay_event()`
 */
void thread_wait_timed(pok_thread_t *thread, pok_time_t time)
{
    assert(thread != NULL);
    assert(!pok_time_is_infinity(time));

    thread_wait(thread);
    thread_delay_event(thread, time, &thread_wake_up);
}


/**
 * @funcname `thread_suspend`
 *
 * @description
 * Suspends a thread (not suspended already): it leaves the eligible queue.
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
 * 1. Set `t->suspended` to `TRUE`.
 * 2. Call `thread_set_uneligible()` with the argument `t`.
 *
 * @calls
 * - `thread_set_uneligible()`
 */
void thread_suspend(pok_thread_t* t)
{
    t->suspended = TRUE;
    thread_set_uneligible(t);
}

/**
 * @funcname `thread_resume_waited`
 *
 * @description
 * The callback of a timed suspension that ran out: the thread is runnable
 * again, with the wait result TIMEOUT.
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
 * 1. Check that `t->state == POK_STATE_WAITING` holds (assert: stop otherwise).
 * 2. Set `t->suspended` to `FALSE`.
 * 3. Set `t->state` to `POK_STATE_RUNNABLE`.
 * 4. Set `t->wait_result` to `POK_ERRNO_TIMEOUT`.
 * 5. Call `thread_set_eligible()` with the argument `t`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `thread_set_eligible()`
 */
static void thread_resume_waited(pok_thread_t* t)
{
    assert(t->state == POK_STATE_WAITING);

    t->suspended = FALSE;
    t->state = POK_STATE_RUNNABLE;

    // Set flag that we has been interrupted by timeout.
    t->wait_result = POK_ERRNO_TIMEOUT;

    thread_set_eligible(t);
}

/**
 * @funcname `thread_suspend_timed`
 *
 * @description
 * Suspends the current thread for a duration (ARINC 653 allows a timed
 * suspension of the current thread only, which waits for nothing else).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
 * | `time` | `pok_time_t` | [in] The duration. |
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
 * 1. Set `t->suspended` to `TRUE`.
 * 2. Call `thread_set_uneligible()` with the argument `t`.
 * 3. Call `thread_wait()` with the argument `t`.
 * 4. Call `thread_delay_event()` with the arguments `t`,
 *    `pok_time_add(jet_system_time(), time)` and the address of
 *    `thread_resume_waited`.
 *
 * @calls
 * - `thread_set_uneligible()`
 * - `thread_wait()`
 * - `thread_delay_event()`
 * - `pok_time_add()`
 * - `ja_system_time()`
 */
void thread_suspend_timed(pok_thread_t* t, pok_time_t time)
{
    t->suspended = TRUE;
    thread_set_uneligible(t);
    thread_wait(t);
    thread_delay_event(t, pok_time_add(jet_system_time(), time),
        &thread_resume_waited);
}


/**
 * @funcname `thread_resume`
 *
 * @description
 * Resumes a suspended thread: a timed suspension is cancelled (wait result OK),
 * and a runnable thread becomes eligible.
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
 * 1. Set `t->suspended` to `FALSE`.
 * 2. If `delayed_event_is_active(&t->thread_delayed_event) &&`
 *    `(t->thread_delayed_func == &thread_resume_waited)` holds:
 *    1. Set `t->state` to `POK_STATE_RUNNABLE`.
 *    2. Call `thread_delay_event_cancel()` with the argument `t`.
 *    3. Set `t->wait_result` to `POK_ERRNO_OK`.
 * 3. If `t->state` is equal to `POK_STATE_RUNNABLE`:
 *    1. Call `thread_set_eligible()` with the argument `t`.
 *
 * @calls
 * - `delayed_event_is_active()`
 * - `thread_delay_event_cancel()`
 * - `thread_set_eligible()`
 */
void thread_resume(pok_thread_t* t)
{
    t->suspended = FALSE;
    if(delayed_event_is_active(&t->thread_delayed_event)
	&& (t->thread_delayed_func == &thread_resume_waited))
    {
	// We are waited on timer for suspencion. Cancel that waiting.
	t->state = POK_STATE_RUNNABLE;

	thread_delay_event_cancel(t);

        // Set flag that we doesn't hit timeout.
        t->wait_result = POK_ERRNO_OK;
    }

    if(t->state == POK_STATE_RUNNABLE)
    {
        thread_set_eligible(t);
    }
}
