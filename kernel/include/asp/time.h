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
 * Modified by Kirill Brazhnikov, 2026-11-04: ja_system_time_stop() and
 * ja_system_time_resume() stop the system time while the GDB server holds
 * the system.
 */

#ifndef JET_ASP_TIME_H
#define JET_ASP_TIME_H

#include <config.h>
#include <types.h>
#include <uapi/time.h>

/* Return current system time. */
pok_time_t ja_system_time(void);

/* Return current calendar time (seconds since Epoch). */
time_t ja_calendar_time(void);

#ifdef POK_NEEDS_GDB
/*
 * Stop the system time while the GDB server holds the system, and let it
 * go on from the same value when the system resumes, so that time slots,
 * deadlines and timers don't expire because of debugging.
 */
void ja_system_time_stop(void);
void ja_system_time_resume(void);
#endif /* POK_NEEDS_GDB */


#endif /* JET_ASP_TIME_H */
