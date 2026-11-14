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
 * all if/else/loop bodies; essentially Boolean conditions; time() returns
 * (time_t)-1 if the system call fails (the result was uninitialized);
 * deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * time() of libpok.
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements one of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>
#include <core/syscall.h>

/**
 * @funcname `time`
 *
 * @description
 * Returns the calendar time (C standard): the time syscall.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timer` | `time_t *` | [out] Where the time is also stored, or NULL. |
 *
 * @return The time, or -1 if it isn't available.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `time_t` | The time |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `pok_syscall1(POK_SYSCALL_TIME, (uintptr_t)&ret)` is not equal to
 *    `POK_ERRNO_OK`:
 *    1. Set `ret` to `(time_t) -1`.
 * 2. If `timer` is not NULL:
 *    1. Set `*timer` to `ret`.
 * 3. Return `ret`.
 *
 * @calls
 * - `lja_do_syscall()`
 */
time_t time(time_t *timer)
{
    time_t ret;
    
    /*
     * MISRA C:2025 Rule 11.4 deviation: system call arguments are passed as
     * integers; the kernel converts the address back to a pointer.
     */
    if(pok_syscall1(POK_SYSCALL_TIME, (uintptr_t)&ret) != POK_ERRNO_OK) {
        ret = (time_t) -1; // The time is not available.
    }
    
    if(timer != NULL) {
        *timer = ret;
    }
    
    return ret;
}
