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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/include/arch/deployment.h to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; ja_spaces_n is a uint32_t.
 */

/* Definitions which are used in deployment.c */

#ifndef JET_X86_64_DEPLOYMENT_H
#define JET_X86_64_DEPLOYMENT_H

#include <stdint.h>
#include <stddef.h>

/*
 * Description of one user space.
 */
struct ja_x86_64_space
{
    /*
     * Physical address of memory chunk.
     *
     * If it is set to 0 in deployment.c, it will be filled upon space initialization.
     */
    uintptr_t   phys_base;

    /*
     * Size of the memory  (code and static data).
     *
     * Set in deployment.c.
     */
    size_t      size_normal;
    /*
     * Size of the memory  (heap).
     *
     * Set in deployment.c.
     */
    size_t      size_heap;
    /*
     * Size of the memory for stacks.
     *
     * Set in deployment.c.
     */
    size_t size_stack;

    /*
     * Total size for partition's use.
     *
     * Calculated upon space initialization.
     */
    size_t size_total;
    /*
     * Offset from the beginning of the space to the ending of the heap.
     *
     * Calculated upon space initialization.
     */
    size_t heap_end;
    /*
     * Memory currently used for stacks.
     */
    size_t size_stack_used;

    /*
     * Value of CR3 (top-level page table) for this space.
     *
     * Calculated upon space initialization.
     */
    uint64_t cr3;
};

/*
 * Array of user space descriptions.
 *
 * Should be defined in deployment.c.
 */
extern struct ja_x86_64_space ja_spaces[];
extern uint32_t ja_spaces_n;

#endif /* JET_X86_64_DEPLOYMENT_H */
