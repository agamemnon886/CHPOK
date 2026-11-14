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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * flag_test_and_reset() takes the flag's address.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the backward
 * goto is replaced by a loop; operands of mixed precedence are
 * parenthesized; deviations are documented where a guideline can't be
 * followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: essentially
 * Boolean condition in the GDB code (built on x86_64 now).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: a switch to the idle context
 * ("do nothing", no thread) no longer reads the FP store of a NULL thread;
 * the current FP store stays that of the last thread. Doxygen headers for
 * the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: struct jet_context is used as the
 * typedef jet_context_t (declared in asp/cswitch.h); the structures and enums
 * are used by their typedef names: jet_partition_event_t,
 * jet_thread_shared_data_t, msection_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * The scheduler inside an ARINC 653 partition: the choice of the thread to
 * run (error handler, locked thread, the first eligible thread), the
 * partition's events (timers, queuing ports), the msection hand-over and the
 * context switches.
 */

#include <core/sched_arinc.h>
#include <core/delayed_event.h>
#include <core/thread.h>
#include "thread_internal.h"

#include <cswitch.h>
#include <core/space.h>
#include <asp/arch.h>
#include <core/syscall.h>
#include <core/uaccess.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the jet_user_to_kernel*()
 * helpers return the kernel address of user memory as a pointer to void,
 * which is converted to the type of the object the user passed.
 * MISRA C:2025 Rule 18.4 deviation (this file): thread and object IDs are
 * the index of an element, computed as the difference between its address
 * and the start of its array, and a thread is found from its ID the same
 * way.
 */

/**
 * @funcname `thread_start_func`
 *
 * @description
 * The first code of a thread in the kernel: jumps to the thread's entry point
 * in user mode, with its stack.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread_current` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `thread_current` to `current_thread`.
 * 2. Call `pok_partition_jump_user()` with the arguments
 *    `thread_current->entry`, `thread_current->init_stack_addr` and
 *    `thread_current->initial_sp`.
 *
 * @calls
 * - `pok_partition_jump_user()`
 */
static void thread_start_func(void)
{
    pok_thread_t* thread_current = current_thread;

    pok_partition_jump_user(
        thread_current->entry,
        thread_current->init_stack_addr,
        thread_current->initial_sp);
}

/**
 * @funcname `sched_arinc_start`
 *
 * @description
 * The start operation of an ARINC 653 partition: runs its main thread, with the
 * partition locked (initialization).
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
 * | `thread_main` | `pok_thread_t *` | The main thread |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `part->idle_sp` to `NULL`.
 * 3. Set `thread_main` to the address of
 *    `part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID]`.
 * 4. Set `thread_main->entry_sp_user` to `NULL`.
 * 5. Set `part->lock_level` to `1`.
 * 6. Set `part->thread_locked` to `thread_main`.
 * 7. Set `thread_main->state` to `POK_STATE_RUNNABLE`.
 * 8. Set `part->thread_current` to `thread_main`.
 * 9. Set `part->kshd->current_thread_id` to
 *    `POK_PARTITION_ARINC_MAIN_THREAD_ID`.
 * 10. Set `part->base_part.fp_store_current` to `thread_main->fp_store`.
 * 11. Call `ja_context_restart_and_save()` with the arguments
 *     `thread_main->initial_sp`, the address of `thread_start_func` and the
 *     address of `thread_main->sp`.
 *
 * @calls
 * - `ja_context_restart_and_save()`
 */
void sched_arinc_start(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    part->idle_sp = NULL;

    pok_thread_t* thread_main
        = &part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID];
#ifdef POK_NEEDS_GDB
    thread_main->entry_sp_user = NULL;
#endif
    part->lock_level = 1;
	part->thread_locked = thread_main;

	thread_main->state = POK_STATE_RUNNABLE;
    part->thread_current = thread_main;
    // Update kernel shared data.
    part->kshd->current_thread_id = POK_PARTITION_ARINC_MAIN_THREAD_ID;

	// Direct jump into main thread.
    part->base_part.fp_store_current = thread_main->fp_store;

	jet_context_restart_and_save(thread_main->initial_sp,
        &thread_start_func, &thread_main->sp);
}


