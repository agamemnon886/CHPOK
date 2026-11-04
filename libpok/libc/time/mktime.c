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
 * all if/else/loop bodies; discarded results are cast to void; deviations
 * are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized.
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements one of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>
#include "time_internal.h"

time_t mktime(struct tm *timeptr)
{
    // "As if" time is UTC.
    time_t local_timer = 0;
    
    int actual_year = timeptr->tm_year + tm0_year;
    int actual_month = timeptr->tm_mon;

    actual_year += actual_month / 12;
    actual_month = actual_month % 12;
    
    // Year shouldn't be before 1970 or after MAX_INT32
    if(actual_year < t0_year) {
        return -1;
    }

    /* 
     * Year and month are the only intervals which has non-constant duration.
     * 
     * Compute number of days since Epoch till beginning of the month.
     */
    int yday = 0;
    for(int year = t0_year; year < actual_year; year++) {
        yday += days_in_year(year);
        if(yday < 0) {
            return -1; // Overflow
        }
    }
    
    int year_is_leap = leap_year(actual_year);
    
    for(int month = 0; month < actual_month; month++) {
        yday += days_in_month[year_is_leap][month];
    }
    
    yday += timeptr->tm_mday - 1;
    
    if(yday < 0) {
        return -1; // Overflow
    }
    
    
    // Compute the time.
    local_timer = ((time_t)yday * SECS_DAY)
        + timeptr->tm_sec
        + (timeptr->tm_min * 60)
        + (timeptr->tm_hour * (60 * 60));
    
    // Correct fields using reverse convertion.
    (void) gmtime_r(&local_timer, timeptr);

    // Take timezone into account.
    return local_timer + timezone;
}
