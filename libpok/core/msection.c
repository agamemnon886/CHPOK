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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; if-else-if
 * chains end with else; msection_wq_size() returns 0 if the syscall fails
 * instead of an uninitialized value.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: increments and
 * assignments are separate statements; operands of mixed precedence are
 * parenthesized; deviations are documented where a guideline can't be
 * followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: jet_thread_shared_data_t, msection_t, msection_wq_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * msections: the critical sections of the partition's processes, entered
 * without a syscall when they are free, and their wait queues (in the
 * data shared with the kernel).
 */

#include <msection.h>
#include <core/syscall.h>
#include <kernel_shared_data.h>

#include <arinc653/types.h>
#include <utils.h>

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): the shared data of a thread
 * is found by adding its ID to the start of the array of all threads.
 */

/**
 * @funcname `msection_init`
 *
 * @description
 * Initializes an msection: free.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t *` | [out] The section. |
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
 * 1. Set `section->owner` to `JET_THREAD_ID_NONE`.
 * 2. Set `section->msection_kernel_flags` to `0`.
 *
 * @calls
 * None.
 */
void msection_init(msection_t* section)
{
    section->owner = JET_THREAD_ID_NONE;
    section->msection_kernel_flags = 0;
}

/**
 * @funcname `msection_enter`
 *
 * @description
 * Enters an msection: takes it if it is free, else waits for it (a syscall). On
 * error (incorrect state, incorrect parameter) the partition is terminated (but
 * can be restarted).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t *` | [in,out] The section. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tshd_current` | `jet_thread_shared_data_t *` | The current thread's shared data |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `tshd_current` to `kshd.tshd + kshd.current_thread_id`.
 * 2. Increment `tshd_current->msection_count`.
 * 3. Set `tshd_current->msection_entering` to `section`.
 * 4. If `section->owner` is equal to `JET_THREAD_ID_NONE`:
 *    1. Set `section->owner` to `kshd.current_thread_id`.
 * 5. Otherwise, if `section->owner` is not equal to `kshd.current_thread_id`:
 *    1. Call `jet_msection_enter_helper()` with the argument `section`.
 * 6. Otherwise:
 *    1. Do nothing.
 * 7. Set `tshd_current->msection_entering` to `NULL`.
 *
 * @calls
 * - `jet_msection_enter_helper()`
 */
void msection_enter(msection_t* section)
{
    jet_thread_shared_data_t* tshd_current = kshd.tshd + kshd.current_thread_id;

    tshd_current->msection_count++;
    tshd_current->msection_entering = section;

    if(section->owner == JET_THREAD_ID_NONE)
    {
        section->owner = kshd.current_thread_id;
    }
    else if(section->owner != kshd.current_thread_id)
    {
        (void) jet_msection_enter_helper(section); //syscall
    }
    else
    {
        // The section is already ours.
    }

    tshd_current->msection_entering = NULL;
}

/**
 * @funcname `msection_leave`
 *
 * @description
 * Leaves an msection; reschedules if the thread has been killed meanwhile (it
 * is killed now) or someone waits on the section. On error the partition is
 * terminated (but can be restarted).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t *` | [in,out] The section. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tshd_current` | `jet_thread_shared_data_t *` | The current thread's shared data |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `tshd_current` to `kshd.tshd + kshd.current_thread_id`.
 * 2. Set `section->owner` to `JET_THREAD_ID_NONE`.
 * 3. Decrement `tshd_current->msection_count`.
 * 4. If `tshd_current->msection_count` is equal to `0U` and
 *    `(tshd_current->thread_kernel_flags & THREAD_KERNEL_FLAG_KILLED)` is not
 *    equal to `0U`:
 *    1. Call `jet_resched()`.
 * 5. Otherwise, if `(section->msection_kernel_flags &`
 *    `MSECTION_KERNEL_FLAG_RESCHED_AFTER_LEAVE)` is not equal to `0U`:
 *    1. Call `jet_resched()`.
 * 6. Otherwise:
 *    1. Do nothing.
 *
 * @calls
 * - `jet_resched()`
 */