/**
 * @funcname `port_queuing_fired`
 *
 * @description
 * Handles an event of a queuing port: wakes up the threads waiting on it while
 * there are messages to receive (IN port) or room to send (OUT port).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port_queuing` | `pok_port_queuing_t *` | [in] The port. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message_size` | `pok_message_size_t` | The size of a message (not used) |
 * | `t` | `pok_thread_t *` | The thread woken up |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `port_queuing->direction` is equal to `POK_PORT_DIRECTION_IN`:
 *    1. While `pok_thread_wq_is_empty(&port_queuing->waiters)` is false:
 *       1. If `(pok_channel_queuing_r_get_message(port_queuing->channel,`
 *          `&message_size, TRUE))` is NULL:
 *          1. Leave the loop.
 *       2. Call `pok_thread_wq_wake_up()` with the address of
 *          `port_queuing->waiters`, and store the result in `t`.
 *       3. Call `port_queuing_receive()` with the arguments `port_queuing` and
 *          `t`.
 * 2. Otherwise:
 *    1. While `pok_thread_wq_is_empty(&port_queuing->waiters)` is false:
 *       1. If `pok_channel_queuing_s_get_message(port_queuing->channel, TRUE)`
 *          is NULL:
 *          1. Leave the loop.
 *       2. Call `pok_thread_wq_wake_up()` with the address of
 *          `port_queuing->waiters`, and store the result in `t`.
 *       3. Call `port_queuing_send()` with the arguments `port_queuing` and
 *          `t`.
 *
 * @calls
 * - `pok_thread_wq_is_empty()`
 * - `pok_channel_queuing_r_get_message()`
 * - `pok_thread_wq_wake_up()`
 * - `port_queuing_receive()`
 * - `pok_channel_queuing_s_get_message()`
 * - `port_queuing_send()`
 */
static void port_queuing_fired(pok_port_queuing_t* port_queuing)
{
    if(port_queuing->direction == POK_PORT_DIRECTION_IN)
    {
        while(!pok_thread_wq_is_empty(&port_queuing->waiters))
        {
            pok_thread_t* t;
            pok_message_size_t message_size; // Just for function's call.

            if((pok_channel_queuing_r_get_message(port_queuing->channel,
                &message_size, TRUE)) == NULL) {
                break; // wait again
            }

            t = pok_thread_wq_wake_up(&port_queuing->waiters);

            port_queuing_receive(port_queuing, t);
        }
    }
    else // if(port_queuing->direction == POK_PORT_DIRECTION_OUT)
    {
        while(!pok_thread_wq_is_empty(&port_queuing->waiters))
        {
            pok_thread_t* t;

            if(pok_channel_queuing_s_get_message(port_queuing->channel,
                                                 TRUE) == NULL) {
                break; // wait again
            }

            t = pok_thread_wq_wake_up(&port_queuing->waiters);

            port_queuing_send(port_queuing, t);
        }
    }
}

/**
 * @funcname `do_nothing_func`
 *
 * @description
 * The context of a partition when none of its threads can run: enables local
 * preemption and loops.
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
 * 1. Call `pok_preemption_local_enable()`.
 * 2. Call `ja_inf_loop()`.
 *
 * @calls
 * - `pok_preemption_local_enable()`
 * - `ja_inf_loop()`
 */
static void do_nothing_func(void)
{
    pok_preemption_local_enable();

    ja_inf_loop();
}

