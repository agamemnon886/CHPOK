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

void msection_init(struct msection* section)
{
    section->owner = JET_THREAD_ID_NONE;
    section->msection_kernel_flags = 0;
}

void msection_enter(struct msection* section)
{
    struct jet_thread_shared_data* tshd_current = kshd.tshd + kshd.current_thread_id;

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

void msection_leave(struct msection* section)
{
    struct jet_thread_shared_data* tshd_current = kshd.tshd + kshd.current_thread_id;
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

pok_ret_t msection_wait(struct msection* section, pok_time_t timeout)
{
    return jet_msection_wait(section, &timeout);
}

pok_ret_t msection_notify(struct msection* section, pok_thread_id_t thread_id)
{
    return jet_msection_notify(section, thread_id);
}

void msection_wq_add(struct msection_wq* wq, pok_thread_id_t next)
{
    pok_thread_id_t *pnext = (next != JET_THREAD_ID_NONE)
        ? &kshd.tshd[next].wq_prev
        : &wq->last;

    pok_thread_id_t prev = *pnext;

    pok_thread_id_t* pprev = (prev != JET_THREAD_ID_NONE)
        ? &kshd.tshd[prev].wq_next
        : &wq->first;

    struct jet_thread_shared_data* tshd_current = &kshd.tshd[kshd.current_thread_id];

    tshd_current->wq_next = next;
    tshd_current->wq_prev = prev;

    *pprev = kshd.current_thread_id;
    *pnext = kshd.current_thread_id;
}

void msection_wq_add_after(struct msection_wq* wq, pok_thread_id_t prev)
{
    pok_thread_id_t* pprev = (prev != JET_THREAD_ID_NONE)
        ? &kshd.tshd[prev].wq_next
        : &wq->first;

    pok_thread_id_t next = *pprev;

    pok_thread_id_t *pnext = (next != JET_THREAD_ID_NONE)
        ? &kshd.tshd[next].wq_prev
        : &wq->last;

    struct jet_thread_shared_data* tshd_current = &kshd.tshd[kshd.current_thread_id];

    tshd_current->wq_next = next;
    tshd_current->wq_prev = prev;

    *pprev = kshd.current_thread_id;
    *pnext = kshd.current_thread_id;
}

void msection_wq_del(struct msection_wq* wq, pok_thread_id_t thread)
{
    struct jet_thread_shared_data* tshd_t = &kshd.tshd[thread];

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


pok_ret_t msection_wq_notify(struct msection* section,
    struct msection_wq* wq,
    pok_bool_t is_all)
{
    return jet_msection_wq_notify(section, wq, is_all);
}

size_t msection_wq_size(struct msection* section,
   struct msection_wq* wq)
{
    size_t size;

    if(jet_msection_wq_size(section, wq, &size) != POK_ERRNO_OK) {
        size = 0;
    }

    return size;
}
