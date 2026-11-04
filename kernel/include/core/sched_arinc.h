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
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: the syscall jet_resched() is
 * declared here instead of only in the generated syscall map.
 */

#ifndef POK_SCHED_ARINC_H
#define POK_SCHED_ARINC_H

#include <core/sched.h>

/* 
 * Local scheduler for ARINC partition.
 */

/**
 * Set `invalidate` flag for local scheduler.
 * 
 * This flag can be checked with pok_sched_local_check_invalidated().
 */
void pok_sched_local_invalidate(void);

/**
 * Return true if data for local scheduler has changed.
 * 
 * Clear `invalidate` flag for local scheduler.
 */
pok_bool_t pok_sched_local_check_invalidated(void);


/**
 *  Return true if new time slot is started.
 * 
 * Clear corresponded flag.
 * 
 * DEV: Intended to be used only within local scheduler.
 */
pok_bool_t pok_sched_local_check_slot_started(void);


/**
 * Begins critical section, local to given ARINC partition.
 * 
 * Only preemption between current partition's threads is disabled.
 * 
 * Preemption between partitions and between threads on other partitions
 * is still enabled.
 */
void pok_preemption_local_disable(void);

void pok_preemption_local_enable(void);


/* 
 * Starts main thread and jumps into it.
 * 
 * Used only in partition_arinc.c.
 */
void sched_arinc_start(void);

void pok_sched_arinc_on_event(void);

/* Syscall: reschedule after leaving an msection (see <uapi/msection.h>). */
pok_ret_t jet_resched(void);

#endif /* ! POK_SCHED_ARINC_H */
