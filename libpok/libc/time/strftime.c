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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: nested formats
 * (%c, %D, %T, ...) are processed with a stack of formats instead of
 * recursion, and numbers are printed without <stdarg.h>; every switch
 * clause ends with break. Fixed conversions: %m and %j were one less, %C
 * printed the year modulo 100, %e was zero-padded, %F used U+2212 instead of
 * '-', %G printed two digits, %I printed 01 for noon, %U, %V and %W were
 * computed wrongly and %U and %W were not zero-padded, %z treated the
 * timezone as minutes, and %% printed "%??". A literal text that didn't fit
 * was silently truncated instead of making strftime() return 0.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Long lines are wrapped at 80 columns.
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements one of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "time_internal.h"

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): the output position moves
 * forward by the number of bytes written.
 */

/*
 * Conversions may be defined by other formats ("%c" is "%a %b %e %T %Y",
 * whose "%T" is "%H:%M:%S"). The formats being processed are kept in a
 * small stack instead of calling the formatter recursively (Rule 17.2).
 */
#define TIME_FORMAT_DEPTH 3U

struct time_format_state {
    char* out; // String for current output
    size_t n; // Bytes left in the output, including the terminating '\0'.

    size_t bytes_used_total; // Total number of bytes used.
};

/*
 * Append a single character.
 *
 * Return true on success, false if there is no room for it.
 */
static bool time_print_char(struct time_format_state* tfs, char c)
{
    bool ok = false;

    if(tfs->n > 1U) {
        *tfs->out = c;
        tfs->out++;
        tfs->n--;
        tfs->bytes_used_total++;
        ok = true;
    }
    return ok;
}

/*
 * Append a string.
 *
 * Return true on success, false if there is no room for it.
 */
static bool time_print_string(struct time_format_state* tfs, const char* s)
{
    size_t bytes_needed = strlen(s);
    bool ok = false;

    if(bytes_needed < tfs->n) {
        (void) memcpy(tfs->out, s, bytes_needed);
        tfs->bytes_used_total += bytes_needed;
        // Advance position
        tfs->out += bytes_needed;
        tfs->n -= bytes_needed;
        ok = true;
    }
    return ok;
}

/*
 * Append a decimal number of at least 'width' characters, padded on the left
 * with 'pad' ('0' or ' '). A negative number is prefixed with '-'.
 *
 * Return true on success, false if there is no room for it.
 */
static bool time_print_number(struct time_format_state* tfs, int64_t value,
    uint32_t width, char pad)
{
    static const char digits[] = "0123456789";
    char buf[24];
    uint32_t len = 0U;
    uint64_t magnitude = (value < 0) ? (0U - (uint64_t) value)
        : (uint64_t) value;
    bool ok = true;

    do {
        buf[len] = digits[magnitude % 10U];
        len++;
        magnitude /= 10U;
    } while(magnitude != 0U);

    if(value < 0) {
        buf[len] = '-';
        len++;
    }
    while((len < width) && (len < (uint32_t) sizeof(buf))) {
        buf[len] = pad;
        len++;
    }
    while(ok && (len > 0U)) {
        len--;
        ok = time_print_char(tfs, buf[len]);
    }
    return ok;
}

/* Append a name from a locale table, or "?" if the index is out of range. */
static bool time_print_name(struct time_format_state* tfs,
    const char* const names[], int32_t count, int32_t index)
{
    return time_print_string(tfs,
        ((index >= 0) && (index < count)) ? names[index] : "?");
}

/*
 * ISO 8601 (week-based) year and week:
 *
 *  - weeks begin on a Monday;
 *  - week 1 of a year is the week which contains January 4th, so the days
 *    before it belong to the last week of the preceding year, and
 *    December 29th-31st may belong to week 1 of the following year.
 */
static int32_t iso_weeks_in_year(int32_t year)
{
    // A year has 53 weeks if it starts on a Thursday, or if it is a leap year
    // which starts on a Wednesday (December 31st is a Thursday then).
    int32_t dec31_wday = (year + (year / 4) - (year / 100) + (year / 400)) % 7;
    int32_t prev = year - 1;
    int32_t prev_dec31_wday =
        (prev + (prev / 4) - (prev / 100) + (prev / 400)) % 7;

    return ((dec31_wday == 4) || (prev_dec31_wday == 3)) ? 53 : 52;
}

