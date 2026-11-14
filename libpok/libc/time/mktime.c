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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * mktime() of libpok.
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements one of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>
#include "time_internal.h"

/**
 * @funcname `mktime`
 *
 * @description
 * Converts a broken-down local time to a calendar time (C standard),
 * normalizing its fields: days since the Epoch to the month (years and months
 * are the only intervals of variable duration), then the seconds.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timeptr` | `struct tm *` | [in,out] The broken-down time (its fields are corrected). |
 *
 * @return The calendar time, or -1 before 1970 or on an overflow.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `days_in_month` | `const int[2][12]` | The number of days of each month, in normal and leap years |
 * | `t0_year` | `const int` | The year of time 0 (1970) |
 * | `timezone` | `long` | The difference between UTC and local time, in seconds |
 * | `tm0_year` | `const int` | The year of tm_year 0 (1900) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `actual_month` | `int` | The month, 0-11 |
 * | `actual_year` | `int` | The year |
 * | `local_timer` | `time_t` | The time, as if it were UTC |
 * | `month` | `int` | A month counted |
 * | `yday` | `int` | The days since the Epoch |
 * | `year` | `int` | A year counted |
 * | `year_is_leap` | `int` | Whether the year is a leap year |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `local_timer` to `0`.
 * 2. Set `actual_year` to `timeptr->tm_year + tm0_year`.
 * 3. Set `actual_month` to `timeptr->tm_mon`.
 * 4. Add `actual_month / 12` to `actual_year`.
 * 5. Set `actual_month` to `actual_month % 12`.
 * 6. If `actual_year` is less than `t0_year`:
 *    1. Return `-1`.
 * 7. Set `yday` to `0`.
 * 8. Set `year` to `t0_year`; then while `year` is less than `actual_year`,
 *    repeat (after each pass: increment `year`):
 *    1. Add `days_in_year(year)` to `yday`.
 *    2. If `yday` is less than `0`:
 *       1. Return `-1`.
 * 9. Call `leap_year()` with the argument `actual_year`, and store the result
 *    in `year_is_leap`.
 * 10. Set `month` to `0`; then while `month` is less than `actual_month`,
 *     repeat (after each pass: increment `month`):
 *     1. Add `days_in_month[year_is_leap][month]` to `yday`.
 * 11. Add `timeptr->tm_mday - 1` to `yday`.
 * 12. If `yday` is less than `0`:
 *     1. Return `-1`.
 * 13. Set `local_timer` to
 *     `((time_t)yday * SECS_DAY) + timeptr->tm_sec + (timeptr->tm_min * 60) +`
 *     `(timeptr->tm_hour * (60 * 60))`.
 * 14. Call `gmtime_r()` with the arguments the address of `local_timer` and
 *     `timeptr`.
 * 15. Return `local_timer + timezone`.
 *
 * @calls
 * - `days_in_year()`
 * - `leap_year()`
 * - `gmtime_r()`
 */
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
