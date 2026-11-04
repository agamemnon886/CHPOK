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
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/uaccess.c to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized. Long lines are wrapped at 80 columns.
 */

#include <asp/uaccess.h>
#include <assert.h>
#include "space.h"

// TODO: Revisit (see also pok_space_* code in arch.h)
/*
 * MISRA C:2025 Rule 11.6 deviation (this file): user-space addresses are
 * checked against the partition's memory as integers, and translated to
 * kernel addresses by integer arithmetic.
 */
static pok_bool_t x86_64_check_access(const void* JET_USER addr, size_t size,
    jet_space_id space_id)
{
    assert(space_id != 0U);
    assert(size != 0U);

    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + size;

    struct ja_x86_64_space* space = &ja_spaces[space_id - 1U];

    /*
     * Currently, there are 2 segments accessible to user:
     * 1. [POK_PARTITION_MEMORY_BASE; POK_PARTITION_MEMORY_BASE + space->heap_end)
     *    code,data and heap
     * 2. [POK_PARTITION_MEMORY_BASE + space->size_total - space->size_stack_used; POK_PARTITION_MEMORY_BASE + space->size_total)
     *    stacks
     */

    if(end < start) {
        return FALSE; // Segments doesn't cross NULL.
    }

    if(end <= (space->heap_end + POK_PARTITION_MEMORY_BASE))
    {
        return (start >= POK_PARTITION_MEMORY_BASE);
    }
    else if(end <= (POK_PARTITION_MEMORY_BASE + space->size_total))
    {
        return (start >= (POK_PARTITION_MEMORY_BASE + space->size_total
                          - space->size_stack_used));
    }
    else
    {
        return FALSE;
    }
}

/* The kernel accesses user memory through its identity mapping of RAM. */
static uintptr_t user_space_shift(jet_space_id space_id)
{
    assert(space_id != 0U);
    struct ja_x86_64_space* space = &ja_spaces[space_id - 1U];

    return POK_PARTITION_MEMORY_BASE - space->phys_base;
}

void* JET_KUSER ja_user_to_kernel_space(void* JET_USER addr, size_t size,
    jet_space_id space_id)
{
    if(x86_64_check_access(addr, size, space_id)) {
       return (void* JET_KUSER)((uintptr_t)addr - user_space_shift(space_id));
    } else {
       return NULL;
    }
}

const void* JET_KUSER ja_user_to_kernel_ro_space(const void* JET_USER addr,
    size_t size, jet_space_id space_id)
{
    if(x86_64_check_access(addr, size, space_id)) {
       return (const void* JET_KUSER)((uintptr_t)addr
                                      - user_space_shift(space_id));
    } else {
       return NULL;
    }
}

pok_bool_t ja_check_access_exec(void* JET_USER addr, jet_space_id space_id)
{
    assert(space_id != 0U);

    uintptr_t start = (uintptr_t)addr;

    struct ja_x86_64_space* space = &ja_spaces[space_id - 1U];

    /*
     * Only single segment could be executed by user:
     *   [POK_PARTITION_MEMORY_BASE; POK_PARTITION_MEMORY_BASE + space->size_normal)
     *    code and data
     */

    return (start < (POK_PARTITION_MEMORY_BASE + space->size_normal))
        && (start >= POK_PARTITION_MEMORY_BASE);
}
