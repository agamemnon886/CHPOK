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
 */

#ifndef JET_UAPI_TIME_H
#define JET_UAPI_TIME_H

typedef long time_t;

// POSIX
typedef int clockid_t;

#define CLOCK_REALTIME 0
#define CLOCK_PROCESS_CPUTIME_ID 1

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};


#endif /* JET_UAPI_TIME_H */