/**
 * @funcname `select_thread`
 *
 * @description
 * Selects a thread to run, and finds the thread that really runs: the one a
 * stopping thread waits for, or the owner of an msection that the thread is
 * entering.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `selected_thread` | `pok_thread_t *` | [in,out] The thread chosen, or NULL. |
 *
 * @return The thread to run, or NULL for none.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n_attempts` | `int32_t` | The number of owners followed at most |
 * | `new_thread` | `pok_thread_t *` | The thread to run |
 * | `owner` | `pok_thread_id_t` | The owner of the msection |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `waiting_section` | `msection_t *` | The msection being entered |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `part->thread_selected` to `selected_thread`.
 * 3. If `part->waiting_section` is not NULL:
 *    1. Set `part->waiting_section->msection_kernel_flags` to `0`.
 *    2. Set `part->waiting_section` to `NULL`.
 * 4. If `selected_thread` is NULL:
 *    1. Return `NULL`.
 * 5. Set `new_thread` to `selected_thread`.
 * 6. If `new_thread->relations_stop.donate_target` is not NULL:
 *    1. Set `new_thread` to `new_thread->relations_stop.donate_target`.
 * 7. Set `waiting_section` to `new_thread->msection_entering`.
 * 8. If `waiting_section` is not NULL:
 *    1. Set `n_attempts` to `5`.
 *    2. Repeat the following while `waiting_section` is not NULL (at least
 *       once):
 *       1. Set `owner` to `waiting_section->owner`.
 *       2. If `owner` is equal to `JET_THREAD_ID_NONE` or `owner` is equal to
 *          `(new_thread - part->threads)`:
 *          1. Set `waiting_section->owner` to `(new_thread - part->threads)`.
 *          2. Set `new_thread->msection_entering` to `NULL`.
 *          3. Leave the loop.
 *       3. Check that `owner < part->nthreads_used` holds (assert_os: stop
 *          otherwise).
 *       4. Check that `owner > POK_PARTITION_ARINC_MAIN_THREAD_ID` holds
 *          (assert_os: stop otherwise).
 *       5. Set `new_thread` to `part->threads + owner`.
 *       6. Check that `new_thread->state == POK_STATE_RUNNABLE` holds
 *          (assert_os: stop otherwise).
 *       7. Set `part->waiting_section` to `waiting_section`.
 *       8. Set `waiting_section` to `new_thread->msection_entering`.
 *       9. Check that `n_attempts != 0` holds (assert: stop otherwise).
 *       10. Decrement `n_attempts`.
 * 9. If `part->waiting_section` is not NULL:
 *    1. Set `part->waiting_section->msection_kernel_flags` to
 *       `MSECTION_KERNEL_FLAG_RESCHED_AFTER_LEAVE`.
 * 10. Return `new_thread`.
 *
 * @calls
 * - `pok_assertion_fail()`
 */
static pok_thread_t* select_thread(pok_thread_t* selected_thread)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    part->thread_selected = selected_thread;
    if(part->waiting_section != NULL)
    {
        // Clear waiting section for possibly set it after.
        part->waiting_section->msection_kernel_flags = 0;
        part->waiting_section = NULL;
    }

    if(selected_thread == NULL) {
        return NULL;
    }

    pok_thread_t* new_thread = selected_thread;

    if(new_thread->relations_stop.donate_target != NULL)
    {
        /* We should wait for other thread to stop. */
        new_thread = new_thread->relations_stop.donate_target;
    }

    // Check that given thread is able to continue.
    msection_t* waiting_section = new_thread->msection_entering;
    if(waiting_section != NULL)
    {
        /* Limit number of attempts to determine current thread. TODO: this should be configurable constant. */
        int32_t n_attempts = 5;

        do {
            pok_thread_id_t owner = waiting_section->owner;
            if((owner == JET_THREAD_ID_NONE)
               || (owner == (new_thread - part->threads)))
            {
                waiting_section->owner = (new_thread - part->threads);
                new_thread->msection_entering = NULL;
                break;
            }

            assert_os(owner < part->nthreads_used);
            // Contention on msection is possible only in NORMAL mode, when main thread is not executed.
            assert_os(owner > POK_PARTITION_ARINC_MAIN_THREAD_ID);

            new_thread = part->threads + owner;
            assert_os(new_thread->state == POK_STATE_RUNNABLE);
            // TODO: other consistency checks
            part->waiting_section = waiting_section;

            waiting_section = new_thread->msection_entering;

            assert(n_attempts != 0);
            n_attempts--;

        } while(waiting_section != NULL);
    }

    if(part->waiting_section != NULL)
    {
        part->waiting_section->msection_kernel_flags = MSECTION_KERNEL_FLAG_RESCHED_AFTER_LEAVE;
    }

    return new_thread;
}

