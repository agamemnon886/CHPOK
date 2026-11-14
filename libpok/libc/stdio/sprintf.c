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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the type is moved to
 * libpok/libc/stdio/printf_emitter.h; struct string_stream is the typedef
 * string_stream_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * snprintf() and vsnprintf() of libpok: formatted output into a buffer.
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

/**
 * @funcname `string_stream_emit_character`
 *
 * @description
 * The output function of vsnprintf(): stores a character if there is room (one
 * byte stays for the NUL) and counts it in any case.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 * | `_sstream` | `void *` | [in,out] The buffer and its counts. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `sstream` | `string_stream_t *` | The buffer and its counts |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `sstream` to `_sstream`.
 * 2. If `sstream->bytes_written` is less than `sstream->bytes_max`:
 *    1. Set `sstream->str[sstream->bytes_written]` to `c`.
 * 3. Increment `sstream->bytes_written`.
 *
 * @calls
 * None.
 */
static void string_stream_emit_character(char c, void *_sstream)
{
    string_stream_t* sstream = _sstream;

    if(sstream->bytes_written < sstream->bytes_max) {
        sstream->str[sstream->bytes_written] = c;
    }

    sstream->bytes_written++;
}

/**
 * @funcname `vsnprintf`
 *
 * @description
 * Formatted output into a buffer of a size, NUL-terminated (C standard); the
 * output is cut to the size.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [out] The buffer. |
 * | `n` | `size_t` | [in] Its size. |
 * | `format` | `const char *` | [in] The format. |
 * | `arg` | `va_list` | [in] The variable arguments. |
 *
 * @return The number of characters of the whole output (without the NUL).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `end` | `size_t` | The position of the NUL |
 * | `sstream` | `string_stream_t` | The buffer and its counts |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `sstream.str` to `s`.
 * 2. Set `sstream.bytes_written` to `0U`.
 * 3. Set `sstream.bytes_max` to `(n - 1U)` if `n` is greater than `0U`,
 *    otherwise `0U`.
 * 4. Call `printf_emitter()` with the arguments the address of
 *    `string_stream_emit_character`, the address of `sstream`, `format` and
 *    `arg`.
 * 5. If `n` is greater than `0U`:
 *    1. Set `end` to `sstream.bytes_written` if `sstream.bytes_written` is less
 *       than `sstream.bytes_max`, otherwise `sstream.bytes_max`.
 *    2. Set `s[end]` to `'\0'`.
 * 6. Return `(int) sstream.bytes_written`.
 *
 * @calls
 * - `printf_emitter()`
 */
int vsnprintf(char * s, size_t n, const char * format, va_list arg)
{
    string_stream_t sstream;

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

/**
 * @funcname `snprintf`
 *
 * @description
 * Formatted output into a buffer of a size, NUL-terminated (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `char *` | [out] The buffer. |
 * | `n` | `size_t` | [in] Its size. |
 * | `format` | `const char *` | [in] The format. |
 * | `...` | `...` | [in] The arguments of the conversions. |
 *
 * @return The number of characters of the whole output (without the NUL).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg` | `va_list` | The variable arguments |
 * | `res` | `int` | The number of characters |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `va_start()` with the arguments `arg` and `format`.
 * 2. Call `vsnprintf()` with the arguments `s`, `n`, `format` and `arg`, and
 *    store the result in `res`.
 * 3. Use the macro `va_end()` with the argument `arg`.
 * 4. Return `res`.
 *
 * @calls
 * - `vsnprintf()`
 */
int snprintf(char * s, size_t n, const char * format, ...)
{
    va_list arg;

    va_start(arg, format);
    int res = vsnprintf (s, n, format, arg);
    va_end(arg);

    return res;
}
