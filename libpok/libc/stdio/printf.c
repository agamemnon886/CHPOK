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
 * Modified by Kirill Brazhnikov, 2026-10-31: thread-safe output: each
 * call formats into its own buffer and writes it under the stream lock.
 */

#include <stdio.h>
#include "printf_emitter.h"
#include "stream.h"

/* This function is part of C99, but we don't expose it to the user. */
static int vfprintf(FILE * restrict stream, const char * restrict format, va_list arg)
{
    struct stdio_operation op;

    stdio_operation_begin(&op, stream);
    printf_emitter(stdio_operation_emit_character, &op, format, arg);

    return stdio_operation_end(&op);
}

int vprintf(const char * restrict format, va_list arg)
{
    return vfprintf(stdout, format, arg);
}


int printf(const char * restrict format, ...)
{
    va_list arg;

    va_start(arg, format);
    int res = vprintf (format, arg);
    va_end(arg);

    return res;
}
