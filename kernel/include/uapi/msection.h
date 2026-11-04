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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 */

#ifndef JET_UAPI_MSECTION_H
#define JET_UAPI_MSECTION_H

#include <types.h>

/* 
 * Mutual section.
 * 
 * Provide critical section, when high-priority thread doesn't go into
 * waiting state when try to enter into busy section.
 * 
 * Instead, owner of the section is executed until it leaves the section.
 * Then, high-priority thread continues it execution.
 */

struct msection
{
    /* Owner thread or JET_THREAD_ID_NONE. */
    pok_thread_id_t owner;

    /* Flags which are set by the kernel and are checked by the user. */
    uint8_t msection_kernel_flags;
};

/* When leaving the section, need to call jet_resched(). */
#define MSECTION_KERNEL_FLAG_RESCHED_AFTER_LEAVE 1U

/* 
 * Queue of waiting processes protected by waitqueue.
 * 
 * Linkage of elements is within 'struct jet_thread_shared_data'.
 */
struct msection_wq
{
    // The first thread in the queue. JET_THREAD_ID_NONE if queue is empty.
    pok_thread_id_t first;
    // The last thread in the queue. JET_THREAD_ID_NONE if queue is empty.
    pok_thread_id_t last;
};

#endif /* JET_UAPI_MSECTION_H */
