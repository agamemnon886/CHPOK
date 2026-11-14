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
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: no restrict
 * qualifiers; deviations are documented where a guideline can't be
 * followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the internal
 * vfprintf() is renamed stream_vprintf() (vfprintf is a reserved
 * identifier). Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * printf() and vprintf() of libpok: formatted output to stdout.
 */

/*
 * MISRA C:2025 Rule 21.6 deviation: this file implements printf() and vprintf()
 * of libpok's C library. Rule 17.1: these functions take variable arguments, as
 * in the C standard.
 */
#include <stdio.h>
#include "printf_emitter.h"
#include "stream.h"

/**
 * @funcname `stream_vprintf`
 *
 * @description
 * Formatted output to a stream, as one operation (like C99's vfprintf(), which
 * isn't exposed to the user).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stream` | `FILE *` | [in,out] The stream. |
 * | `format` | `const char *` | [in] The format. |
 * | `arg` | `va_list` | [in] The variable arguments. |
 *
 * @return The number of characters written.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `op` | `stdio_operation_t` | The output operation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `stdio_operation_begin()` with the arguments the address of `op` and
 *    `stream`.
 * 2. Call `printf_emitter()` with the arguments
 *    `stdio_operation_emit_character`, the address of `op`, `format` and `arg`.
 * 3. Call `stdio_operation_end()` with the address of `op`, and return its
 *    result.
 *
 * @calls
 * - `stdio_operation_begin()`
 * - `printf_emitter()`
 * - `stdio_operation_end()`
 */
static int stream_vprintf(FILE * stream, const char * format, va_list arg)
{
    stdio_operation_t op;

    stdio_operation_begin(&op, stream);
    printf_emitter(stdio_operation_emit_character, &op, format, arg);

    return stdio_operation_end(&op);
}

/**
 * @funcname `vprintf`
 *
 * @description
 * Formatted output to stdout (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `format` | `const char *` | [in] The format. |
 * | `arg` | `va_list` | [in] The variable arguments. |
 *
 * @return The number of characters written.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stdout` | `FILE *` | The standard output stream |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `stream_vprintf()` with the arguments `stdout`, `format` and `arg`,
 *    and return its result.
 *
 * @calls
 * - `stream_vprintf()`
 */
int vprintf(const char * format, va_list arg)
{
    return stream_vprintf(stdout, format, arg);
}


/**
 * @funcname `printf`
 *
 * @description
 * Formatted output to stdout (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `format` | `const char *` | [in] The format. |
 * | `...` | `...` | [in] The arguments of the conversions. |
 *
 * @return The number of characters written.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg` | `va_list` | The variable arguments |
 * | `res` | `int` | The number of characters written |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `va_start()` with the arguments `arg` and `format`.
 * 2. Call `vprintf()` with the arguments `format` and `arg`, and store the
 *    result in `res`.
 * 3. Use the macro `va_end()` with the argument `arg`.
 * 4. Return `res`.
 *
 * @calls
 * - `vprintf()`
 */
int printf(const char * format, ...)
{
    va_list arg;

    va_start(arg, format);
    int res = vprintf (format, arg);
    va_end(arg);

    return res;
}