void msection_leave(msection_t* section)
{
    jet_thread_shared_data_t* tshd_current = kshd.tshd + kshd.current_thread_id;
    // TODO: assert(tshd_current->msection_count > 0);

    section->owner = JET_THREAD_ID_NONE;
    tshd_current->msection_count--;

    if((tshd_current->msection_count == 0U)
        && ((tshd_current->thread_kernel_flags & THREAD_KERNEL_FLAG_KILLED)
            != 0U))
    {
        // Let us being killed.
        (void) jet_resched();
    }
    else if((section->msection_kernel_flags
             & MSECTION_KERNEL_FLAG_RESCHED_AFTER_LEAVE) != 0U)
    {
        // Someone waits on the section.
        (void) jet_resched();
    }
    else
    {
        // Nobody waits: no reschedule is needed.
    }
}

/**
 * @funcname `msection_wait`
 *
 * @description
 * Atomically leaves an msection and waits (a syscall), up to a time-out if it
 * isn't negative. The section should be the only one which the process enters;
 * after being awoken (whatever the reason), the process enters the section
 * again.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t *` | [in,out] The section. |
 * | `timeout` | `pok_time_t` | [in] The time-out. |
 *
 * @return POK_ERRNO_OK (awoken by msection_notify()), POK_ERRNO_TIMEOUT,
 *         POK_ERRNO_CANCELLED (someone calls STOP() for it) or POK_ERRNO_MODE
 *         (waiting isn't allowed in this mode).
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
 * 1. Call `jet_msection_wait()` with the arguments `section` and the address of
 *    `timeout`, and return its result.
 *
 * @calls
 * - `jet_msection_wait()`
 */
pok_ret_e msection_wait(msection_t* section, pok_time_t timeout)
{
    return jet_msection_wait(section, &timeout);
}

/**
 * @funcname `msection_notify`
 *
 * @description
 * Awakes a thread which waits on an msection (a syscall). The current thread
 * should have entered the section, so the awoken thread may continue only after
 * it leaves the section.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t *` | [in] The section. |
 * | `thread_id` | `pok_thread_id_t` | [in] The thread. |
 *
 * @return POK_ERRNO_OK (it has been awoken), POK_ERRNO_EXISTS (it doesn't wait)
 *         or POK_ERRNO_UNAVAILABLE (thread_id is invalid).
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
 * 1. Call `jet_msection_notify()` with the arguments `section` and `thread_id`,
 *    and return its result.
 *
 * @calls
 * - `jet_msection_notify()`
 */
pok_ret_e msection_notify(msection_t* section, pok_thread_id_t thread_id)
{
    return jet_msection_notify(section, thread_id);
}

/**
 * @funcname `msection_wq_add`
 *
 * @description
 * Adds the current thread to an msection wait queue: before a thread, or at the
 * end of the queue.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `msection_wq_t *` | [in] The wait queue. |
 * | `next` | `pok_thread_id_t` | [in] The thread to add before, or JET_THREAD_ID_NONE for the end. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pnext` | `pok_thread_id_t *` | The link of the next thread (or the last one of the queue) |
 * | `pprev` | `pok_thread_id_t *` | The link of the thread before (or the first one of the queue) |
 * | `prev` | `pok_thread_id_t` | The thread before |
 * | `tshd_current` | `jet_thread_shared_data_t *` | The current thread's shared data |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `pnext` to the address of `kshd.tshd[next].wq_prev` if `next` is not
 *    equal to `JET_THREAD_ID_NONE`, otherwise the address of `wq->last`.
 * 2. Set `prev` to the value at `pnext`.
 * 3. Set `pprev` to the address of `kshd.tshd[prev].wq_next` if `prev` is not
 *    equal to `JET_THREAD_ID_NONE`, otherwise the address of `wq->first`.
 * 4. Set `tshd_current` to the address of `kshd.tshd[kshd.current_thread_id]`.
 * 5. Set `tshd_current->wq_next` to `next`.
 * 6. Set `tshd_current->wq_prev` to `prev`.
 * 7. Set `*pprev` to `kshd.current_thread_id`.
 * 8. Set `*pnext` to `kshd.current_thread_id`.
 *
 * @calls
 * None.
 */
