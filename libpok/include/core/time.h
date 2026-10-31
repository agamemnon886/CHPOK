/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef __POK_TIME_H__
#define __POK_TIME_H__

#include <core/dependencies.h>

#include <types.h>
#include <errno.h>
#include <core/syscall.h>
#include <time.h>

/*
 * Compute a deadline from now according to the first parameter.
 * This function is a facility to compute tasks' deadline.
 */
pok_ret_t pok_time_compute_deadline (const pok_time_t relative, pok_time_t* absolute);

/*
 * Get number of nanoseconds that passed since the system starts.
 */
static inline pok_time_t pok_time_get(void)
{
    pok_time_t res;

    pok_syscall2(POK_SYSCALL_CLOCK_GETTIME, (uintptr_t)CLOCK_REALTIME, (uintptr_t)&res);

    return res;
}

#define pok_thread_replenish pok_sched_replenish

#endif  /* __POK_TIME_H__ */
