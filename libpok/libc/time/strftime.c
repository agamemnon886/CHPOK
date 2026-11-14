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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macro and the type are moved
 * to time_internal.h; struct time_format_state is the typedef
 * time_format_state_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * strftime() of libpok. Conversions may be defined by other formats ("%c"
 * is "%a %b %e %T %Y", whose "%T" is "%H:%M:%S"); the formats being
 * processed are kept in a small stack instead of calling the formatter
 * recursively (Rule 17.2).
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

/**
 * @funcname `time_print_char`
 *
 * @description
 * Appends a single character to the output.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tfs` | `time_format_state_t *` | [in,out] The output. |
 * | `c` | `char` | [in] The character. |
 *
 * @return true on success, false if there is no room for it.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ok` | `bool` | Whether it was appended |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ok` to `false`.
 * 2. If `tfs->n` is greater than `1U`:
 *    1. Set `*tfs->out` to `c`.
 *    2. Increment `tfs->out`.
 *    3. Decrement `tfs->n`.
 *    4. Increment `tfs->bytes_used_total`.
 *    5. Set `ok` to `true`.
 * 3. Return `ok`.
 *
 * @calls
 * None.
 */
static bool time_print_char(time_format_state_t* tfs, char c)
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

/**
 * @funcname `time_print_string`
 *
 * @description
 * Appends a string to the output.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tfs` | `time_format_state_t *` | [in,out] The output. |
 * | `s` | `const char *` | [in] The string. |
 *
 * @return true on success, false if there is no room for it.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bytes_needed` | `size_t` | The length of the string |
 * | `ok` | `bool` | Whether it was appended |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `strlen()` with the argument `s`, and store the result in
 *    `bytes_needed`.
 * 2. Set `ok` to `false`.
 * 3. If `bytes_needed` is less than `tfs->n`:
 *    1. Call `memcpy()` with the arguments `tfs->out`, `s` and `bytes_needed`.
 *    2. Add `bytes_needed` to `tfs->bytes_used_total`.
 *    3. Add `bytes_needed` to `tfs->out`.
 *    4. Subtract `bytes_needed` from `tfs->n`.
 *    5. Set `ok` to `true`.
 * 4. Return `ok`.
 *
 * @calls
 * - `strlen()`
 * - `memcpy()`
 */
static bool time_print_string(time_format_state_t* tfs, const char* s)
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

/**
 * @funcname `time_print_number`
 *
 * @description
 * Appends a decimal number of at least a width, padded on the left; a negative
 * number is prefixed with '-'.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tfs` | `time_format_state_t *` | [in,out] The output. |
 * | `value` | `int64_t` | [in] The number. |
 * | `width` | `uint32_t` | [in] The width. |
 * | `pad` | `char` | [in] The padding character ('0' or ' '). |
 *
 * @return true on success, false if there is no room for it.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buf` | `char[24]` | The characters, last first |
 * | `digits` | `static const char[11]` | The digit characters |
 * | `len` | `uint32_t` | Their number |
 * | `magnitude` | `uint64_t` | The part not converted yet |
 * | `ok` | `bool` | Whether it was appended so far |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `len` to `0U`.
 * 2. Set `magnitude` to `(0U - (uint64_t) value)` if `value` is less than `0`,
 *    otherwise `(uint64_t) value`.
 * 3. Set `ok` to `true`.
 * 4. Repeat the following while `magnitude` is not equal to `0U` (at least
 *    once):
 *    1. Set `buf[len]` to `digits[magnitude % 10U]`.
 *    2. Increment `len`.
 *    3. Divide `magnitude` by `10U`.
 * 5. If `value` is less than `0`:
 *    1. Set `buf[len]` to `'-'`.
 *    2. Increment `len`.
 * 6. While `len` is less than `width` and `len` is less than
 *    `(uint32_t) sizeof(buf)`:
 *    1. Set `buf[len]` to `pad`.
 *    2. Increment `len`.
 * 7. While `ok` is true and `len` is greater than `0U`:
 *    1. Decrement `len`.
 *    2. Call `time_print_char()` with the arguments `tfs` and `buf[len]`, and
 *       store the result in `ok`.
 * 8. Return `ok`.
 *
 * @calls
 * - `time_print_char()`
 */