void msection_wq_add(msection_wq_t* wq, pok_thread_id_t next)
{
    pok_thread_id_t *pnext = (next != JET_THREAD_ID_NONE)
        ? &kshd.tshd[next].wq_prev
        : &wq->last;

    pok_thread_id_t prev = *pnext;

    pok_thread_id_t* pprev = (prev != JET_THREAD_ID_NONE)
        ? &kshd.tshd[prev].wq_next
        : &wq->first;

    jet_thread_shared_data_t* tshd_current = &kshd.tshd[kshd.current_thread_id];

    tshd_current->wq_next = next;
    tshd_current->wq_prev = prev;

    *pprev = kshd.current_thread_id;
    *pnext = kshd.current_thread_id;
}

/**
 * @funcname `msection_wq_add_after`
 *
 * @description
 * Adds the current thread to an msection wait queue: after a thread, or at the
 * beginning of the queue.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `msection_wq_t *` | [in] The wait queue. |
 * | `prev` | `pok_thread_id_t` | [in] The thread to add after, or JET_THREAD_ID_NONE for the beginning. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `next` | `pok_thread_id_t` | The thread after |
 * | `pnext` | `pok_thread_id_t *` | The link of the next thread (or the last one of the queue) |
 * | `pprev` | `pok_thread_id_t *` | The link of the thread before (or the first one of the queue) |
 * | `tshd_current` | `jet_thread_shared_data_t *` | The current thread's shared data |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `pprev` to the address of `kshd.tshd[prev].wq_next` if `prev` is not
 *    equal to `JET_THREAD_ID_NONE`, otherwise the address of `wq->first`.
 * 2. Set `next` to the value at `pprev`.
 * 3. Set `pnext` to the address of `kshd.tshd[next].wq_prev` if `next` is not
 *    equal to `JET_THREAD_ID_NONE`, otherwise the address of `wq->last`.
 * 4. Set `tshd_current` to the address of `kshd.tshd[kshd.current_thread_id]`.
 * 5. Set `tshd_current->wq_next` to `next`.
 * 6. Set `tshd_current->wq_prev` to `prev`.
 * 7. Set `*pprev` to `kshd.current_thread_id`.
 * 8. Set `*pnext` to `kshd.current_thread_id`.
 *
 * @calls
 * None.
 */
void msection_wq_add_after(msection_wq_t* wq, pok_thread_id_t prev)
{
    pok_thread_id_t* pprev = (prev != JET_THREAD_ID_NONE)
        ? &kshd.tshd[prev].wq_next
        : &wq->first;

    pok_thread_id_t next = *pprev;

    pok_thread_id_t *pnext = (next != JET_THREAD_ID_NONE)
        ? &kshd.tshd[next].wq_prev
        : &wq->last;

    jet_thread_shared_data_t* tshd_current = &kshd.tshd[kshd.current_thread_id];

    tshd_current->wq_next = next;
    tshd_current->wq_prev = prev;

    *pprev = kshd.current_thread_id;
    *pnext = kshd.current_thread_id;
}