/**
 * @funcname `sched_arinc`
 *
 * @description
 * The scheduler of the current partition: handles its events and timers, then,
 * if a recheck was asked for, chooses the thread to run and switches to it.
 * Called with local preemption disabled.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `jet_partition_event_t` | An event of the partition |
 * | `new_sp` | `jet_context_t *` | The context to run |
 * | `new_thread` | `pok_thread_t *` | The thread to run |
 * | `old_sp` | `jet_context_t **` | Where the old context is saved |
 * | `old_thread` | `pok_thread_t *` | The thread that ran |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `recheck` | `pok_bool_t` | Whether the events must be handled again |
 * | `time_now` | `pok_time_t` | The current time |
 * | `timer_new` | `pok_time_t` | The time of the next delayed event |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Repeat the following while `recheck` is true (at least once):
 *    1. Set `recheck` to `FALSE`.
 *    2. If `flag_test_and_reset(&part->base_part.is_event)` is true:
 *       1. While `pok_partition_get_event(&event)` is true:
 *          1. Depending on `event.event_type`:
 *             1. Case `JET_PARTITION_EVENT_TYPE_TIMER`:
 *                1. Call `delayed_event_queue_check()` with the arguments the
 *                   address of `part->partition_delayed_events` and
 *                   `jet_system_time()`.
 *                2. Leave the switch.
 *             2. Case `JET_PARTITION_EVENT_TYPE_PORT_SEND_AVAILABLE` or
 *                `JET_PARTITION_EVENT_TYPE_PORT_RECEIVE_AVAILABLE`:
 *                1. Call `port_queuing_fired()` with the address of
 *                   `part->ports_queuing[event.handler_id]`.
 *                2. Leave the switch.
 *             3. Otherwise (default):
 *                1. Use the macro `unreachable()`.
 *    3. Call `delayed_event_queue_get_check_time()` with the address of
 *       `part->partition_delayed_events`, and store the result in `timer_new`.
 *    4. Call `pok_partition_set_timer()` with the arguments the address of
 *       `part->base_part` and `timer_new`.
 *    5. If `timer_new` is not equal to `0`:
 *       1. Set `time_now` to `jet_system_time()`.
 *       2. If `time_now` is greater than or equal to `timer_new`:
 *          1. Call `delayed_event_queue_check()` with the arguments the address
 *             of `part->partition_delayed_events` and `time_now`.
 *          2. Set `recheck` to `TRUE`.
 * 3. If `flag_test_and_reset(&part->sched_local_recheck_needed)` is false:
 *    1. Return.
 * 4. Set `part->base_part.is_error_handler` to `FALSE`.
 * 5. Set `old_thread` to `part->thread_current`.
 * 6. Check that `part->mode != POK_PARTITION_MODE_IDLE` holds (assert: stop
 *    otherwise).
 * 7. If `part->mode` is not equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Set `new_thread` to the address of
 *       `part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID]`.
 *    2. If `new_thread->state` is equal to `POK_STATE_STOPPED`:
 *       1. Set `new_thread` to `NULL`.
 * 8. Otherwise, if `part->thread_error` is not NULL and
 *    `part->thread_error->state` is not equal to `POK_STATE_STOPPED`:
 *    1. Set `new_thread` to `part->thread_error`.
 *    2. Set `part->base_part.is_error_handler` to `TRUE`.
 * 9. Otherwise, if `list_empty(&part->error_list)` is false:
 *    1. Set `new_thread` to `part->thread_error`.
 *    2. Set `new_thread->priority` to `new_thread->base_priority`.
 *    3. Set `new_thread->sp` to `NULL`.
 *    4. Set `new_thread->state` to `POK_STATE_RUNNABLE`.
 *    5. Set `part->base_part.is_error_handler` to `TRUE`.
 * 10. Otherwise, if `part->lock_level` is not equal to `0U`:
 *     1. Set `new_thread` to `part->thread_locked`.
 * 11. Otherwise, if `list_empty(&part->eligible_threads)` is false:
 *     1. Set `new_thread` to
 *        `list_first_entry(&part->eligible_threads, pok_thread_t,`
 *        `eligible_elem)`.
 * 12. Otherwise:
 *     1. Set `new_thread` to `NULL`.
 * 13. Call `select_thread()` with the argument `new_thread`, and store the
 *     result in `new_thread`.
 * 14. If `new_thread` is equal to `old_thread`:
 *     1. If `new_thread` is NULL:
 *        1. Return.
 *     2. If `new_thread->sp` is not NULL:
 *        1. Return.
 *     3. Check that `new_thread == part->thread_error` holds (assert: stop
 *        otherwise).
 *     4. Set `new_thread->entry_sp_user` to `NULL`.
 *     5. Call `ja_context_restart_and_save()` with the arguments
 *        `new_thread->initial_sp`, the address of `thread_start_func` and the
 *        address of `new_thread->sp`.
 * 15. Set `part->thread_current` to `new_thread`.
 * 16. If `new_thread` is not NULL:
 *     1. Set `part->kshd->current_thread_id` to `new_thread - part->threads`.
 * 17. Set `old_sp` to the address of `old_thread->sp` if `old_thread` is not
 *     NULL, otherwise the address of `part->idle_sp`.
 * 18. If `new_thread` is not NULL:
 *     1. Set `new_sp` to `new_thread->sp`.
 *     2. If `new_sp` is NULL:
 *        1. Call `ja_context_init()` with the arguments
 *           `new_thread->initial_sp` and the address of `thread_start_func`,
 *           and store the result in `new_sp`.
 *        2. Set `new_thread->sp` to `new_sp`.
 * 19. Otherwise:
 *     1. Set `new_sp` to `part->idle_sp`.
 *     2. If `new_sp` is NULL:
 *        1. Call `ja_context_init()` with the arguments
 *           `part->base_part.initial_sp` and the address of `do_nothing_func`,
 *           and store the result in `new_sp`.
 *        2. Set `part->idle_sp` to `new_sp`.
 * 20. If the value at `old_sp` is NULL:
 *     1. Set `old_sp` to `NULL`.
 * 21. If `old_thread` is not NULL and `part->base_part.entry_sp_user` is not
 *     NULL:
 *     1. Set `old_thread->entry_sp_user` to `part->base_part.entry_sp_user`.
 *     2. Set `part->base_part.entry_sp_user` to `NULL`.
 * 22. If `new_thread` is not NULL:
 *     1. Set `part->base_part.fp_store_current` to `new_thread->fp_store`.
 * 23. If `old_sp` is not NULL:
 *     1. Call `ja_context_switch()` with the arguments `old_sp` and `new_sp`.
 * 24. Otherwise:
 *     1. Call `ja_context_jump()` with the argument `new_sp`.
 *
 * @calls
 * - `flag_test_and_reset()`
 * - `pok_partition_get_event()`
 * - `delayed_event_queue_check()`
 * - `ja_system_time()`
 * - `port_queuing_fired()`
 * - `pok_assertion_fail()`
 * - `delayed_event_queue_get_check_time()`
 * - `pok_partition_set_timer()`
 * - `list_empty()`
 * - `select_thread()`
 * - `ja_context_restart_and_save()`
 * - `ja_context_init()`
 * - `ja_context_switch()`
 * - `ja_context_jump()`
 */
