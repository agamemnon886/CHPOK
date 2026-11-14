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
 * assert_os() of libpok: a false assertion raises an OS error of the
 * partition.
 */

#include <core/assert_os.h>

#include <core/syscall.h>
/*
 * MISRA C:2025 Rule 21.6 deviation: snprintf() is libpok's own formatting
 * function; no streams or files are used.
 */
#include <stdio.h>

/**
 * @funcname `assertion_os_fail`
 *
 * @description
 * Reports a false assertion (assert_os()): raises an OS error with a message
 * that names the expression, file and line.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `expression` | `const char *` | [in] The text of the expression. |
 * | `file` | `const char *` | [in] The source file. |
 * | `line` | `int32_t` | [in] The line. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `msg` | `static char[128]` | The message |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `snprintf()` with the arguments `msg`, `sizeof(msg)`,
 *    `"Assertion failed (%%s) in %%s:%%d\n"`, `expression`, `file` and `line`.
 * 2. Call `pok_error_raise_os_error()` with the arguments `msg` and
 *    `sizeof(msg)`.
 *
 * @calls
 * - `snprintf()`
 * - `pok_error_raise_os_error()`
 */
void assertion_os_fail(const char *expression, const char *file, int32_t line)
{
    static char msg[128];
    
    (void) snprintf(msg, sizeof(msg), "Assertion failed (%s) in %s:%d\n",
        expression, file, line);
    
    (void) pok_error_raise_os_error(msg, sizeof(msg));
}