/**
 * @funcname `msection_wq_del`
 *
 * @description
 * Removes a thread from an msection wait queue, if it has been added before.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `msection_wq_t *` | [in] The wait queue. |
 * | `thread` | `pok_thread_id_t` | [in] The thread. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `next` | `pok_thread_id_t` | The thread after |
 * | `pnext` | `pok_thread_id_t *` | The link of the next thread (or the last one of the queue) |
 * | `pprev` | `pok_thread_id_t *` | The link of the thread before (or the first one of the queue) |
 * | `prev` | `pok_thread_id_t` | The thread before |
 * | `tshd_t` | `jet_thread_shared_data_t *` | The thread's shared data |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `tshd_t` to the address of `kshd.tshd[thread]`.
 * 2. Set `next` to `tshd_t->wq_next`.
 * 3. Set `prev` to `tshd_t->wq_prev`.
 * 4. Set `pnext` to the address of `kshd.tshd[next].wq_prev` if `next` is not
 *    equal to `JET_THREAD_ID_NONE`, otherwise the address of `wq->last`.
 * 5. Set `pprev` to the address of `kshd.tshd[prev].wq_next` if `prev` is not
 *    equal to `JET_THREAD_ID_NONE`, otherwise the address of `wq->first`.
 * 6. Set `*pnext` to `prev`.
 * 7. Set `*pprev` to `next`.
 * 8. Set `tshd_t->wq_prev` to `JET_THREAD_ID_NONE`.
 * 9. Set `tshd_t->wq_next` to `JET_THREAD_ID_NONE`.
 *
 * @calls
 * None.
 */
void msection_wq_del(msection_wq_t* wq, pok_thread_id_t thread)
{
    jet_thread_shared_data_t* tshd_t = &kshd.tshd[thread];

    pok_thread_id_t next = tshd_t->wq_next;
    pok_thread_id_t prev = tshd_t->wq_prev;

    pok_thread_id_t *pnext = (next != JET_THREAD_ID_NONE)
        ? &kshd.tshd[next].wq_prev
        : &wq->last;

    pok_thread_id_t *pprev = (prev != JET_THREAD_ID_NONE)
        ? &kshd.tshd[prev].wq_next
        : &wq->first;

    *pnext = prev;
    *pprev = next;

    tshd_t->wq_prev = JET_THREAD_ID_NONE;
    tshd_t->wq_next = JET_THREAD_ID_NONE;
}


/**
 * @funcname `msection_wq_notify`
 *
 * @description
 * Awakes waiting threads of a wait queue (a syscall); every thread in the queue
 * which hasn't waited is removed from it. With is_all, all waiting threads (the
 * list of the awoken threads may be iterated from user space); otherwise only
 * the first, then pointed by wq->first. Called only by the owner of the
 * section.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t *` | [in] The section. |
 * | `wq` | `msection_wq_t *` | [in,out] The wait queue. |
 * | `is_all` | `pok_bool_t` | [in] Whether to awake all. |
 *
 * @return POK_ERRNO_OK (at least one thread has been notified) or
 *         POK_ERRNO_EMPTY (no thread waits).
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
 * 1. Call `jet_msection_wq_notify()` with the arguments `section`, `wq` and
 *    `is_all`, and return its result.
 *
 * @calls
 * - `jet_msection_wq_notify()`
 */
pok_ret_e msection_wq_notify(msection_t* section,
    msection_wq_t* wq,
    pok_bool_t is_all)
{
    return jet_msection_wq_notify(section, wq, is_all);
}

/**
 * @funcname `msection_wq_size`
 *
 * @description
 * Returns the number of waiting threads of a wait queue (a syscall); every
 * thread in the queue which hasn't waited is removed from it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `section` | `msection_t *` | [in] The section. |
 * | `wq` | `msection_wq_t *` | [in,out] The wait queue. |
 *
 * @return The number (0 on an error).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | The number |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `jet_msection_wq_size(section, wq, &size)` is not equal to
 *    `POK_ERRNO_OK`:
 *    1. Set `size` to `0`.
 * 2. Return `size`.
 *
 * @calls
 * - `jet_msection_wq_size()`
 */
size_t msection_wq_size(msection_t* section,
   msection_wq_t* wq)
{
    size_t size;

    if(jet_msection_wq_size(section, wq, &size) != POK_ERRNO_OK) {
        size = 0;
    }

    return size;
}
