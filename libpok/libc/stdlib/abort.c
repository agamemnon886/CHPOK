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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: discarded
 * results are cast to void.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
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
 * abort() of libpok.
 */

#include <stdlib.h>
#include <core/syscall.h>

/**
 * @funcname `abort`
 *
 * @description
 * Aborts the process (C standard): raises an application error.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `abort_message` | `static const char[8]` | The message of the error |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_error_raise_application_error()` with the arguments
 *    `abort_message` and `sizeof(abort_message)`.
 *
 * @calls
 * - `pok_error_raise_application_error()`
 */
void abort(void)
{
    static const char abort_message[] = "Aborted";

    (void) pok_error_raise_application_error (abort_message,
                                              sizeof(abort_message));
}
