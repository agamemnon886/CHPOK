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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * ctime() of libpok.
 */

/*
 * MISRA C:2025 Rule 21.10 deviation: this file implements one of the
 * <time.h> functions of libpok's C library.
 */
#include <time.h>

/**
 * @funcname `ctime`
 *
 * @description
 * Converts a calendar time to a local time string (C standard):
 * asctime(localtime(timer)), the equivalent that C99 gives.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timer` | `const time_t *` | [in] The calendar time. |
 *
 * @return The string (in a static buffer).
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
 * 1. Call `asctime()` with the argument `localtime(timer)`, and return its
 *    result.
 *
 * @calls
 * - `asctime()`
 * - `localtime()`
 */
char *ctime(const time_t *timer)
{
    // Use equivalent explicitely pointed in C99.
    return asctime(localtime(timer));
}
