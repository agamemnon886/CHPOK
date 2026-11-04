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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; essentially Boolean conditions;
 * discarded results are cast to void; includes <stdio.h> for printf() (it
 * relied on the caller); deviations are documented where a guideline
 * can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 */

#ifndef LIBJET_ASSERT_H
#define LIBJET_ASSERT_H

/*
 * MISRA C:2025 Rule 21.6 and Rule 21.8 deviation: as in the C standard, a
 * failed assertion is printed and then aborts the process (libpok's abort()
 * raises an application error).
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
/*
 * MISRA C:2025 Rule 20.10 deviation: the failed expression is printed with
 * the # operator, as the C standard requires for assert().
 */
#define assert(expr) do {                                                   \
    if(!(expr)) {                                                           \
        (void) printf("Assertion failed: `%s', function %s, file %s, "    \
            "line %d\n",                                                \
            #expr, __func__, __FILE__, __LINE__);                           \
        abort();                                                            \
    }} while(false)
#endif

#endif /* LIBJET_ASSERT_H */