static void iso_week(const struct tm* timeptr, int32_t* year, int32_t* week)
{
    int32_t y = timeptr->tm_year + tm0_year;
    // 1 (Monday) - 7 (Sunday)
    int32_t wday_monday = ((timeptr->tm_wday + 6) % 7) + 1;
    int32_t w = (((timeptr->tm_yday + 1) - wday_monday) + 10) / 7;

    if(w < 1) {
        y--;
        w = iso_weeks_in_year(y);
    }
    else if(w > iso_weeks_in_year(y)) {
        y++;
        w = 1;
    }
    else {
        // The week belongs to the year of the date.
    }
    *year = y;
    *week = w;
}

/*
 * Return the format which defines conversion 'spec', or NULL if the
 * conversion is not defined by another format.
 */
static const char* time_composite_format(char spec)
{
    const char* format;

    switch(spec) {
        case 'c':
            format = current_locale_time->date_time_format;
            break;
        case 'D':
            format = "%m/%d/%y";
            break;
        case 'F':
            format = "%Y-%m-%d";
            break;
        case 'h':
            format = "%b";
            break;
        case 'r':
            format = current_locale_time->time_12_format;
            break;
        case 'R':
            format = "%H:%M";
            break;
        case 'T':
            format = "%H:%M:%S";
            break;
        case 'x':
            format = current_locale_time->date_format;
            break;
        case 'X':
            format = current_locale_time->time_format;
            break;
        default:
            format = NULL;
            break;
    }
    return format;
}

/*
 * Append the result of a conversion which is not defined by another format.
 *
 * Return true on success, false if there is no room for it.
 */
static bool time_convert(struct time_format_state* tfs, char spec,
    const struct tm* timeptr)
{
    const struct locale_time* locale = current_locale_time;
    int32_t year = timeptr->tm_year + tm0_year;
    int32_t iso_year;
    int32_t iso_week_number;
    bool ok;

    switch(spec) {
        case 'a':
            ok = time_print_name(tfs, locale->weekday_abbrev, 7,
                                 timeptr->tm_wday);
            break;
        case 'A':
            ok = time_print_name(tfs, locale->weekday, 7, timeptr->tm_wday);
            break;
        case 'b':
            ok = time_print_name(tfs, locale->month_abbrev, 12,
                                 timeptr->tm_mon);
            break;
        case 'B':
            ok = time_print_name(tfs, locale->month, 12, timeptr->tm_mon);
            break;
        case 'C': // 00-99, the year divided by 100.
            ok = time_print_number(tfs, year / 100, 2U, '0');
            break;
        case 'd': // 01-31
            ok = time_print_number(tfs, timeptr->tm_mday, 2U, '0');
            break;
        case 'e': // 1-31, single digit prepended by space.
            ok = time_print_number(tfs, timeptr->tm_mday, 2U, ' ');
            break;
        case 'g': // 00-99, week-based.
            iso_week(timeptr, &iso_year, &iso_week_number);
            ok = time_print_number(tfs, ((iso_year % 100) + 100) % 100, 2U,
                                   '0');
            break;
        case 'G': // Week-based.
            iso_week(timeptr, &iso_year, &iso_week_number);
            ok = time_print_number(tfs, iso_year, 1U, '0');
            break;
        case 'H': // 00-23.
            ok = time_print_number(tfs, timeptr->tm_hour, 2U, '0');
            break;
        case 'I': // 01-12.
            ok = time_print_number(tfs,
                ((timeptr->tm_hour % 12) == 0) ? 12 : (timeptr->tm_hour % 12),
                2U, '0');
            break;
        case 'j': // 001-366.
            ok = time_print_number(tfs, timeptr->tm_yday + 1, 3U, '0');
            break;
        case 'm': // 01-12.
            ok = time_print_number(tfs, timeptr->tm_mon + 1, 2U, '0');
            break;
        case 'M': // 00-59.
            ok = time_print_number(tfs, timeptr->tm_min, 2U, '0');
            break;
        case 'n':
            ok = time_print_char(tfs, '\n');
            break;
        case 'p':
            ok = time_print_name(tfs, locale->am_pm, 2, timeptr->tm_hour / 12);
            break;
        case 'S': // 00-60.
            ok = time_print_number(tfs, timeptr->tm_sec, 2U, '0');
            break;
        case 't':
            ok = time_print_char(tfs, '\t');
            break;
        case 'u': // 1-7, since Monday
            ok = time_print_number(tfs, ((timeptr->tm_wday + 6) % 7) + 1, 1U,
                                   '0');
            break;
        case 'U': // 00-53, the first Sunday is the first day of week 1.
            ok = time_print_number(tfs,
                ((timeptr->tm_yday + 7) - timeptr->tm_wday) / 7, 2U, '0');
            break;
        case 'V': // 01-53, week-based.
            iso_week(timeptr, &iso_year, &iso_week_number);
            ok = time_print_number(tfs, iso_week_number, 2U, '0');
            break;
        case 'w': // 0-6, since Sunday
            ok = time_print_number(tfs, timeptr->tm_wday, 1U, '0');
            break;
        case 'W': // 00-53, the first Monday is the first day of week 1.
            ok = time_print_number(tfs,
                ((timeptr->tm_yday + 7) - ((timeptr->tm_wday + 6) % 7)) / 7,
                2U, '0');
            break;
        case 'y': // 00-99.
            ok = time_print_number(tfs, ((year % 100) + 100) % 100, 2U, '0');
            break;
        case 'Y':
            ok = time_print_number(tfs, year, 1U, '0');
            break;
        case 'z':
            {
                // 'timezone' is the number of seconds west of UTC; %z is
                // +hhmm east of it.
                int64_t east = -(int64_t) timezone;
                int64_t minutes = ((east < 0) ? -east : east) / 60;

                ok = time_print_char(tfs, (east < 0) ? '-' : '+');
                if(ok) {
                    ok = time_print_number(tfs,
                        ((minutes / 60) * 100) + (minutes % 60), 4U, '0');
                }
            }
            break;
        case 'Z':
            ok = time_print_string(tfs, locale->tz_name);
            break;
        case '%':
            ok = time_print_char(tfs, '%');
            break;
        default: // Behaviour is undefined.
            ok = time_print_string(tfs, "??");
            break;
    }
    return ok;
}

