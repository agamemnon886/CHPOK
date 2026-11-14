/*
 * COPIED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify original one (kernel/include/uapi/time.h).
 */
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
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros.
 */

#ifndef JET_UAPI_TIME_H
#define JET_UAPI_TIME_H

/**
 * @typename `time_t`
 *
 * @description
 * A calendar time, in seconds since the Epoch (POSIX).
 */
typedef long time_t;

// POSIX

/**
 * @typename `clockid_t`
 *
 * @description
 * The ID of a clock (POSIX): CLOCK_REALTIME or CLOCK_PROCESS_CPUTIME_ID.
 */
typedef int clockid_t;

/**
 * @name Clocks
 * The clocks of clock_gettime() (clockid_t).
 * @{
 */

/**
 * @macroname `CLOCK_REALTIME`
 *
 * @description
 * The system time, which the kernel gives (the only clock it supports).
 */
#define CLOCK_REALTIME 0

/**
 * @macroname `CLOCK_PROCESS_CPUTIME_ID`
 *
 * @description
 * The CPU time of the process. Not supported by the kernel.
 */
#define CLOCK_PROCESS_CPUTIME_ID 1

/** @} */

/**
 * @typename `struct timespec`
 *
 * @description
 * A time in seconds and nanoseconds (POSIX).
 */
struct timespec {
    time_t tv_sec; /**< The seconds. */
    long tv_nsec; /**< The nanoseconds (0 to 999999999). */
};


#endif /* JET_UAPI_TIME_H */
