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
 * Modified by Kirill Brazhnikov, 2026-10-31: thread-safe output (see
 * stream.c).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; discarded results are cast to void; deviations
 * are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: an index loop
 * that doesn't modify the parameter.
 */

/*
 * MISRA C:2025 Rule 21.6 deviation: this file implements puts() of libpok's
 * C library.
 */
#include <stdio.h>
#include "stream.h"

int puts(const char *s)
{
    struct stdio_operation op;

    stdio_operation_begin(&op, stdout);
    for(size_t i = 0U; s[i] != '\0'; i++) {
        stdio_operation_emit_character(s[i], &op);
    }
    stdio_operation_emit_character('\n', &op);
    (void) stdio_operation_end(&op);

    return 0;
}