/*
 * Produce string for formatted time according to rules of strftime
 * except appending terminating '\0'.
 *
 * Return true on success, false if there is not enough space.
 */
static bool time_format(struct time_format_state* tfs, const char* format,
    const struct tm * timeptr)
{
    const char* formats[TIME_FORMAT_DEPTH]; // Formats being processed.
    uint32_t depth = 0U; // Index of the innermost format in 'formats'.
    bool ok = true;
    bool done = false;

    formats[0] = format;

    while(ok && !done) {
        const char* sub_format;
        char spec;

        if(*formats[depth] == '\0') {
            // End of this format: continue with the enclosing one.
            if(depth == 0U) {
                done = true;
            }
            else {
                depth--;
            }
        }
        else if(*formats[depth] != '%') {
            ok = time_print_char(tfs, *formats[depth]);
            formats[depth]++;
        }
        else {
            formats[depth]++; // First character after '%'

            // Currently 'E' and 'O' modifiers are ignored.
            while((*formats[depth] == 'E') || (*formats[depth] == 'O')) {
                formats[depth]++;
            }

            spec = *formats[depth];
            if(spec == '\0') {
                // Missed actual specifier after '%': stop here.
                done = true;
            }
            else {
                formats[depth]++;
                sub_format = time_composite_format(spec);
                if((sub_format != NULL) && (depth < (TIME_FORMAT_DEPTH - 1U))) {
                    depth++;
                    formats[depth] = sub_format;
                }
                else {
                    ok = time_convert(tfs, spec, timeptr);
                }
            }
        }
    }
    return ok;
}

size_t strftime(char * s, size_t maxsize, const char * format,
                const struct tm * timeptr)
{
    struct time_format_state tfs;
    size_t result = 0U;

    tfs.out = s;
    tfs.n = maxsize;
    tfs.bytes_used_total = 0U;

    if(maxsize != 0U) {
        if(time_format(&tfs, format, timeptr)) {
            // Append terminating null.
            *tfs.out = '\0';
            result = tfs.bytes_used_total;
        }
    }
    return result;
}
