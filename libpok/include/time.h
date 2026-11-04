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
 * guard isn't a reserved identifier; no restrict qualifiers; deviations
 * are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 */

/*
 * MISRA C:2025 Rule 5.10 and Rule 20.15 deviation (this header): this is
 * the <time.h> header of libpok's own C library (partitions are built
 * freestanding, without the system one), so it declares the standard
 * names and defines CLOCKS_PER_SEC.
 */
#ifndef LIBJET_TIME_H
#define LIBJET_TIME_H

#include <stddef.h>
#include <stdint.h>
#include <uapi/time.h>

#define CLOCKS_PER_SEC 1000000000

typedef int64_t clock_t;

struct tm
{
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *timeptr);
time_t time(time_t *timer);
char *asctime(const struct tm *timeptr);
char *ctime(const time_t *timer);
struct tm *gmtime(const time_t *timer);
struct tm *localtime(const time_t *timer);
size_t strftime(char * s, size_t maxsize, const char * format,
                const struct tm * timeptr);

/* POSIX */

extern long timezone;

//Variable 'daylight' is currently not supported and is always assumed to be 0.

struct tm *gmtime_r(const time_t *timer, struct tm* timeptr);
struct tm *localtime_r(const time_t *timer, struct tm* timeptr);

int clock_gettime(clockid_t clock_id, struct timespec* tp);

#endif /* LIBJET_TIME_H */
