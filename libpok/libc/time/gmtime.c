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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the local
 * variable no longer hides time(); deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: one declarator
 * per declaration.
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements some of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>

#include "time_internal.h"

struct tm *gmtime_r(const time_t *timer, struct tm* timeptr)
{
    time_t t = *timer;
    int dayclock;
    int dayno;
    int year = t0_year;
 
    /* Extract total number of days and number of seconds since day starts. */
    dayclock = (int)(t % SECS_DAY);
    dayno = (int)(t / SECS_DAY);
 
    timeptr->tm_sec = dayclock % 60;
    timeptr->tm_min = (dayclock % 3600) / 60;
    timeptr->tm_hour = dayclock / 3600;
    timeptr->tm_wday = (dayno + 4) % 7;       /* day 0 was a thursday */
 
    /* Just count years, one by one. */
    while (dayno >= days_in_year(year)) {
        dayno -= days_in_year(year);
        year++;
    }
 
    timeptr->tm_year = year - tm0_year;
    timeptr->tm_yday = dayno;
    timeptr->tm_mon = 0;
 
    /* Compute using tm_yday compute tm_mon and tm_mday. */
    while (dayno >= days_in_month[leap_year(year)][timeptr->tm_mon]) {
        dayno -= days_in_month[leap_year(year)][timeptr->tm_mon];
        timeptr->tm_mon++;
    }
 
    timeptr->tm_mday = dayno + 1;
    timeptr->tm_isdst = 0;

    return timeptr;
}

struct tm *gmtime(const time_t *timer)
{
    return gmtime_r(timer, &global_tm);
}