static void sched_arinc(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_bool_t recheck;

    do {
        recheck = FALSE;
        if(flag_test_and_reset(&part->base_part.is_event))
        {
            jet_partition_event_t event;

            while(pok_partition_get_event(&event))
            {
                switch(event.event_type) {
                    case JET_PARTITION_EVENT_TYPE_TIMER:
                        delayed_event_queue_check(
                            &part->partition_delayed_events, jet_system_time());
                        break;
                    case JET_PARTITION_EVENT_TYPE_PORT_SEND_AVAILABLE:
                    case JET_PARTITION_EVENT_TYPE_PORT_RECEIVE_AVAILABLE:
                        port_queuing_fired(
                            &part->ports_queuing[event.handler_id]);
                        break;
                    default:
                        unreachable();
                }
            }
        }

        // Update timer.
        pok_time_t timer_new = delayed_event_queue_get_check_time(
            &part->partition_delayed_events);

        pok_partition_set_timer(&part->base_part, timer_new);

        if(timer_new != 0) {
            /* 
             * Check wether timer is not expired before we set it.
             * 
             * Such a way we eliminate consiquenses of possible race between
             * setting the timer and checking it in the global scheduler.
             */
            pok_time_t time_now = jet_system_time();
            if(time_now >= timer_new) {
                // Recheck delayed events.
                delayed_event_queue_check(&part->partition_delayed_events,
                                          time_now);
                // Need to redrain events before setting timer again.
                recheck = TRUE;
            }
        }
    } while(recheck);

    if(!flag_test_and_reset(&part->sched_local_recheck_needed)) {
        return;
    }

    part->base_part.is_error_handler = FALSE; // Will be set if needed.

    pok_thread_t* old_thread = part->thread_current;
    pok_thread_t* new_thread;

    assert(part->mode != POK_PARTITION_MODE_IDLE); // Idle mode is implemented via function with preemption disabled.

    if(part->mode != POK_PARTITION_MODE_NORMAL)
    {
        new_thread = &part->threads[POK_PARTITION_ARINC_MAIN_THREAD_ID];
        if(new_thread->state == POK_STATE_STOPPED)
        {
            /*
             * Stopped main thread means that partition can do nothing.
             * 
             * But ARINC doesn't specify this state as IDLE.
             * 
             * So we treat this case as general "none of the threads are ready now".
             */
            new_thread = NULL;
        }
    }
#ifdef POK_NEEDS_ERROR_HANDLING
    else if((part->thread_error != NULL)
            && (part->thread_error->state != POK_STATE_STOPPED))
    {
        // Continue error handler
        new_thread = part->thread_error;
        part->base_part.is_error_handler = TRUE;
    }
    else if(!list_empty(&part->error_list))
    {
        // Start error handler (aperiodic, no timeout).
        new_thread = part->thread_error;

        new_thread->priority = new_thread->base_priority;
        new_thread->sp = NULL;

        new_thread->state = POK_STATE_RUNNABLE;

        part->base_part.is_error_handler = TRUE;
    }
#endif
    else if(part->lock_level != 0U)
    {
        new_thread = part->thread_locked;
    }
    else if(!list_empty(&part->eligible_threads))
    {
        new_thread = list_first_entry(&part->eligible_threads,
            pok_thread_t, eligible_elem);
    }
    else
    {
        new_thread = NULL;
    }

    new_thread = select_thread(new_thread);

    if(new_thread == old_thread)
    {
        if(new_thread == NULL) {
            return; // "do_nothing" continues
        }

        if(new_thread->sp != NULL) {
            return; // Thread continues its execution.
        }

        /* 
         * None common thread can restart itself: someone *other* should
         * call START function for it.
         * 
         * The only exception is error handler: If it calls STOP and
         * error list is not empty, error handler is automatically restarted.
         */
        assert(new_thread == part->thread_error);

#ifdef POK_NEEDS_GDB
        new_thread->entry_sp_user = NULL;
#endif
        jet_context_restart_and_save(new_thread->initial_sp,
            &thread_start_func, &new_thread->sp);
    }

    // Switch between different threads
    part->thread_current = new_thread;
    // Update kernel shared data
    if(new_thread != NULL) {
        part->kshd->current_thread_id = new_thread - part->threads;
    }

    jet_context_t** old_sp = (old_thread != NULL)? &old_thread->sp
        : &part->idle_sp;
    jet_context_t* new_sp;

    if(new_thread != NULL)
    {
        new_sp = new_thread->sp;
        if(new_sp == NULL)
        {
            new_sp = jet_context_init(new_thread->initial_sp,
                &thread_start_func);
            new_thread->sp = new_sp;
        }
    }
    else
    {
        // New thread is "do_nothing"
        new_sp = part->idle_sp;
        if(new_sp == NULL)
        {
            new_sp = jet_context_init(part->base_part.initial_sp,
                &do_nothing_func);
            part->idle_sp = new_sp;
        }
    }

    /*
     * If old_thread->sp is 0, this should be processed upon
     * returning to given thread, not now.
     */
    if(*old_sp == NULL) {
        old_sp = NULL;
    }

#ifdef POK_NEEDS_GDB
    if((old_thread != NULL) && (part->base_part.entry_sp_user != NULL))
    {
        /*
         * Copy address of frame with user space addresses locally
         * and clear original.
         */
        old_thread->entry_sp_user = part->base_part.entry_sp_user;
        part->base_part.entry_sp_user = NULL;
    }
#endif /* POK_NEEDS_GDB */
    // The idle context has no FP store: the last thread's stays current.
    if(new_thread != NULL)
    {
        part->base_part.fp_store_current = new_thread->fp_store;
    }

    if(old_sp != NULL)
    {
        jet_context_switch(old_sp, new_sp);
    }
    else
    {
        jet_context_jump(new_sp);
    }
}

