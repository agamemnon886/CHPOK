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
 * Modified by Kirill Brazhnikov, 2026-11-01: derived from
 * libpok/arinc653/semaphore.h for the ARINC 653 mutexes.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 */

#ifndef LIBJET_ARINC_MUTEX_H
#define LIBJET_ARINC_MUTEX_H

#include <config.h>

#ifdef POK_NEEDS_ARINC653_MUTEX
#include <arinc653/types.h>
#include <arinc653/mutex.h>
#include <msection.h>
#include <types.h>

struct arinc_mutex
{
    MUTEX_NAME_TYPE mutex_name;

    PRIORITY_TYPE priority;              /* priority ceiling */
    QUEUING_DISCIPLINE_TYPE discipline;

    pok_thread_id_t owner;               /* JET_THREAD_ID_NONE if available */
    LOCK_COUNT_TYPE lock_count;
    PRIORITY_TYPE retained_priority;     /* owner's priority before acquiring */

    struct msection_wq process_queue;
};

/* Preallocated array of mutexes. */
extern struct arinc_mutex* arinc_mutexes;

/* Initializes the mutex subsystem; called once at partition start. */
void arinc_mutex_init(void);

/*
 * SET_PRIORITY for the process running as kernel thread 'thread'.
 *
 * If the process owns a mutex, only its retained priority is changed: the
 * new priority applies when the mutex is released. Otherwise this is
 * pok_thread_set_priority(). Returns the kernel's error code.
 */
pok_ret_t arinc_mutex_set_priority(pok_thread_id_t thread, PRIORITY_TYPE priority);

#endif /* POK_NEEDS_ARINC653_MUTEX */

/*
 * Thread that holds the preemption lock (LOCK_PREEMPTION) in NORMAL mode,
 * or JET_THREAD_ID_NONE. Valid only while the lock level is positive: the
 * kernel releases the lock when that thread is stopped. Set in process.c.
 */
extern pok_thread_id_t arinc_preemption_lock_holder;

#endif /* LIBJET_ARINC_MUTEX_H */