static bool time_print_number(time_format_state_t* tfs, int64_t value,
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

/**
 * @funcname `time_print_name`
 *
 * @description
 * Appends a name from a locale table, or "?" if the index is out of range.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tfs` | `time_format_state_t *` | [in,out] The output. |
 * | `names` | `const char * const[]` | [in] The table. |
 * | `count` | `int32_t` | [in] Its size. |
 * | `index` | `int32_t` | [in] The index. |
 *
 * @return true on success, false if there is no room for it.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `time_print_string()` with the arguments `tfs` and `names[index]` if
 *    (`index` is greater than or equal to `0` and `index` is less than
 *    `count`), otherwise `"?"`, and return its result.
 *
 * @calls
 * - `time_print_string()`
 */
static bool time_print_name(time_format_state_t* tfs,
    const char* const names[], int32_t count, int32_t index)
{
    return time_print_string(tfs,
        ((index >= 0) && (index < count)) ? names[index] : "?");
}

/**
 * @funcname `iso_weeks_in_year`
 *
 * @description
 * Returns the number of ISO 8601 weeks of a year: 53 if it starts on a
 * Thursday, or if it is a leap year which starts on a Wednesday (December 31st
 * is a Thursday then), else 52.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `year` | `int32_t` | [in] The year. |
 *
 * @return 52 or 53.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dec31_wday` | `int32_t` | The weekday of December 31st of the year |
 * | `prev` | `int32_t` | The preceding year |
 * | `prev_dec31_wday` | `int32_t` | The weekday of its December 31st |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `dec31_wday` to
 *    `(year + (year / 4) - (year / 100) + (year / 400)) % 7`.
 * 2. Set `prev` to `year - 1`.
 * 3. Set `prev_dec31_wday` to
 *    `(prev + (prev / 4) - (prev / 100) + (prev / 400)) % 7`.
 * 4. Return `53` if (`dec31_wday` is equal to `4` or `prev_dec31_wday` is equal
 *    to `3`), otherwise `52`.
 *
 * @calls
 * None.
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

/**
 * @funcname `iso_week`
 *
 * @description
 * Returns the ISO 8601 (week-based) year and week of a date: weeks begin on a
 * Monday; week 1 of a year is the week which contains January 4th, so the days
 * before it belong to the last week of the preceding year, and December
 * 29th-31st may belong to week 1 of the following year.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timeptr` | `const struct tm *` | [in] The date. |
 * | `year` | `int32_t *` | [out] Where the week-based year is stored. |
 * | `week` | `int32_t *` | [out] Where the week is stored. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tm0_year` | `const int` | The year of tm_year 0 (1900) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `w` | `int32_t` | The week |
 * | `wday_monday` | `int32_t` | The weekday, 1 (Monday) to 7 (Sunday) |
 * | `y` | `int32_t` | The year |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `y` to `timeptr->tm_year + tm0_year`.
 * 2. Set `wday_monday` to `((timeptr->tm_wday + 6) % 7) + 1`.
 * 3. Set `w` to `(((timeptr->tm_yday + 1) - wday_monday) + 10) / 7`.
 * 4. If `w` is less than `1`:
 *    1. Decrement `y`.
 *    2. Call `iso_weeks_in_year()` with the argument `y`, and store the result
 *       in `w`.
 * 5. Otherwise, if `w` is greater than `iso_weeks_in_year(y)`:
 *    1. Increment `y`.
 *    2. Set `w` to `1`.
 * 6. Otherwise:
 *    1. Do nothing.
 * 7. Set `*year` to `y`.
 * 8. Set `*week` to `w`.
 *
 * @calls
 * - `iso_weeks_in_year()`
 */
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

/**
 * @funcname `time_composite_format`
 *
 * @description
 * Returns the format which defines a conversion (such as %%c, %%D or %%T).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `spec` | `char` | [in] The conversion. |
 *
 * @return The format, or NULL if the conversion isn't defined by another
 *         format.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_locale_time` | `locale_time_t *` | The time formats of the current locale |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `format` | `const char *` | The format |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Depending on `spec`:
 *    1. Case `'c'`:
 *       1. Set `format` to `current_locale_time->date_time_format`.
 *       2. Leave the switch.
 *    2. Case `'D'`:
 *       1. Set `format` to `"%%m/%%d/%%y"`.
 *       2. Leave the switch.
 *    3. Case `'F'`:
 *       1. Set `format` to `"%%Y-%%m-%%d"`.
 *       2. Leave the switch.
 *    4. Case `'h'`:
 *       1. Set `format` to `"%%b"`.
 *       2. Leave the switch.
 *    5. Case `'r'`:
 *       1. Set `format` to `current_locale_time->time_12_format`.
 *       2. Leave the switch.
 *    6. Case `'R'`:
 *       1. Set `format` to `"%%H:%%M"`.
 *       2. Leave the switch.
 *    7. Case `'T'`:
 *       1. Set `format` to `"%%H:%%M:%%S"`.
 *       2. Leave the switch.
 *    8. Case `'x'`:
 *       1. Set `format` to `current_locale_time->date_format`.
 *       2. Leave the switch.
 *    9. Case `'X'`:
 *       1. Set `format` to `current_locale_time->time_format`.
 *       2. Leave the switch.
 *    10. Otherwise (default):
 *        1. Set `format` to `NULL`.
 *        2. Leave the switch.
 * 2. Return `format`.
 *
 * @calls
 * None.
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

/**
 * @funcname `time_convert`
 *
 * @description
 * Appends the result of a conversion which isn't defined by another format (an
 * undefined one gives "??").
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tfs` | `time_format_state_t *` | [in,out] The output. |
 * | `spec` | `char` | [in] The conversion. |
 * | `timeptr` | `const struct tm *` | [in] The broken-down time. |
 *
 * @return true on success, false if there is no room for it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_locale_time` | `locale_time_t *` | The time formats of the current locale |
 * | `timezone` | `long` | The difference between UTC and local time, in seconds |
 * | `tm0_year` | `const int` | The year of tm_year 0 (1900) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `east` | `int64_t` | The offset of the time zone east of UTC, in seconds |
 * | `iso_week_number` | `int32_t` | The week-based week |
 * | `iso_year` | `int32_t` | The week-based year |
 * | `locale` | `const locale_time_t *` | The time formats of the locale |
 * | `minutes` | `int64_t` | Its magnitude, in minutes |
 * | `ok` | `bool` | Whether it was appended |
 * | `year` | `int32_t` | The year |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `locale` to `current_locale_time`.
 * 2. Set `year` to `timeptr->tm_year + tm0_year`.
 * 3. Depending on `spec`:
 *    1. Case `'a'`:
 *       1. Call `time_print_name()` with the arguments `tfs`,
 *          `locale->weekday_abbrev`, `7` and `timeptr->tm_wday`, and store the
 *          result in `ok`.
 *       2. Leave the switch.
 *    2. Case `'A'`:
 *       1. Call `time_print_name()` with the arguments `tfs`,
 *          `locale->weekday`, `7` and `timeptr->tm_wday`, and store the result
 *          in `ok`.
 *       2. Leave the switch.
 *    3. Case `'b'`:
 *       1. Call `time_print_name()` with the arguments `tfs`,
 *          `locale->month_abbrev`, `12` and `timeptr->tm_mon`, and store the
 *          result in `ok`.
 *       2. Leave the switch.
 *    4. Case `'B'`:
 *       1. Call `time_print_name()` with the arguments `tfs`, `locale->month`,
 *          `12` and `timeptr->tm_mon`, and store the result in `ok`.
 *       2. Leave the switch.
 *    5. Case `'C'`:
 *       1. Call `time_print_number()` with the arguments `tfs`, `year / 100`,
 *          `2U` and `'0'`, and store the result in `ok`.
 *       2. Leave the switch.
 *    6. Case `'d'`:
 *       1. Call `time_print_number()` with the arguments `tfs`,
 *          `timeptr->tm_mday`, `2U` and `'0'`, and store the result in `ok`.
 *       2. Leave the switch.
 *    7. Case `'e'`:
 *       1. Call `time_print_number()` with the arguments `tfs`,
 *          `timeptr->tm_mday`, `2U` and `' '`, and store the result in `ok`.
 *       2. Leave the switch.
 *    8. Case `'g'`:
 *       1. Call `iso_week()` with the arguments `timeptr`, the address of
 *          `iso_year` and the address of `iso_week_number`.
 *       2. Call `time_print_number()` with the arguments `tfs`,
 *          `((iso_year % 100) + 100) % 100`, `2U` and `'0'`, and store the
 *          result in `ok`.
 *       3. Leave the switch.
 *    9. Case `'G'`:
 *       1. Call `iso_week()` with the arguments `timeptr`, the address of
 *          `iso_year` and the address of `iso_week_number`.
 *       2. Call `time_print_number()` with the arguments `tfs`, `iso_year`,
 *          `1U` and `'0'`, and store the result in `ok`.
 *       3. Leave the switch.
 *    10. Case `'H'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `timeptr->tm_hour`, `2U` and `'0'`, and store the result in `ok`.
 *        2. Leave the switch.
 *    11. Case `'I'`:
 *        1. Call `time_print_number()` with the arguments `tfs`, `12` if
 *           `(timeptr->tm_hour % 12)` is equal to `0`, otherwise
 *           `(timeptr->tm_hour % 12)`, `2U` and `'0'`, and store the result in
 *           `ok`.
 *        2. Leave the switch.
 *    12. Case `'j'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `timeptr->tm_yday + 1`, `3U` and `'0'`, and store the result in
 *           `ok`.
 *        2. Leave the switch.
 *    13. Case `'m'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `timeptr->tm_mon + 1`, `2U` and `'0'`, and store the result in
 *           `ok`.
 *        2. Leave the switch.
 *    14. Case `'M'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `timeptr->tm_min`, `2U` and `'0'`, and store the result in `ok`.
 *        2. Leave the switch.
 *    15. Case `'n'`:
 *        1. Call `time_print_char()` with the arguments `tfs` and `'\n'`, and
 *           store the result in `ok`.
 *        2. Leave the switch.
 *    16. Case `'p'`:
 *        1. Call `time_print_name()` with the arguments `tfs`, `locale->am_pm`,
 *           `2` and `timeptr->tm_hour / 12`, and store the result in `ok`.
 *        2. Leave the switch.
 *    17. Case `'S'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `timeptr->tm_sec`, `2U` and `'0'`, and store the result in `ok`.
 *        2. Leave the switch.
 *    18. Case `'t'`:
 *        1. Call `time_print_char()` with the arguments `tfs` and `'\t'`, and
 *           store the result in `ok`.
 *        2. Leave the switch.
 *    19. Case `'u'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `((timeptr->tm_wday + 6) % 7) + 1`, `1U` and `'0'`, and store the
 *           result in `ok`.
 *        2. Leave the switch.
 *    20. Case `'U'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `((timeptr->tm_yday + 7) - timeptr->tm_wday) / 7`, `2U` and `'0'`,
 *           and store the result in `ok`.
 *        2. Leave the switch.
 *    21. Case `'V'`:
 *        1. Call `iso_week()` with the arguments `timeptr`, the address of
 *           `iso_year` and the address of `iso_week_number`.
 *        2. Call `time_print_number()` with the arguments `tfs`,
 *           `iso_week_number`, `2U` and `'0'`, and store the result in `ok`.
 *        3. Leave the switch.
 *    22. Case `'w'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `timeptr->tm_wday`, `1U` and `'0'`, and store the result in `ok`.
 *        2. Leave the switch.
 *    23. Case `'W'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `((timeptr->tm_yday + 7) - ((timeptr->tm_wday + 6) % 7)) / 7`, `2U`
 *           and `'0'`, and store the result in `ok`.
 *        2. Leave the switch.
 *    24. Case `'y'`:
 *        1. Call `time_print_number()` with the arguments `tfs`,
 *           `((year % 100) + 100) % 100`, `2U` and `'0'`, and store the result
 *           in `ok`.
 *        2. Leave the switch.
 *    25. Case `'Y'`:
 *        1. Call `time_print_number()` with the arguments `tfs`, `year`, `1U`
 *           and `'0'`, and store the result in `ok`.
 *        2. Leave the switch.
 *    26. Case `'z'`:
 *        1. Set `east` to `-(int64_t) timezone`.
 *        2. Set `minutes` to `((east < 0) ? -east : east) / 60`.
 *        3. Call `time_print_char()` with the arguments `tfs` and `'-'` if
 *           `east` is less than `0`, otherwise `'+'`, and store the result in
 *           `ok`.
 *        4. If `ok` is true:
 *           1. Call `time_print_number()` with the arguments `tfs`,
 *              `((minutes / 60) * 100) + (minutes % 60)`, `4U` and `'0'`, and
 *              store the result in `ok`.
 *        5. Leave the switch.
 *    27. Case `'Z'`:
 *        1. Call `time_print_string()` with the arguments `tfs` and
 *           `locale->tz_name`, and store the result in `ok`.
 *        2. Leave the switch.
 *    28. Case `'%%'`:
 *        1. Call `time_print_char()` with the arguments `tfs` and `'%%'`, and
 *           store the result in `ok`.
 *        2. Leave the switch.
 *    29. Otherwise (default):
 *        1. Call `time_print_string()` with the arguments `tfs` and `"??"`, and
 *           store the result in `ok`.
 *        2. Leave the switch.
 * 4. Return `ok`.
 *
 * @calls
 * - `time_print_name()`
 * - `time_print_number()`
 * - `iso_week()`
 * - `time_print_char()`
 * - `time_print_string()`
 */
static bool time_convert(time_format_state_t* tfs, char spec,
    const struct tm* timeptr)
{
    const locale_time_t* locale = current_locale_time;
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

/**
 * @funcname `time_format`
 *
 * @description
 * Produces the string of a format and a broken-down time according to the rules
 * of strftime(), without the terminating NUL; the E and O modifiers are
 * ignored.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tfs` | `time_format_state_t *` | [in,out] The output. |
 * | `format` | `const char *` | [in] The format. |
 * | `timeptr` | `const struct tm *` | [in] The broken-down time. |
 *
 * @return true on success, false if there isn't enough room.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `depth` | `uint32_t` | The index of the innermost one |
 * | `done` | `bool` | Whether the format has ended |
 * | `formats` | `const char *[3]` | The formats being processed |
 * | `ok` | `bool` | Whether the output is complete so far |
 * | `spec` | `char` | The conversion |
 * | `sub_format` | `const char *` | The format of a composite conversion |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `depth` to `0U`.
 * 2. Set `ok` to `true`.
 * 3. Set `done` to `false`.
 * 4. Set `formats[0]` to `format`.
 * 5. While `ok` is true and `done` is false:
 *    1. If the value at `formats[depth]` is equal to `'\0'`:
 *       1. If `depth` is equal to `0U`:
 *          1. Set `done` to `true`.
 *       2. Otherwise:
 *          1. Decrement `depth`.
 *    2. Otherwise, if the value at `formats[depth]` is not equal to `'%%'`:
 *       1. Call `time_print_char()` with the arguments `tfs` and the value at
 *          `formats[depth]`, and store the result in `ok`.
 *       2. Increment `formats[depth]`.
 *    3. Otherwise:
 *       1. Increment `formats[depth]`.
 *       2. While the value at `formats[depth]` is equal to `'E'` or the value
 *          at `formats[depth]` is equal to `'O'`:
 *          1. Increment `formats[depth]`.
 *       3. Set `spec` to the value at `formats[depth]`.
 *       4. If `spec` is equal to `'\0'`:
 *          1. Set `done` to `true`.
 *       5. Otherwise:
 *          1. Increment `formats[depth]`.
 *          2. Call `time_composite_format()` with the argument `spec`, and
 *             store the result in `sub_format`.
 *          3. If `sub_format` is not NULL and `depth` is less than
 *             `(TIME_FORMAT_DEPTH - 1U)`:
 *             1. Increment `depth`.
 *             2. Set `formats[depth]` to `sub_format`.
 *          4. Otherwise:
 *             1. Call `time_convert()` with the arguments `tfs`, `spec` and
 *                `timeptr`, and store the result in `ok`.
 * 6. Return `ok`.
 *
 * @calls
 * - `time_print_char()`
 * - `time_composite_format()`
 * - `time_convert()`
 */
static bool time_format(time_format_state_t* tfs, const char* format,
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

/**
 * @funcname `strftime`
 *
 * @description
 * Formats a broken-down time into a buffer of a size (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [out] The buffer. |
 * | `maxsize` | `size_t` | [in] Its size. |
 * | `format` | `const char *` | [in] The format. |
 * | `timeptr` | `const struct tm *` | [in] The broken-down time. |
 *
 * @return The number of characters (without the NUL), or 0 if they don't fit.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `result` | `size_t` | The number of characters |
 * | `tfs` | `time_format_state_t` | The output |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `result` to `0U`.
 * 2. Set `tfs.out` to `s`.
 * 3. Set `tfs.n` to `maxsize`.
 * 4. Set `tfs.bytes_used_total` to `0U`.
 * 5. If `maxsize` is not equal to `0U`:
 *    1. If `time_format(&tfs, format, timeptr)` is true:
 *       1. Set `*tfs.out` to `'\0'`.
 *       2. Set `result` to `tfs.bytes_used_total`.
 * 6. Return `result`.
 *
 * @calls
 * - `time_format()`
 */
size_t strftime(char * s, size_t maxsize, const char * format,
                const struct tm * timeptr)
{
    time_format_state_t tfs;
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
