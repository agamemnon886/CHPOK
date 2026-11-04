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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 */

#ifndef LIBJET_ASSERT_OS_H
#define LIBJET_ASSERT_OS_H

#include <stdint.h>

void assertion_os_fail(const char *expression, const char *file, int32_t line);

/*
 * MISRA C:2025 Rule 20.10 deviation: the failed expression is printed with
 * the # operator, as for the standard assert().
 */
#define assert_os(expr) ( \
        (expr) ? \
        ((void) 0) : \
        assertion_os_fail(#expr, __FILE__, __LINE__) \
    )

#endif /* LIBJET_ASSERT_OS_H */
