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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: discarded
 * results are cast to void; the name tables have room for the terminating
 * null character; deviations are documented where a guideline can't be
 * followed.
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
 * asctime() of libpok.
 */

/*
 * MISRA C:2025 Rule 21.6 and Rule 21.10 deviation: this file implements
 * asctime() of libpok's C library, which formats with snprintf().
 */
#include <time.h>
#include <stdio.h>
#include <assert.h>

/**
 * @funcname `asctime`
 *
 * @description
 * Converts a broken-down time to a string like "Sun Sep 16 01:03:52 1973\n" (C
 * standard). The implementation is the one of the C99 standard, with snprintf()
 * instead of sprintf() and '%02d' instead of '%.2d' (the snprintf() of libpok
 * uses no precision for integers).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timeptr` | `const struct tm *` | [in] The broken-down time. |
 *
 * @return The string (in a static buffer).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mon_name` | `static const char[12][4]` | The abbreviated month names |
 * | `result` | `static char[26]` | The string |
 * | `wday_name` | `static const char[7][4]` | The abbreviated weekday names |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `snprintf()` with the arguments `result`, `sizeof(result)`,
 *    `"%.3s %.3s%3d %02d:%02d:%02d %%d\n"`, `wday_name[timeptr->tm_wday]`,
 *    `mon_name[timeptr->tm_mon]`, `timeptr->tm_mday`, `timeptr->tm_hour`,
 *    `timeptr->tm_min`, `timeptr->tm_sec` and `1900 + timeptr->tm_year`.
 * 2. Return `result`.
 *
 * @calls
 * - `snprintf()`
 */
char *asctime(const struct tm *timeptr)
{
    /*
     * Full implementation is taken from the C99 standard
     * with replacement sprintf->snprintf.
     * 
     * TODO: because currently snprintf implementation doesn't use
     * precision for integer specifiers, '%.2d' has been replaced with '%02d'
     */
    static const char wday_name[7][4] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static const char mon_name[12][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    static char result[26];
    (void) snprintf(result, sizeof(result), "%.3s %.3s%3d %02d:%02d:%02d %d\n",
        wday_name[timeptr->tm_wday],
        mon_name[timeptr->tm_mon],
        timeptr->tm_mday, timeptr->tm_hour,
        timeptr->tm_min, timeptr->tm_sec,
        1900 + timeptr->tm_year);

    return result;
}