/**
 * @funcname `pok_preemption_local_disable`
 *
 * @description
 * Disables local preemption in the current partition.
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
 * 1. Check that `!current_partition_arinc->base_part.preempt_local_disabled`
 *    holds (assert: stop otherwise).
 * 2. Use the macro `flag_set()` with the argument
 *    `current_partition_arinc->base_part.preempt_local_disabled`.
 *
 * @calls
 * - `pok_assertion_fail()`
 */
void pok_preemption_local_disable(void)
{
    assert(!current_partition_arinc->base_part.preempt_local_disabled);

    flag_set(current_partition_arinc->base_part.preempt_local_disabled);
}

/**
 * @funcname `pok_preemption_local_enable`
 *
 * @description
 * Enables local preemption in the current partition, running its scheduler
 * first and again for each event that comes meanwhile.
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
 * 2. Check that `part->base_part.preempt_local_disabled` holds (assert: stop
 *    otherwise).
 * 3. Call `sched_arinc()`.
 * 4. Execute the assembly code `barrier()`.
 * 5. Use the macro `flag_reset()` with the argument
 *    `part->base_part.preempt_local_disabled`.
 * 6. While `ACCESS_ONCE(part->base_part.is_event)` holds:
 *    1. Use the macro `flag_set()` with the argument
 *       `part->base_part.preempt_local_disabled`.
 *    2. Call `sched_arinc()`.
 *    3. Execute the assembly code `barrier()`.
 *    4. Use the macro `flag_reset()` with the argument
 *       `current_partition_arinc->base_part.preempt_local_disabled`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `sched_arinc()`
 */
