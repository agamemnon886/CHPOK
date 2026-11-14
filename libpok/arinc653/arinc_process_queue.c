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
 * all if/else/loop bodies; operands of the same essential type.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: msection_wq_t is used by its
 * typedef name.
 */

/**
 * @file
 * Wait queues of ARINC 653 processes for the intra-partition objects: the
 * current process is added in FIFO or priority order.
 */

#include "arinc_process_queue.h"
#include <arinc653/process.h>
#include <kernel_shared_data.h>

/**
 * @funcname `arinc_process_queue_add`
 *
 * @description
 * Adds the current process to the end of a wait queue.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `msection_wq_t *` | [in] The wait queue. |
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
 * 1. Call `msection_wq_add()` with the arguments `wq` and `JET_THREAD_ID_NONE`.
 *
 * @calls
 * - `msection_wq_add()`
 */
void arinc_process_queue_add(msection_wq_t* wq)
{
    msection_wq_add(wq, JET_THREAD_ID_NONE);
}

/**
 * @funcname `arinc_process_queue_add_common`
 *
 * @description
 * Adds the current process to a wait queue according to a discipline: FIFO, or
 * by priority (after the processes of the same or higher priority).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `wq` | `msection_wq_t *` | [in] The wait queue. |
 * | `discipline` | `QUEUING_DISCIPLINE_TYPE` | [in] FIFO or PRIORITY. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `prev` | `pok_thread_id_t` | The process after which it is added |
 * | `priority` | `PRIORITY_TYPE` | The current process's priority |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `discipline` is equal to `PRIORITY`:
 *    1. Set `priority` to `kshd.tshd[kshd.current_thread_id].priority`.
 *    2. Set `prev` to `wq->last`.
 *    3. While `prev` is not equal to `JET_THREAD_ID_NONE`:
 *       1. If `(PRIORITY_TYPE)kshd.tshd[prev].wq_priority` is greater than or
 *          equal to `priority`:
 *          1. Leave the loop.
 *       2. Set `prev` to `kshd.tshd[prev].wq_prev`.
 *    4. Call `msection_wq_add_after()` with the arguments `wq` and `prev`.
 *    5. Set `kshd.tshd[kshd.current_thread_id].wq_priority` to `priority`.
 * 2. Otherwise:
 *    1. Call `msection_wq_add()` with the arguments `wq` and
 *       `JET_THREAD_ID_NONE`.
 *
 * @calls
 * - `msection_wq_add_after()`
 * - `msection_wq_add()`
 */
void arinc_process_queue_add_common(msection_wq_t* wq,
    QUEUING_DISCIPLINE_TYPE discipline)
{
    if(discipline == PRIORITY)
    {
        PRIORITY_TYPE priority = kshd.tshd[kshd.current_thread_id].priority;
        
        pok_thread_id_t prev = wq->last;
        
        while(prev != JET_THREAD_ID_NONE)
        {
            if((PRIORITY_TYPE)kshd.tshd[prev].wq_priority >= priority) {
                break;
            }
            
            prev = kshd.tshd[prev].wq_prev;
        }
        
        msection_wq_add_after(wq, prev);
        
        kshd.tshd[kshd.current_thread_id].wq_priority = priority;
    }
    else
    {
        msection_wq_add(wq, JET_THREAD_ID_NONE);
    }
}
