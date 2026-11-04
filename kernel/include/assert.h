 /* 
 *  Copyright (C) 2014 Maxim Malkov, ISPRAS <malkov@ispras.ru> 
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; discarded results are cast to void;
 * deviations are documented where a guideline can't be followed;
 * unreachable() calls the _Noreturn pok_assertion_fail() directly.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Long lines are wrapped at 80 columns.
 */

#ifndef POK_KERNEL_ASSERT_H
#define POK_KERNEL_ASSERT_H

#include <core/debug.h>

/*
 * MISRA C:2025 Dir 1.2 deviation: _Noreturn is a C11 keyword, accepted by GCC
 * in the gnu99 mode used here. Calls to non-returning functions terminate
 * switch-clauses (Rule 16.3).
 */
_Noreturn static inline void pok_assertion_fail(const char *expression,
                                                const char *file, int32_t line)
{
    (void) printf("Assertion failed (%s) in %s:%d\n", expression, file, line);
    pok_fatal("");
}

#ifndef NDEBUG
/*
 * MISRA C:2025 Rule 20.10 deviation: the failed expression is printed with
 * the # operator, as for the standard assert().
 */
    #define assert(expr) ( \
        (expr) ? \
        ((void) 0) : \
        pok_assertion_fail(#expr, __FILE__, __LINE__) \
    )
#else
    #define assert(expr) do {} while (0)
#endif

/* Reports an unexpected code path; doesn't return, also with NDEBUG. */
#define unreachable() pok_assertion_fail("unreachable", __FILE__, __LINE__)

#endif