void pok_preemption_local_enable(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    assert(part->base_part.preempt_local_disabled);

    sched_arinc();

    barrier();

    flag_reset(part->base_part.preempt_local_disabled);

    // Check that we do not miss events since sched_arinc() starts.

    while(ACCESS_ONCE(part->base_part.is_event))
    {
        flag_set(part->base_part.preempt_local_disabled);

        sched_arinc();

        barrier();

        flag_reset(current_partition_arinc->base_part.preempt_local_disabled);
    }
}

/**
 * @funcname `pok_sched_arinc_on_event`
 *
 * @description
 * The on_event operation of an ARINC 653 partition: takes the msection the
 * current thread is entering from the shared data, and runs the scheduler.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msection_entering` | `msection_t *` | The kernel address of the msection it enters |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `thread_current` | `pok_thread_t *` | The current thread |
 * | `tshd_current` | `jet_thread_shared_data_t *` | Its shared data |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `thread_current` to `part->thread_current`.
 * 3. If `thread_current` is not NULL:
 *    1. Set `tshd_current` to
 *       `part->kshd->tshd + (thread_current - part->threads)`.
 *    2. If `tshd_current->msection_entering` is not NULL:
 *       1. Set `msection_entering` to
 *          `jet_user_to_kernel_typed(tshd_current->msection_entering)`.
 *       2. Check that `msection_entering != NULL` holds (assert_os: stop
 *          otherwise).
 *       3. Set `thread_current->msection_entering` to `msection_entering`.
 * 4. Call `sched_arinc()`.
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `pok_assertion_fail()`
 * - `sched_arinc()`
 */
void pok_sched_arinc_on_event(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;
    pok_thread_t* thread_current = part->thread_current;

    if(thread_current != NULL)
    {
        //Update information according to current kernel shared data.
        jet_thread_shared_data_t* tshd_current = part->kshd->tshd
            + (thread_current - part->threads);

        if(tshd_current->msection_entering != NULL)
        {
            msection_t * JET_KUSER msection_entering =
                jet_user_to_kernel_typed(tshd_current->msection_entering);

            assert_os(msection_entering != NULL);

            thread_current->msection_entering = msection_entering;
        }
    }
    sched_arinc();
}

