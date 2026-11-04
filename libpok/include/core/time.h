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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; deviations are documented where a
 * guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: pok_time_get()
 * checks the result of the system call (it returned an uninitialized
 * value on failure); deviations are documented where a guideline can't be
 * followed.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
 */


#ifndef POK_TIME_H
#define POK_TIME_H

#include <core/dependencies.h>

#include <types.h>
#include <errno.h>
#include <core/syscall.h>
/*
 * MISRA C:2025 Rule 21.10 deviation: only CLOCK_REALTIME is used from libpok's
 * own <time.h>; none of the time and date functions.
 */
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

    /*
     * MISRA C:2025 Rule 11.4 deviation: system call arguments are passed as
     * integers; the kernel converts the address back to a pointer.
     */
    if (pok_syscall2(POK_SYSCALL_CLOCK_GETTIME, (uintptr_t)CLOCK_REALTIME,
                     (uintptr_t)&res) != POK_ERRNO_OK) {
        res = 0; // The time is not available.
    }

    return res;
}

#define pok_thread_replenish pok_sched_replenish

#endif  /* POK_TIME_H */
