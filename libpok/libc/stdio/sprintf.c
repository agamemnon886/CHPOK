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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: U suffixes for
 * unsigned constants; no restrict qualifiers; counts are size_t;
 * characters are emitted as char; deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Long lines are wrapped at 80 columns.
 */

/*
 * MISRA C:2025 Rule 21.6 deviation: this file implements snprintf() and
 * vsnprintf() of libpok's C library. Rule 17.1: these functions take variable
 * arguments, as in the C standard.
 */
#include <stdio.h>
#include "printf_emitter.h"

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the callback gets its
 * private data as a pointer to void, which points to the object named by the
 * type it is converted to.
 */

struct string_stream
{
    char* str;
    size_t bytes_written;
    size_t bytes_max;
};

static void string_stream_emit_character(char c, void *_sstream)
{
    struct string_stream* sstream = _sstream;

    if(sstream->bytes_written < sstream->bytes_max) {
        sstream->str[sstream->bytes_written] = c;
    }

    sstream->bytes_written++;
}

int vsnprintf(char * s, size_t n, const char * format, va_list arg)
{
    struct string_stream sstream;

    sstream.str = s;
    sstream.bytes_written = 0U;
    sstream.bytes_max = (n > 0U) ? (n - 1U) : 0U;

    printf_emitter(&string_stream_emit_character, &sstream, format, arg);

    if(n > 0U) {
        size_t end = (sstream.bytes_written < sstream.bytes_max)
            ? sstream.bytes_written : sstream.bytes_max;
        s[end] = '\0';
    }

    // The count doesn't fit into int only for outputs of 2 GiB or more.
    return (int) sstream.bytes_written;
}

int snprintf(char * s, size_t n, const char * format, ...)
{
    va_list arg;

    va_start(arg, format);
    int res = vsnprintf (s, n, format, arg);
    va_end(arg);

    return res;
}
