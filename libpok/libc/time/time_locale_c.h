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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: an include
 * guard is added.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 */


#ifndef LIBJET_TIME_LOCALE_C_H
#define LIBJET_TIME_LOCALE_C_H

/*
 * MISRA C:2025 Rule 21.10 deviation: this header is internal to the
 * <time.h> implementation of libpok's C library.
 */
#include <time.h>
#include "time_internal.h"

extern struct locale_time locale_time_c;

#endif /* LIBJET_TIME_LOCALE_C_H */
