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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/uaccess.c for arm64.
 */

#include <asp/uaccess.h>
#include <assert.h>
#include "space.h"

/*
 * The kernel accesses user memory through its identity mapping of RAM,
 * that is by physical address.
 */
static pok_bool_t arm64_check_access(const void* __user addr, size_t size,
    jet_space_id space_id)
{
    assert(space_id != 0);
    assert(size != 0);

    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + size;

    struct ja_arm64_space* space = &ja_spaces[space_id - 1];

    /*
     * Same as on x86, there are 2 areas accessible to user:
     * 1. [POK_PARTITION_MEMORY_BASE; POK_PARTITION_MEMORY_BASE + space->heap_end)
     *    code,data and heap
     * 2. [POK_PARTITION_MEMORY_BASE + space->size_total - space->size_stack_used; POK_PARTITION_MEMORY_BASE + space->size_total)
     *    stacks
     */

    if(end < start) return FALSE; // Area doesn't wrap around.

    if(end <= space->heap_end + POK_PARTITION_MEMORY_BASE)
    {
        return (start >= POK_PARTITION_MEMORY_BASE);
    }
    else if(end <= POK_PARTITION_MEMORY_BASE + space->size_total)
    {
        return (start >= POK_PARTITION_MEMORY_BASE + space->size_total - space->size_stack_used);
    }
    else
    {
        return FALSE;
    }
}

static void* __kuser user_to_kernel(const void* __user addr, jet_space_id space_id)
{
    struct ja_arm64_space* space = &ja_spaces[space_id - 1];

    return (void* __kuser)((uintptr_t)addr - POK_PARTITION_MEMORY_BASE + space->phys_base);
}

void* __kuser ja_user_to_kernel_space(void* __user addr, size_t size,
    jet_space_id space_id)
{
    if(arm64_check_access(addr, size, space_id))
       return user_to_kernel(addr, space_id);
    else
       return NULL;
}

const void* __kuser ja_user_to_kernel_ro_space(const void* __user addr,
    size_t size, jet_space_id space_id)
{
    if(arm64_check_access(addr, size, space_id))
       return user_to_kernel(addr, space_id);
    else
       return NULL;
}

pok_bool_t ja_check_access_exec(void* __user addr, jet_space_id space_id)
{
    assert(space_id != 0);

    uintptr_t start = (uintptr_t)addr;

    struct ja_arm64_space* space = &ja_spaces[space_id - 1];

    /*
     * Only code and data could be executed by user:
     *   [POK_PARTITION_MEMORY_BASE; POK_PARTITION_MEMORY_BASE + space->size_normal)
     */

    return (start < POK_PARTITION_MEMORY_BASE + space->size_normal)
        && (start >= POK_PARTITION_MEMORY_BASE);
}
