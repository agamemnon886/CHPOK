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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the result of
 * the system call is compared with POK_ERRNO_OK; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * clock() of libpok.
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements one of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>
#include <core/syscall.h>

/**
 * @funcname `clock`
 *
 * @description
 * Returns the processor time (C standard): the system time, in nanoseconds.
 *
 * @return The time, or -1 if it isn't available.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `pok_ret_e` | The result of the syscall |
 * | `t` | `pok_time_t` | The time |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ret` to
 *    `pok_syscall2(POK_SYSCALL_CLOCK_GETTIME, (uintptr_t)CLOCK_REALTIME,`
 *    `(uintptr_t)&t)`.
 * 2. If `ret` is not equal to `POK_ERRNO_OK`:
 *    1. Return `-1`.
 * 3. Otherwise:
 *    1. Return `(clock_t)t`.
 *
 * @calls
 * - `lja_do_syscall()`
 */
clock_t clock(void)
{
    pok_time_t t;
    
    /*
     * MISRA C:2025 Rule 11.4 deviation: system call arguments are passed as
     * integers; the kernel converts the address back to a pointer.
     */
    pok_ret_e ret = pok_syscall2(POK_SYSCALL_CLOCK_GETTIME,
                                 (uintptr_t)CLOCK_REALTIME, (uintptr_t)&t);
    
    if(ret != POK_ERRNO_OK) {
        return -1; // Not supported.
    } else {
        return (clock_t)t;
    }
}
