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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_CREATE_01}: `thread_create()` shall make
 *   a process whose attributes are set a created, stopped process with its user
 *   stack, in no queue, and fail if its stack cannot be allocated.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_001}
 *   - \srqref{SRQ_JET_MEM_008}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_CREATE_01}: `thread_create()` shall set
 *   `init_stack_addr` of `t` to the result of `ja_ustack_alloc()` for
 *   `space_id` of the base partition of `current_partition_arinc` and
 *   `user_stack_size` of `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_CREATE_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_CREATE_02}: If that result is 0,
 *   `thread_create()` shall return `FALSE` and change nothing else.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_CREATE_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_CREATE_03}: If that result is not 0,
 *   `thread_create()` shall set `msection_count` to 0 and `msection_entering`
 *   to NULL in the shared data of the thread (`kshd->tshd` plus its index), set
 *   `sp`, `entry_sp_user` (with `POK_NEEDS_GDB`), `donate_target`,
 *   `first_donator` and `next_donator` of `relations_stop`, and
 *   `msection_entering` of `t` to NULL, its `priority` to its `base_priority`,
 *   its `state` to `POK_STATE_STOPPED` and `suspended` to `FALSE`, call
 *   `delayed_event_init()` for `thread_deadline_event` and
 *   `thread_delayed_event` and `INIT_LIST_HEAD()` for `wait_elem`,
 *   `eligible_elem` and `error_elem`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_CREATE_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_01}
 * 2. Call `ja_ustack_alloc()` with the arguments `part->base_part.space_id` and
 *    `t->user_stack_size`, and store the result in `t->init_stack_addr`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_01}
 * 3. If `t->init_stack_addr` is equal to `0U`:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_02}
 *    1. Return `FALSE`.
 * 4. Set `tshd_t` to `part->kshd->tshd + (t - part->threads)`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 5. Set `tshd_t->msection_count` to `0`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 6. Set `tshd_t->msection_entering` to `NULL`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 7. Set `t->sp` to `NULL`. \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 8. Set `t->entry_sp_user` to `NULL`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 9. Set `t->priority` to `t->base_priority`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 10. Set `t->state` to `POK_STATE_STOPPED`.
 *     \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 11. Set `t->suspended` to `FALSE`.
 *     \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 12. Set `t->relations_stop.donate_target` to `NULL`.
 *     \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 13. Set `t->relations_stop.first_donator` to `NULL`.
 *     \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 14. Set `t->relations_stop.next_donator` to `NULL`.
 *     \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 15. Set `t->msection_entering` to `NULL`.
 *     \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 16. Call `delayed_event_init()` with the address of
 *     `t->thread_deadline_event`. \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 17. Call `delayed_event_init()` with the address of
 *     `t->thread_delayed_event`. \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 18. Call `INIT_LIST_HEAD()` with the address of `t->wait_elem`.
 *     \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 19. Call `INIT_LIST_HEAD()` with the address of `t->eligible_elem`.
 *     \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 20. Call `INIT_LIST_HEAD()` with the address of `t->error_elem`.
 *     \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
 * 21. Return `TRUE`. \llrref{LLR_THREAD_INTERNAL_THREAD_CREATE_03}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_IS_ELIGIBLE_01}: `thread_is_eligible()`
 *   shall tell whether a process is among the ready processes of its partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_004}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_IS_ELIGIBLE_01}: If `list_empty()`
 *   returns false for `eligible_elem` of `t`, `thread_is_eligible()` shall
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_IS_ELIGIBLE_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_IS_ELIGIBLE_02}: If `list_empty()`
 *   returns true for `eligible_elem` of `t`, `thread_is_eligible()` shall
 *   return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_IS_ELIGIBLE_01}
 *
 * @design
 * 1. Return `!list_empty(&t->eligible_elem)`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_IS_ELIGIBLE_01},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_IS_ELIGIBLE_02}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_01}: `thread_set_eligible()`
 *   shall make a process of a partition in `NORMAL` mode ready, after the ready
 *   processes of the same or a higher priority, and make the partition choose
 *   again if it becomes the first.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_004}
 *   - \srqref{SRQ_JET_THR_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_01}: `thread_set_eligible()`
 *   shall insert `eligible_elem` of `t` into `eligible_threads` of the
 *   partition with `list_add_tail()`, before the first thread whose `priority`
 *   is less than `priority` of `t`, or at the end if there is none.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_02}: If and only if `t` is
 *   then the first thread of `eligible_threads` (`prev` of its `eligible_elem`
 *   is the head), `thread_set_eligible()` shall call
 *   `pok_sched_local_invalidate()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_01}
 * 2. Set `next` to `part->eligible_threads.next`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_01}
 * 3. While `next` is not equal to the address of `part->eligible_threads`:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_01}
 *    1. Set `other_thread` to `list_entry(next, pok_thread_t, eligible_elem)`.
 *    2. If `other_thread->priority` is less than `t->priority`:
 *       1. Leave the loop.
 *    3. Set `next` to `next->next`.
 * 4. Call `list_add_tail()` with the arguments the address of
 *    `t->eligible_elem` and `next`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_01}
 * 5. If `t->eligible_elem.prev` is equal to the address of
 *    `part->eligible_threads`:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_ELIGIBLE_02}
 *    1. Call `pok_sched_local_invalidate()`.
 *
 * @calls
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_YIELD_01}: `thread_yield()` shall move a
 *   ready process after the ready processes of its priority.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *   - \srqref{SRQ_JET_THR_016}
 *   - \srqref{SRQ_JET_THR_021}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_YIELD_01}: If `thread_is_eligible()`
 *   returns true for `t`, `thread_yield()` shall call `list_del_init()` for its
 *   `eligible_elem` and then `thread_set_eligible()` with `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_YIELD_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_YIELD_02}: If `thread_is_eligible()`
 *   returns true for `t`, `thread_yield()` shall first call
 *   `pok_sched_local_invalidate()` if and only if `t` is the first thread of
 *   `eligible_threads`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_YIELD_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_YIELD_03}: If `thread_is_eligible()`
 *   returns false for `t`, `thread_yield()` shall leave the eligible threads as
 *   they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_YIELD_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_YIELD_01},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_YIELD_02},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_YIELD_03}
 * 2. If `thread_is_eligible(t)` is true:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_YIELD_01},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_YIELD_02},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_YIELD_03}
 *    1. If `t->eligible_elem.prev` is equal to the address of
 *       `part->eligible_threads`: \llrref{LLR_THREAD_INTERNAL_THREAD_YIELD_02}
 *       1. Call `pok_sched_local_invalidate()`.
 *    2. Call `list_del_init()` with the address of `t->eligible_elem`.
 *       \llrref{LLR_THREAD_INTERNAL_THREAD_YIELD_01}
 *    3. Call `thread_set_eligible()` with the argument `t`.
 *       \llrref{LLR_THREAD_INTERNAL_THREAD_YIELD_01}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_01}:
 *   `thread_set_uneligible()` shall remove a process from the ready processes
 *   of its partition, making the partition choose again if it was the first.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_004}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_01}: If `list_empty()`
 *   returns false for `eligible_elem` of `t`, `thread_set_uneligible()` shall
 *   call `list_del_init()` for it, after calling `pok_sched_local_invalidate()`
 *   if and only if `t` is the first thread of `eligible_threads`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_02}: If `list_empty()`
 *   returns true for `eligible_elem` of `t`, `thread_set_uneligible()` shall
 *   leave the eligible threads as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_01},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_02}
 * 2. If `list_empty(&t->eligible_elem)` is false:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_01},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_02}
 *    1. If `t->eligible_elem.prev` is equal to the address of
 *       `part->eligible_threads`:
 *       \llrref{LLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_01}
 *       1. Call `pok_sched_local_invalidate()`.
 *    2. Call `list_del_init()` with the address of `t->eligible_elem`.
 *       \llrref{LLR_THREAD_INTERNAL_THREAD_SET_UNELIGIBLE_01}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_PROCESS_DELAYED_EVENT_01}:
 *   `thread_process_delayed_event()` shall run the action that a delayed event
 *   of a process was set for, at its time.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_008}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_PROCESS_DELAYED_EVENT_01}:
 *   `thread_process_delayed_event()` shall call the thread's
 *   `thread_delayed_func` for the thread of the handler ID.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_PROCESS_DELAYED_EVENT_01}
 *
 * @design
 * 1. Set `t` to the address of `current_partition_arinc->threads[handler_id]`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_PROCESS_DELAYED_EVENT_01}
 * 2. Call the function `t->thread_delayed_func` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_PROCESS_DELAYED_EVENT_01}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_01}: `thread_delay_event()`
 *   shall arrange for an action to be done for a process at a time, replacing
 *   the action set before.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_01}: `thread_delay_event()`
 *   shall set `thread_delayed_func` of `t` to `thread_delayed_func` and call
 *   `delayed_event_add()` once with `partition_delayed_events` of the
 *   partition, `thread_delayed_event` of `t`, `delay_time`, the index of `t` in
 *   `threads` and `thread_process_delayed_event`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_01}
 * 2. Set `t->thread_delayed_func` to `thread_delayed_func`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_01}
 * 3. Call `delayed_event_add()` with the arguments the address of
 *    `part->partition_delayed_events`, the address of
 *    `t->thread_delayed_event`, `delay_time`, `t - part->threads` and the
 *    address of `thread_process_delayed_event`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_01}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_DEADLINE_OCCURED_01}:
 *   `thread_deadline_occured()` shall report that a process missed its
 *   deadline, as a deadline missed error.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_012}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_DEADLINE_OCCURED_01}:
 *   `thread_deadline_occured()` shall call `printf_debug()` (`printf()` with
 *   `POK_NEEDS_DEBUG`) with `"[%%lld] Deadline miss occured for thread %%s (Id:
 *   %%d, Part: %%d)\n"`, the result of `jet_system_time()`, `name` of the
 *   thread `threads[handler_id]`, `handler_id` and `partition_id` of
 *   `current_partition`, and then `pok_thread_emit_deadline_missed()` with that
 *   thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_DEADLINE_OCCURED_01}
 *
 * @design
 * 1. Set `thread` to the address of
 *    `current_partition_arinc->threads[handler_id]`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_DEADLINE_OCCURED_01}
 * 2. Evaluate
 *    `printf_debug("[%%lld] Deadline miss occured for thread %%s (Id: %%d,
 *    Part: %%d)\n", (long long)jet_system_time(), thread->name,
 *    (int)handler_id, current_partition->partition_id)`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_DEADLINE_OCCURED_01}
 * 3. Call `pok_thread_emit_deadline_missed()` with the argument `thread`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_DEADLINE_OCCURED_01}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_SET_DEADLINE_01}: `thread_set_deadline()`
 *   shall set or move the deadline of a process to a time.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_012}
 *   - \srqref{SRQ_JET_THR_011}
 *   - \srqref{SRQ_JET_THR_022}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_SET_DEADLINE_01}: `thread_set_deadline()`
 *   shall call `delayed_event_add()` once with `partition_delayed_events` of
 *   the partition, `thread_deadline_event` of `t`, `deadline_time`, the index
 *   of `t` in `threads` and `thread_deadline_occured`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_SET_DEADLINE_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_DEADLINE_01}
 * 2. Call `delayed_event_add()` with the arguments the address of
 *    `part->partition_delayed_events`, the address of
 *    `t->thread_deadline_event`, `deadline_time`, `t - part->threads` and the
 *    address of `thread_deadline_occured`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SET_DEADLINE_01}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_CANCEL_01}:
 *   `thread_delay_event_cancel()` shall cancel the delayed action of a process,
 *   if it is set.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_CANCEL_01}:
 *   `thread_delay_event_cancel()` shall call `delayed_event_remove()` once with
 *   `partition_delayed_events` of the partition and `thread_delayed_event` of
 *   `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_CANCEL_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_CANCEL_01}
 * 2. Call `delayed_event_remove()` with the arguments the address of
 *    `part->partition_delayed_events` and the address of
 *    `t->thread_delayed_event`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_DELAY_EVENT_CANCEL_01}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_STOP_01}: `thread_stop()` shall mark a
 *   process as stopped and take it out of the ready processes and every wait
 *   queue.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 *   - \srqref{SRQ_JET_THR_019}
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_STOP_02}: `thread_stop()` shall cancel
 *   the delayed action and the deadline of the process.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 *   - \srqref{SRQ_JET_THR_019}
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_STOP_03}: `thread_stop()` shall release
 *   the lock of preemption that the process holds.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_STOP_01}: `thread_stop()` shall set
 *   `state` of `t` to `POK_STATE_STOPPED`, and call `thread_set_uneligible()`
 *   and `thread_delay_event_cancel()` with `t`, `delayed_event_remove()` with
 *   the delayed events of the partition and `thread_deadline_event` of `t`, and
 *   `pok_thread_wq_remove()` with `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_STOP_01}
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_STOP_02}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_STOP_02}: If and only if `lock_level` of
 *   the partition is not 0 and `thread_locked` is `t`, `thread_stop()` shall
 *   set `lock_level` to 0 and call `pok_sched_local_invalidate()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_STOP_03}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_STOP_03}: If and only if
 *   `is_unrecoverable` of `t` is true, `thread_stop()` shall set it to `FALSE`
 *   and decrement `nthreads_unrecoverable` of the partition.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_STOP_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_STOP_04}: `thread_stop()` shall set
 *   `msection_entering` of `t` to NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_STOP_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_01},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_02},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_03}
 * 2. Set `t->state` to `POK_STATE_STOPPED`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_01}
 * 3. Call `thread_set_uneligible()` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_01}
 * 4. Call `thread_delay_event_cancel()` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_01}
 * 5. Call `delayed_event_remove()` with the arguments the address of
 *    `part->partition_delayed_events` and the address of
 *    `t->thread_deadline_event`. \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_01}
 * 6. Call `pok_thread_wq_remove()` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_01}
 * 7. If `part->lock_level` is not equal to `0U` and `part->thread_locked` is
 *    equal to `t`: \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_02}
 *    1. Set `current_partition_arinc->lock_level` to `0`.
 *    2. Call `pok_sched_local_invalidate()`.
 * 8. If `t->is_unrecoverable` is true:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_03}
 *    1. Set `t->is_unrecoverable` to `FALSE`.
 *    2. Decrement `part->nthreads_unrecoverable`.
 * 9. Set `t->msection_entering` to `NULL`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_STOP_04}
 *
 * @calls
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_01}:
 *   `thread_is_waiting_allowed()` shall tell whether the current process may
 *   wait: not while the partition is locked, nor as the error handler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_007}
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_02}:
 *   `thread_is_waiting_allowed()` shall treat a wait inside a critical section,
 *   or while another process waits for this one to stop, as an error of the
 *   partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_HM_008}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_01}: If `lock_level`
 *   of the partition is not 0, or (with `POK_NEEDS_ERROR_HANDLING`)
 *   `thread_error` of the partition is `thread_current`,
 *   `thread_is_waiting_allowed()` shall return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_02}: If `lock_level`
 *   of the partition is 0 and `thread_error` is not `thread_current`,
 *   `thread_is_waiting_allowed()` shall return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_02}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_01},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_02}
 * 2. Set `thread_current` to `part->thread_current`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_01},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_02}
 * 3. If `(part->lock_level != 0U) #ifdef POK_NEEDS_ERROR_HANDLING ||`
 *    `(part->thread_error == thread_current)` is true:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_01},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_02}
 *    1. Return `FALSE`.
 *       \llrref{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_01}
 * 4. Set `kshd_current` to
 *    `part->kshd->tshd + (thread_current - part->threads)`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_02}
 * 5. Return `TRUE`. \llrref{LLR_THREAD_INTERNAL_THREAD_IS_WAITING_ALLOWED_02}
 *
 * @calls
 * None.
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_START_01}: `thread_start()` shall make a
 *   process runnable, and ready unless it is suspended.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_009}
 *   - \srqref{SRQ_JET_THR_019}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_START_01}: `thread_start()` shall set
 *   `state` of `t` to `POK_STATE_RUNNABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_START_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_START_02}: If and only if `suspended` of
 *   `t` is false, `thread_start()` shall call `thread_set_eligible()` with `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_START_01}
 *
 * @design
 * 1. Set `t->state` to `POK_STATE_RUNNABLE`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_START_01}
 * 2. If `t->suspended` is false: \llrref{LLR_THREAD_INTERNAL_THREAD_START_02}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_WAIT_01}: `thread_wait()` shall make a
 *   process of a partition in `NORMAL` mode wait, so that it is no longer
 *   ready.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_WAIT_01}: `thread_wait()` shall set
 *   `state` of `t` to `POK_STATE_WAITING` and call `thread_set_uneligible()`
 *   with `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_WAIT_01}
 *
 * @design
 * 1. Set `t->state` to `POK_STATE_WAITING`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_WAIT_01}
 * 2. Call `thread_set_uneligible()` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_WAIT_01}
 *
 * @calls
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_WAKE_UP_01}: `thread_wake_up()` shall
 *   make a waiting process runnable again, and ready unless it is suspended.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *   - \srqref{SRQ_JET_IPC_020}
 *   - \srqref{SRQ_JET_IPC_021}
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_WAKE_UP_02}: `thread_wake_up()` shall
 *   give the result time-out to a process woken while it is still in a wait
 *   queue or entering a critical section.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *   - \srqref{SRQ_JET_IPC_020}
 *   - \srqref{SRQ_JET_IPC_021}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_01}: `thread_wake_up()` shall set
 *   `state` of `t` to `POK_STATE_RUNNABLE` and call
 *   `thread_delay_event_cancel()` with `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_WAKE_UP_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_02}: If `list_empty()` returns
 *   false for `wait_elem` of `t`, `thread_wake_up()` shall call
 *   `list_del_init()` for it and set `wait_result` of `t` to
 *   `POK_ERRNO_TIMEOUT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_WAKE_UP_02}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_03}: If `list_empty()` returns
 *   true for `wait_elem` of `t` and its `msection_entering` is not NULL,
 *   `thread_wake_up()` shall set `wait_result` of `t` to `POK_ERRNO_TIMEOUT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_WAKE_UP_02}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_04}: If `list_empty()` returns
 *   true for `wait_elem` of `t` and its `msection_entering` is NULL,
 *   `thread_wake_up()` shall leave `wait_result` of `t` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_WAKE_UP_02}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_05}: If and only if `suspended`
 *   of `t` is false, `thread_wake_up()` shall call `thread_set_eligible()` with
 *   `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_WAKE_UP_01}
 *
 * @design
 * 1. Set `t->state` to `POK_STATE_RUNNABLE`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_01}
 * 2. Call `thread_delay_event_cancel()` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_01}
 * 3. If `list_empty(&t->wait_elem)` is false:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_02}
 *    1. Call `list_del_init()` with the address of `t->wait_elem`.
 *    2. Set `t->wait_result` to `POK_ERRNO_TIMEOUT`.
 * 4. Otherwise, if `t->msection_entering` is not NULL:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_03},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_04}
 *    1. Set `t->wait_result` to `POK_ERRNO_TIMEOUT`.
 * 5. If `t->suspended` is false: \llrref{LLR_THREAD_INTERNAL_THREAD_WAKE_UP_05}
 *    1. Call `thread_set_eligible()` with the argument `t`.
 *
 * @calls
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_WAIT_TIMED_01}: `thread_wait_timed()`
 *   shall make a process wait until a finite time, when it is woken.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *   - \srqref{SRQ_JET_TIME_005}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_WAIT_TIMED_01}: `thread_wait_timed()`
 *   shall call `thread_wait()` with `thread` and then `thread_delay_event()`
 *   with `thread`, `time` and `thread_wake_up`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_WAIT_TIMED_01}
 *
 * @design
 * 1. Call `thread_wait()` with the argument `thread`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_WAIT_TIMED_01}
 * 2. Call `thread_delay_event()` with the arguments `thread`, `time` and the
 *    address of `thread_wake_up`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_WAIT_TIMED_01}
 *
 * @calls
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_SUSPEND_01}: `thread_suspend()` shall
 *   suspend a process, so that it is no longer ready.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_008}
 *   - \srqref{SRQ_JET_THR_018}
 *   - \srqref{SRQ_JET_THR_006}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_SUSPEND_01}: `thread_suspend()` shall set
 *   `suspended` of `t` to `TRUE` and call `thread_set_uneligible()` once with
 *   `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_SUSPEND_01}
 *
 * @design
 * 1. Set `t->suspended` to `TRUE`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SUSPEND_01}
 * 2. Call `thread_set_uneligible()` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SUSPEND_01}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_RESUME_WAITED_01}:
 *   `thread_resume_waited()` shall make the process of a timed suspension whose
 *   time ran out ready again, with the result time-out.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_RESUME_WAITED_01}:
 *   `thread_resume_waited()` shall set `suspended` of `t` to `FALSE`, its
 *   `state` to `POK_STATE_RUNNABLE` and its `wait_result` to
 *   `POK_ERRNO_TIMEOUT`, and call `thread_set_eligible()` with `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_RESUME_WAITED_01}
 *
 * @design
 * 1. Set `t->suspended` to `FALSE`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_WAITED_01}
 * 2. Set `t->state` to `POK_STATE_RUNNABLE`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_WAITED_01}
 * 3. Set `t->wait_result` to `POK_ERRNO_TIMEOUT`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_WAITED_01}
 * 4. Call `thread_set_eligible()` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_WAITED_01}
 *
 * @calls
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_SUSPEND_TIMED_01}:
 *   `thread_suspend_timed()` shall suspend the current process until the end of
 *   a duration.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_006}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_SUSPEND_TIMED_01}:
 *   `thread_suspend_timed()` shall set `suspended` of `t` to `TRUE`, and then
 *   call `thread_set_uneligible()` with `t`, `thread_wait()` with `t`, and
 *   `thread_delay_event()` with `t`, the result of `pok_time_add()` for the
 *   result of `jet_system_time()` and `time`, and `thread_resume_waited`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_SUSPEND_TIMED_01}
 *
 * @design
 * 1. Set `t->suspended` to `TRUE`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SUSPEND_TIMED_01}
 * 2. Call `thread_set_uneligible()` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SUSPEND_TIMED_01}
 * 3. Call `thread_wait()` with the argument `t`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SUSPEND_TIMED_01}
 * 4. Call `thread_delay_event()` with the arguments `t`,
 *    `pok_time_add(jet_system_time(), time)` and the address of
 *    `thread_resume_waited`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_SUSPEND_TIMED_01}
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
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_RESUME_01}: `thread_resume()` shall end
 *   the suspension of a process and make it ready again if it is runnable.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_008}
 *   - \srqref{SRQ_JET_THR_018}
 * - \hlrid{HLR_THREAD_INTERNAL_THREAD_RESUME_02}: `thread_resume()` shall end a
 *   timed suspension of the process with the result OK.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_008}
 *
 * @llr
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_RESUME_01}: `thread_resume()` shall set
 *   `suspended` of `t` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_RESUME_01}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_RESUME_02}: If
 *   `delayed_event_is_active()` returns true for `thread_delayed_event` of `t`
 *   and its `thread_delayed_func` is `thread_resume_waited`, `thread_resume()`
 *   shall set `state` of `t` to `POK_STATE_RUNNABLE`, call
 *   `thread_delay_event_cancel()` with `t` and set its `wait_result` to
 *   `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_RESUME_02}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_RESUME_03}: If
 *   `delayed_event_is_active()` returns false for `thread_delayed_event` of
 *   `t`, or its `thread_delayed_func` is another function, `thread_resume()`
 *   shall leave its `state`, its delayed event and its `wait_result` as they
 *   are.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_RESUME_02}
 * - \llrid{LLR_THREAD_INTERNAL_THREAD_RESUME_04}: If and only if `state` of `t`
 *   is then `POK_STATE_RUNNABLE`, `thread_resume()` shall call
 *   `thread_set_eligible()` with `t`.
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_INTERNAL_THREAD_RESUME_01}
 *
 * @design
 * 1. Set `t->suspended` to `FALSE`.
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_01}
 * 2. If `delayed_event_is_active(&t->thread_delayed_event) &&`
 *    `(t->thread_delayed_func == &thread_resume_waited)` holds:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_02},
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_03}
 *    1. Set `t->state` to `POK_STATE_RUNNABLE`.
 *       \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_02}
 *    2. Call `thread_delay_event_cancel()` with the argument `t`.
 *       \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_02}
 *    3. Set `t->wait_result` to `POK_ERRNO_OK`.
 *       \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_02}
 * 3. If `t->state` is equal to `POK_STATE_RUNNABLE`:
 *    \llrref{LLR_THREAD_INTERNAL_THREAD_RESUME_04}
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
