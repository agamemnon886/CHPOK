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
 */

#include <stdio.h>
#include "stream.h"

int puts(const char *s)
{
    struct stdio_operation op;

    stdio_operation_begin(&op, stdout);
    for(; *s; s++)
        stdio_operation_emit_character(*s, &op);
    stdio_operation_emit_character('\n', &op);
    stdio_operation_end(&op);

    return 0;
}