/**
 * @funcname `pok_sched_local_invalidate`
 *
 * @description
 * Asks the current partition's scheduler to choose the thread again.
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
 * 1. Use the macro `flag_set()` with the argument
 *    `current_partition_arinc->sched_local_recheck_needed`.
 *
 * @calls
 * None.
 */
void pok_sched_local_invalidate(void)
{
    flag_set(current_partition_arinc->sched_local_recheck_needed);
}

/**
 * @funcname `jet_resched`
 *
 * @description
 * The reschedule syscall: lets the partition choose the thread again after a
 * change in user space (an msection left), and finishes stopping a thread that
 * waited to leave its msections.
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
 * | `donator` | `pok_thread_t *` | A thread that waits for the current one to stop |
 * | `next_donator` | `pok_thread_t *` | The next such thread |
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `thread_current` | `pok_thread_t *` | The current thread |
 * | `tshd_current` | `jet_thread_shared_data_t *` | Its shared data |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. Set `thread_current` to `part->thread_current`.
 * 3. Set `tshd_current` to
 *    `part->kshd->tshd + (thread_current - part->threads)`.
 * 4. Call `pok_preemption_local_disable()`.
 * 5. If `part->waiting_section` is not NULL and `part->waiting_section->owner`
 *    is equal to `JET_THREAD_ID_NONE`:
 *    1. Call `pok_sched_local_invalidate()`.
 * 6. If `thread_current->relations_stop.first_donator` is not NULL and
 *    `tshd_current->msection_count` is equal to `0U`:
 *    1. Set `donator` to `thread_current->relations_stop.first_donator`.
 *    2. Set `thread_current->relations_stop.first_donator` to `NULL`.
 *    3. Call `thread_stop()` with the argument `thread_current`.
 *    4. Repeat the following while `donator` is not NULL (at least once):
 *       1. Set `donator->relations_stop.donate_target` to `NULL`.
 *       2. If `donator` is equal to `part->thread_selected`:
 *          1. Call `pok_sched_local_invalidate()`.
 *       3. Set `next_donator` to `donator->relations_stop.next_donator`.
 *       4. If `donator->relations_stop.first_donator` is not NULL:
 *          1. Set `donator->relations_stop.first_donator` to `NULL`.
 *          2. Call `thread_stop()` with the argument `donator`.
 *       5. Set `donator->relations_stop.next_donator` to `NULL`.
 *       6. Set `donator` to `next_donator`.
 * 7. Call `pok_preemption_local_enable()`.
 * 8. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `pok_preemption_local_disable()`
 * - `pok_sched_local_invalidate()`
 * - `thread_stop()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e jet_resched(void)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_thread_t* thread_current = part->thread_current;

    jet_thread_shared_data_t* tshd_current = part->kshd->tshd
        + (thread_current - part->threads);

    pok_preemption_local_disable();

    if((part->waiting_section != NULL)
       && (part->waiting_section->owner == JET_THREAD_ID_NONE)) {
        pok_sched_local_invalidate(); // msecation we await for has been released.
    }

    if((thread_current->relations_stop.first_donator != NULL)
        && (tshd_current->msection_count == 0U))
    {
        // It is safe to terminate thread now and resume all its donators.
        pok_thread_t* donator = thread_current->relations_stop.first_donator;

        thread_current->relations_stop.first_donator = NULL;
        thread_stop(thread_current);

        // Resume donators.
        do {
            pok_thread_t* next_donator;

            donator->relations_stop.donate_target = NULL;
            if(donator == part->thread_selected) {
                pok_sched_local_invalidate();
            }

            next_donator = donator->relations_stop.next_donator;

            if(donator->relations_stop.first_donator != NULL)
            {
                donator->relations_stop.first_donator = NULL;
                thread_stop(donator); // Donator itself needs to be terminated.
            }
            donator->relations_stop.next_donator = NULL;
            donator = next_donator;
        } while(donator != NULL);
    }

    pok_preemption_local_enable();

    return POK_ERRNO_OK;
}
