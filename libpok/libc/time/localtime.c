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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * localtime() and localtime_r() of libpok.
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements some of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>

#include "time_internal.h"

/**
 * @funcname `localtime_r`
 *
 * @description
 * Converts a calendar time to a broken-down local time (POSIX): gmtime_r() of a
 * "fake" time, whose UTC is the local time.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timer` | `const time_t *` | [in] The calendar time. |
 * | `timeptr` | `struct tm *` | [out] Where the broken-down time is stored. |
 *
 * @return timeptr.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timezone` | `long` | The difference between UTC and local time, in seconds |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `local_timer` | `time_t` | The fake time |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `local_timer` to `*timer - timezone`.
 * 2. Call `gmtime_r()` with the arguments the address of `local_timer` and
 *    `timeptr`, and return its result.
 *
 * @calls
 * - `gmtime_r()`
 */
struct tm *localtime_r(const time_t *timer, struct tm* timeptr)
{
    // "Fake" timer, for which UTC time is same as local time for given timer.
    time_t local_timer = *timer - timezone;
    
    return gmtime_r(&local_timer, timeptr);
}

/**
 * @funcname `localtime`
 *
 * @description
 * Converts a calendar time to a broken-down local time (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timer` | `const time_t *` | [in] The calendar time. |
 *
 * @return The broken-down time (a static one).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `global_tm` | `struct tm` | The broken-down time of gmtime() and localtime() |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `localtime_r()` with the arguments `timer` and the address of
 *    `global_tm`, and return its result.
 *
 * @calls
 * - `localtime_r()`
 */
struct tm *localtime(const time_t *timer)
{
    return localtime_r(timer, &global_tm);
}
