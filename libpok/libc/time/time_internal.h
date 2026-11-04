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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; braces around all if/else/loop
 * bodies; essentially Boolean conditions; deviations are documented where
 * a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the date
 * constants are defined once, in time_internal.c, instead of in each file
 * that includes this header.
 */

#ifndef LIBJET_TIME_TIME_INTERNAL_H
#define LIBJET_TIME_TIME_INTERNAL_H

/*
 * MISRA C:2025 Rule 21.10 deviation: this header is internal to the
 * <time.h> implementation of libpok's C library.
 */
#include <time.h>

/* 
 * Number of days in month, dependent from month index (0-11),
 * and whether the year is LEAP(1) or not(0).
 */

extern const int days_in_month[2][12];

/* Base point for year in tm structure. */
extern const int tm0_year;

/* Base point for year in time_t. */
extern const int t0_year;

/* Number or seconds in single day. */
#define SECS_DAY (60 * 60 * 24)

/* Return 1 if year is LEAP and 0 otherwise. */
static inline int leap_year(int year)
{
    if((year % 4) != 0) {
        return 0; // non-divisible on 4
    }
    
    if((year % 100) != 0) {
        return 1; // divisible on 4 but non-divisible on 100
    }
    
    if((year % 400) != 0) {
        return 0; // divisible on 100 but non-divisible on 400
    }
    
    return 1; // divisible on 400.
}

/* Return number of the days in the given year.*/
static inline int days_in_year(int year)
{
    return (leap_year(year) != 0) ? 366 : 365;
}

/* Global tm structure for non-reentrant functions.*/
extern struct tm global_tm;

struct locale_time {
    const char* weekday[7]; // Since Sunday
    const char* weekday_abbrev[7]; // Since Sunday

    const char* month[12]; // Since Junuary
    const char* month_abbrev[12]; // Since Junuary
    
    const char* date_time_format; // For '%c' specifier
    
    const char* am_pm[2]; // For '%p' specifier
    
    const char* time_12_format; // For '%r' specifier
    
    const char* date_format; // For '%x' specifier
    
    const char* time_format; // For '%X' specifier
    
    const char* tz_name; // For '%Z' specifier
};

extern struct locale_time* current_locale_time;

#endif /*LIBJET_TIME_TIME_INTERNAL_H*/
