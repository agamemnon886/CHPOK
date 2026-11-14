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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: added pok_time_add(), which
 * saturates instead of overflowing (times and durations from the user).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: the function declarations are
 * hidden from Doxygen (\cond), which would otherwise document the functions a
 * second time, in the section of this header.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the forward typedefs of
 * pok_partition_t and pok_partition_arinc_t are moved here from
 * core/partition.h, so that gdb.h, which core/partition.h includes, can use
 * them; the Doxygen comment of the file is removed (header files are not
 * documented); the Doxygen headers of the static inline functions are plain
 * comments (they are not documented).
 */


#ifndef POK_TYPES_H
#define POK_TYPES_H

#include <uapi/types.h>

/*
 * The partition types, used by pointer before their definitions (in
 * core/partition.h and core/partition_arinc.h, and by headers that
 * core/partition.h includes, such as gdb.h).
 */
typedef struct pok_partition pok_partition_t;
typedef struct pok_partition_arinc pok_partition_arinc_t;

/*
 * Tells whether a time is infinite (negative, such as POK_TIME_INFINITY).
 */
static inline pok_bool_t pok_time_is_infinity(pok_time_t t)
{
    return (t<0);
}

/*
 * Adds a duration to a time that isn't negative, saturating at INT64_MAX (which
 * no time reaches): a duration from the user can't make the sum overflow.
 */
/*
 * time + duration, for a time that is not negative (such as
 * jet_system_time()), saturated at INT64_MAX, which no time reaches: a
 * duration from the user can't make the sum overflow (MISRA C:2025
 * Dir 4.14).
 */
static inline pok_time_t pok_time_add(pok_time_t time, pok_time_t duration)
{
    pok_time_t sum = INT64_MAX;

    if (duration <= (INT64_MAX - time)) {
        sum = time + duration;
    }
    return sum;
}

/* 
 * Compare names.
 * 
 * Name is a array of bytes, which is either of MAX_NAME_LENGTH size
 * without null-bytes, or have null-byte in the first MAX_NAME_LENGTH
 * bytes.
 */
/** \cond */
int   strncasecmp(const char *s1, const char *s2, size_t size);
/** \endcond */
/*
 * Compares two ARINC 653 names (case insensitive, at most MAX_NAME_LENGTH
 * characters, NUL-terminated if shorter).
 */
static inline int32_t pok_compare_names(const char* name1, const char* name2)
{
    return strncasecmp(name1, name2, MAX_NAME_LENGTH);
}

#endif
