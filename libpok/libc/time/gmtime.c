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
 * gmtime() and gmtime_r() of libpok.
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements some of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>

#include "time_internal.h"

/**
 * @funcname `gmtime_r`
 *
 * @description
 * Converts a calendar time to a broken-down time in UTC (POSIX): days and
 * seconds of the day, then years and months counted one by one (day 0 was a
 * Thursday).
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
 * | `days_in_month` | `const int[2][12]` | The number of days of each month, in normal and leap years |
 * | `t0_year` | `const int` | The year of time 0 (1970) |
 * | `tm0_year` | `const int` | The year of tm_year 0 (1900) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dayclock` | `int` | The seconds since the day started |
 * | `dayno` | `int` | The days left |
 * | `t` | `time_t` | The calendar time |
 * | `year` | `int` | The year |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `t` to the value at `timer`.
 * 2. Set `year` to `t0_year`.
 * 3. Set `dayclock` to `(int)(t % SECS_DAY)`.
 * 4. Set `dayno` to `(int)(t / SECS_DAY)`.
 * 5. Set `timeptr->tm_sec` to `dayclock % 60`.
 * 6. Set `timeptr->tm_min` to `(dayclock % 3600) / 60`.
 * 7. Set `timeptr->tm_hour` to `dayclock / 3600`.
 * 8. Set `timeptr->tm_wday` to `(dayno + 4) % 7`.
 * 9. While `dayno` is greater than or equal to `days_in_year(year)`:
 *    1. Subtract `days_in_year(year)` from `dayno`.
 *    2. Increment `year`.
 * 10. Set `timeptr->tm_year` to `year - tm0_year`.
 * 11. Set `timeptr->tm_yday` to `dayno`.
 * 12. Set `timeptr->tm_mon` to `0`.
 * 13. While `dayno` is greater than or equal to
 *     `days_in_month[leap_year(year)][timeptr->tm_mon]`:
 *     1. Subtract `days_in_month[leap_year(year)][timeptr->tm_mon]` from
 *        `dayno`.
 *     2. Increment `timeptr->tm_mon`.
 * 14. Set `timeptr->tm_mday` to `dayno + 1`.
 * 15. Set `timeptr->tm_isdst` to `0`.
 * 16. Return `timeptr`.
 *
 * @calls
 * - `days_in_year()`
 * - `leap_year()`
 */
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

/**
 * @funcname `gmtime`
 *
 * @description
 * Converts a calendar time to a broken-down time in UTC (C standard).
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
 * 1. Call `gmtime_r()` with the arguments `timer` and the address of
 *    `global_tm`, and return its result.
 *
 * @calls
 * - `gmtime_r()`
 */
struct tm *gmtime(const time_t *timer)
{
    return gmtime_r(timer, &global_tm);
}
