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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macro and the type of
 * strftime.c (TIME_FORMAT_DEPTH, time_format_state_t) are moved here from
 * libpok/libc/time/strftime.c.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
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

/**
 * @macroname `SECS_DAY`
 *
 * @description
 * The number of seconds in a day.
 */
#define SECS_DAY (60 * 60 * 24)

/*
 * Tells whether a year is a leap year.
 */
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

/*
 * Returns the number of days of a year.
 */
static inline int days_in_year(int year)
{
    return (leap_year(year) != 0) ? 366 : 365;
}

/* Global tm structure for non-reentrant functions.*/
extern struct tm global_tm;

/**
 * @typename `locale_time_t`
 *
 * @description
 * The names and formats of dates and times of a locale (strftime()).
 */
typedef struct {
    const char* weekday[7]; /**< The days of the week, from Sunday. */
    const char* weekday_abbrev[7]; /**< Their abbreviations. */

    const char* month[12]; /**< The months, from January. */
    const char* month_abbrev[12]; /**< Their abbreviations. */
    
    const char* date_time_format; /**< The format of '%%c'. */
    
    const char* am_pm[2]; /**< AM and PM, for '%%p'. */
    
    const char* time_12_format; /**< The format of '%%r'. */
    
    const char* date_format; /**< The format of '%%x'. */
    
    const char* time_format; /**< The format of '%%X'. */
    
    const char* tz_name; /**< The time zone name, for '%%Z'. */
} locale_time_t;

extern locale_time_t* current_locale_time;

/**
 * @macroname `TIME_FORMAT_DEPTH`
 *
 * @description
 * The depth of the stack of the formats being processed by strftime()
 * (time_format()). Conversions may be defined by other formats ("%c" is "%a %b
 * %e %T %Y", whose "%T" is "%H:%M:%S"). The formats being processed are kept in
 * a small stack instead of calling the formatter recursively (Rule 17.2).
 */
#define TIME_FORMAT_DEPTH 3U

/**
 * @typename `time_format_state_t`
 *
 * @description
 * The output of strftime(): where the next character goes, the room left and
 * the bytes used.
 */
typedef struct {
    char* out; /**< String for current output. */
    /** Bytes left in the output, including the terminating '\0'. */
    size_t n;
    size_t bytes_used_total; /**< Total number of bytes used. */
} time_format_state_t;

#endif /*LIBJET_TIME_TIME_INTERNAL_H*/
