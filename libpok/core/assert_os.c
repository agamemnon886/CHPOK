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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: discarded
 * results are cast to void; deviations are documented where a guideline
 * can't be followed.
 */

#include <core/assert_os.h>

#include <core/syscall.h>
/*
 * MISRA C:2025 Rule 21.6 deviation: snprintf() is libpok's own formatting
 * function; no streams or files are used.
 */
#include <stdio.h>

void assertion_os_fail(const char *expression, const char *file, int32_t line)
{
    static char msg[128];
    
    (void) snprintf(msg, sizeof(msg), "Assertion failed (%s) in %s:%d\n",
        expression, file, line);
    
    (void) pok_error_raise_os_error(msg, sizeof(msg));
}
