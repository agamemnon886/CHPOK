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
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements some of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>

#include "time_internal.h"

struct tm *localtime_r(const time_t *timer, struct tm* timeptr)
{
    // "Fake" timer, for which UTC time is same as local time for given timer.
    time_t local_timer = *timer - timezone;
    
    return gmtime_r(&local_timer, timeptr);
}

struct tm *localtime(const time_t *timer)
{
    return localtime_r(timer, &global_tm);
}
