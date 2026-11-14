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
 * kernel/arch/x86/space.c to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * discarded results are cast to void; internal linkage for file-local
 * definitions; writing CR3 is a small inline-assembly function; the
 * initial FPU control words are stored byte by byte instead of through
 * cast pointers; the number of spaces is unsigned; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: memory_blocks_init()
 * doesn't modify its parameter; operands of mixed precedence are parenthesized;
 * deviations are documented where a guideline can't be followed. Long lines are
 * wrapped at 80 columns. ja_ustack_alloc() compares the user's stack size with
 * the free space before aligning it, so that the sum can't wrap around.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: struct jet_fp_store is moved
 * to space.h, so that the GDB server can read and write it.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: ja_x86_64_space_t, jet_fp_store_t,
 * jet_kernel_shared_t, jet_space_layout_t, mb_config_rights_t, memory_block_t;
 * the macros and the declaration of ja_x86_64_user_jump() are moved to
 * kernel/arch/x86_64/space.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * mb_config_rights_e (was mb_config_rights_t).
 */

/**
 * @file
 * User spaces on x86_64.
 *
 * Every space has its own page tables (4-level paging, 4K pages). They
 * contain:
 *  - identity mapping of RAM with 2 MiB pages, accessible only from the
 *    kernel (global, shared by all spaces);
 *  - for user spaces, partition's memory mapped at
 *    POK_PARTITION_MEMORY_BASE, accessible from user space.
 *
 * Space 0 (kernel) has only the identity mapping.
 *
 * Memory blocks are backed by RAM allocated after partitions' memory
 * and mapped into the spaces which have access to them, read-only or
 * read-write. (As on arm64, the configured PhysicalAddress is not used.)
 *
 * The file also has the user stacks, space switches and the FP
 * registers.
 */

#include <types.h>
#include <errno.h>
#include <libc.h>
#include <assert.h>

#include <asp/arch.h>
#include <asp/alloc.h>
#include <asp/space.h>
#include <arch/deployment.h>
#include <core/debug.h>
#include <core/memblocks_config.h>

#include "gdt.h"
#include "space.h"
#include <asp/memory.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the allocator returns its
 * memory as a pointer to void, which is converted to the type of the object
 * allocated.
 */

/* Page directory with the kernel's identity mapping of RAM. */
static uint64_t* kernel_pd;
/* Top-level table of the kernel (space 0). */
static uint64_t kernel_cr3;

/**
 * @funcname `cpu_write_cr3`
 *
 * @description
 * Writes CR3: switches to a top-level page table.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cr3` | `uint64_t` | [in] The physical address of the table. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `mov %0, %%%cr3`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.4 and Rule 11.6 deviation (this file): page table
 * entries hold physical addresses, which are converted to pointers to walk
 * and fill the tables (the kernel maps physical memory one-to-one), and the
 * memory of partitions and memory blocks is described by physical addresses.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (cpu_write_cr3(), ja_fp_save() and
 * ja_fp_restore()): privileged
 * instructions are written as GNU inline assembly, each isolated in a small
 * function.
 */
static void cpu_write_cr3(uint64_t cr3)
{
    asm volatile ("mov %0, %%cr3" : : "r" (cr3) : "memory");
}

/**
 * @funcname `table_alloc`
 *
 * @description
 * Allocates a zeroed page table from the kernel heap.
 *
 * @return The table.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `table` | `uint64_t *` | The table |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `ja_mem_alloc_aligned()` with the arguments `PAGE_SIZE` and
 *    `PAGE_SIZE`, and store the result in `table`.
 * 2. Call `memset()` with the arguments `table`, `0` and `PAGE_SIZE`.
 * 3. Return `table`.
 *
 * @calls
 * - `ja_mem_alloc_aligned()`
 * - `memset()`
 */
static uint64_t* table_alloc(void)
{
    uint64_t* table = ja_mem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
    (void) memset(table, 0, PAGE_SIZE);
    return table;
}

/**
 * @funcname `pml4_create`
 *
 * @description
 * Allocates a top-level page table with the kernel's identity mapping of RAM.
 *
 * @return The table.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kernel_pd` | `uint64_t *` | The page directory of the kernel's identity mapping of RAM |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pdpt` | `uint64_t *` | Its page directory pointer table |
 * | `pml4` | `uint64_t *` | The top-level table |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `table_alloc()`, and store the result in `pml4`.
 * 2. Call `table_alloc()`, and store the result in `pdpt`.
 * 3. Set `pml4[0]` to `(uint64_t)pdpt | PTE_TABLE_USER`.
 * 4. Set `pdpt[0]` to `(uint64_t)kernel_pd | PTE_TABLE_KERNEL`.
 * 5. Return `pml4`.
 *
 * @calls
 * - `table_alloc()`
 */
static uint64_t* pml4_create(void)
{
    uint64_t* pml4 = table_alloc();
    uint64_t* pdpt = table_alloc();

    pml4[0] = (uint64_t)pdpt | PTE_TABLE_USER;
    pdpt[0] = (uint64_t)kernel_pd | PTE_TABLE_KERNEL;

    return pml4;
}

/**
 * @funcname `table_next`
 *
 * @description
 * Returns the next-level page table that an entry points to, creating it if
 * needed; an entry of the kernel's mapping is fatal.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `entry` | `uint64_t *` | [in,out] The entry. |
 *
 * @return The next-level table.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If the value at `entry` is equal to `0U`:
 *    1. Set `*entry` to `(uint64_t)table_alloc() | PTE_TABLE_USER`.
 * 2. Otherwise, if `(*entry & PTE_US)` is equal to `0U`:
 *    1. Call `pok_fatal()` with the argument
 *       `"User mapping overlaps with the kernel's one"`.
 * 3. Otherwise:
 *    1. Do nothing.
 * 4. Return `(uint64_t*)(*entry & PTE_ADDR_MASK)`.
 *
 * @calls
 * - `table_alloc()`
 * - `pok_fatal()`
 */
static uint64_t* table_next(uint64_t* entry)
{
    if (*entry == 0U) {
        *entry = (uint64_t)table_alloc() | PTE_TABLE_USER;
    } else if ((*entry & PTE_US) == 0U) {
        pok_fatal("User mapping overlaps with the kernel's one");
    } else {
        // The entry already points to a table.
    }

    return (uint64_t*)(*entry & PTE_ADDR_MASK);
}

/**
 * @funcname `map_user_page`
 *
 * @description
 * Maps a user page at a virtual address to a physical address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pml4` | `uint64_t *` | [in,out] The top-level table of the space. |
 * | `va` | `uint64_t` | [in] The virtual address. |
 * | `pa` | `uint64_t` | [in] The physical address. |
 * | `writable` | `pok_bool_t` | [in] Whether it may be written. |
 * | `exec` | `pok_bool_t` | [in] Whether it may be executed. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pd` | `uint64_t *` | The page directory |
 * | `pdpt` | `uint64_t *` | The page directory pointer table |
 * | `pt` | `uint64_t *` | The page table |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `table_next()` with the address of
 *    `pml4[TABLE_INDEX(va, PML4_SHIFT)]`, and store the result in `pdpt`.
 * 2. Call `table_next()` with the address of
 *    `pdpt[TABLE_INDEX(va, PDPT_SHIFT)]`, and store the result in `pd`.
 * 3. Call `table_next()` with the address of `pd[TABLE_INDEX(va, PD_SHIFT)]`,
 *    and store the result in `pt`.
 * 4. Set `pt[TABLE_INDEX(va, PT_SHIFT)]` to
 *    `pa | PTE_P | PTE_US | (writable ? PTE_RW : 0U) | (exec ? 0U : PTE_NX)`.
 *
 * @calls
 * - `table_next()`
 */
static void map_user_page(uint64_t* pml4, uint64_t va, uint64_t pa,
    pok_bool_t writable, pok_bool_t exec)
{
    uint64_t* pdpt = table_next(&pml4[TABLE_INDEX(va, PML4_SHIFT)]);
    uint64_t* pd = table_next(&pdpt[TABLE_INDEX(va, PDPT_SHIFT)]);
    uint64_t* pt = table_next(&pd[TABLE_INDEX(va, PD_SHIFT)]);

    pt[TABLE_INDEX(va, PT_SHIFT)] = pa | PTE_P | PTE_US
        | (writable ? PTE_RW : 0U)
        | (exec ? 0U : PTE_NX);
}

/**
 * @funcname `memory_blocks_init`
 *
 * @description
 * Allocates RAM for the memory blocks after the partitions and maps each block
 * into the spaces that have access to it (read-only or read-write); a block at
 * a bad virtual address is not mapped.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `phys_first` | `uintptr_t` | [in] The first free physical address. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 * | `ja_spaces_n` | `uint32_t` | Their number |
 * | `jet_memory_blocks` | `memory_block_t[]` | The memory blocks of the configuration |
 * | `jet_memory_blocks_n` | `size_t` | Their number |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of a block |
 * | `mblock` | `memory_block_t *` | The block |
 * | `offset` | `size_t` | The offset of a page in the block |
 * | `phys` | `uintptr_t` | Its physical address |
 * | `phys_start` | `uintptr_t` | The next free physical address |
 * | `pml4` | `uint64_t *` | The space's top-level table |
 * | `rights` | `mb_config_rights_e` | The space's access to the block |
 * | `size` | `size_t` | Its size, rounded up to pages |
 * | `space_id` | `uint32_t` | A space |
 * | `virt` | `uint64_t` | Its virtual address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `phys_start` to `phys_first`.
 * 2. Set `i` to `0`; then while `i` is less than `jet_memory_blocks_n`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `mblock` to the address of `jet_memory_blocks[i]`.
 *    2. Set `virt` to `mblock->virt_addr`.
 *    3. Call `ALIGN_VAL()` with the arguments `mblock->size` and `PAGE_SIZE`,
 *       and store the result in `size`.
 *    4. Call `ALIGN_VAL()` with the arguments `phys_start` and `PAGE_SIZE`, and
 *       store the result in `phys`.
 *    5. If `(virt & (PAGE_SIZE - 1U))` is not equal to `0U` or `virt` is less
 *       than `(POK_PARTITION_MEMORY_BASE + POK_PARTITION_MEMORY_MAX_SIZE)`:
 *       1. Call `printf()` with the arguments
 *          `"Memory block '%%s' at 0x%%lx is not mapped: virtual address "`
 *          `"should be page-aligned and not below 0x%%llx\n"`, `mblock->name`,
 *          `(unsigned long)virt` and
 *          `POK_PARTITION_MEMORY_BASE + POK_PARTITION_MEMORY_MAX_SIZE`.
 *       2. Go on with the next pass of the loop.
 *    6. If `(phys + size)` is greater than `BOARD_RAM_SIZE`:
 *       1. Call `pok_fatal()` with the argument
 *          `"Not enough memory for memory blocks"`.
 *    7. Call `memset()` with the arguments `(void*)phys`, `0` and `size`.
 *    8. Set `phys_start` to `phys + size`.
 *    9. Set `space_id` to `1U`; then while `space_id` is less than or equal to
 *       `ja_spaces_n` and `space_id` is less than `MAX_PID`, repeat (after each
 *       pass: increment `space_id`):
 *       1. Set `rights` to `mblock->pid_to_rights[space_id]`.
 *       2. If `rights` is equal to `MB_CONFIG_NO_ACCESS`:
 *          1. Go on with the next pass of the loop.
 *       3. Set `pml4` to `(uint64_t*)ja_spaces[space_id - 1U].cr3`.
 *       4. Set `offset` to `0U`; then while `offset` is less than `size`,
 *          repeat (after each pass: add `PAGE_SIZE` to `offset`):
 *          1. Call `map_user_page()` with the arguments `pml4`,
 *             `virt + offset`, `phys + offset`,
 *             `rights == MB_CONFIG_READ_WRITE` and `FALSE`.
 *
 * @calls
 * - `ALIGN_VAL()`
 * - `printf()`
 * - `pok_fatal()`
 * - `memset()`
 * - `map_user_page()`
 */
static void memory_blocks_init(uintptr_t phys_first)
{
    uintptr_t phys_start = phys_first; // Next free physical address.

    for (size_t i = 0; i < jet_memory_blocks_n; i++)
    {
        memory_block_t* mblock = &jet_memory_blocks[i];
        uint64_t virt = mblock->virt_addr;
        size_t size = ALIGN_VAL(mblock->size, PAGE_SIZE);
        uintptr_t phys = ALIGN_VAL(phys_start, PAGE_SIZE);

        if (((virt & (PAGE_SIZE - 1U)) != 0U)
            || (virt
                < (POK_PARTITION_MEMORY_BASE + POK_PARTITION_MEMORY_MAX_SIZE)))
        {
            // As before on (32-bit) x86, access to such a block will fail.
            (void) printf(
                "Memory block '%s' at 0x%lx is not mapped: virtual address "
                "should be page-aligned and not below 0x%llx\n",
                mblock->name, (unsigned long)virt,
                POK_PARTITION_MEMORY_BASE + POK_PARTITION_MEMORY_MAX_SIZE);
            continue;
        }

        if ((phys + size) > BOARD_RAM_SIZE) {
            pok_fatal("Not enough memory for memory blocks");
        }

        (void) memset((void*)phys, 0, size);
        phys_start = phys + size;

        for (uint32_t space_id = 1U; (space_id <= ja_spaces_n)
             && (space_id < MAX_PID); space_id++)
        {
            mb_config_rights_e rights = mblock->pid_to_rights[space_id];
            if (rights == MB_CONFIG_NO_ACCESS) {
                continue;
            }

            uint64_t* pml4 = (uint64_t*)ja_spaces[space_id - 1U].cr3;
            for (size_t offset = 0U; offset < size; offset += PAGE_SIZE)
            {
                map_user_page(pml4, virt + offset, phys + offset,
                    rights == MB_CONFIG_READ_WRITE, FALSE);
            }
        }
    }
}

/**
 * @funcname `ja_space_layout_get`
 *
 * @description
 * Returns the layout of a space: its kernel and user addresses and size.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The space (not 0). |
 * | `space_layout` | `jet_space_layout_t *` | [out] Where the layout is stored. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 * | `ja_spaces_n` | `uint32_t` | Their number |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Check that `(space_id != 0U) && (space_id <= ja_spaces_n)` holds (assert:
 *    stop otherwise).
 * 2. Set `space_layout->kernel_addr` to
 *    `(char*)ja_spaces[space_id - 1U].phys_base`.
 * 3. Set `space_layout->user_addr` to
 *    `(char* JET_USER)POK_PARTITION_MEMORY_BASE`.
 * 4. Set `space_layout->size` to `ja_spaces[space_id - 1U].size_normal`.
 *
 * @calls
 * - `pok_assertion_fail()`
 */
void ja_space_layout_get(jet_space_id space_id,
    jet_space_layout_t* space_layout)
{
    assert((space_id != 0U) && (space_id <= ja_spaces_n));

    space_layout->kernel_addr = (char*)ja_spaces[space_id - 1U].phys_base;
    space_layout->user_addr = (char* JET_USER)POK_PARTITION_MEMORY_BASE;
    space_layout->size = ja_spaces[space_id - 1U].size_normal;
}

/**
 * @funcname `ja_space_shared_data`
 *
 * @description
 * Returns the kernel address of the data that the kernel shares with a space
 * (at the start of its memory).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The space (not 0). |
 *
 * @return The shared data.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space` | `ja_x86_64_space_t *` | The space |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `space` to the address of `ja_spaces[space_id - 1U]`.
 * 2. Return `(jet_kernel_shared_t* JET_KUSER)space->phys_base`.
 *
 * @calls
 * None.
 */
jet_kernel_shared_t* JET_KUSER ja_space_shared_data(
    jet_space_id space_id)
{
    ja_x86_64_space_t* space = &ja_spaces[space_id - 1U];
    return (jet_kernel_shared_t* JET_KUSER)space->phys_base;
}

/**
 * @funcname `ja_space_get_heap`
 *
 * @description
 * Returns the user address of a space's heap.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The space (not 0). |
 *
 * @return The heap.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space` | `ja_x86_64_space_t *` | The space |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `space` to the address of `ja_spaces[space_id - 1U]`.
 * 2. Return `(void JET_USER*)(POK_PARTITION_MEMORY_BASE + space->heap_end -`
 *    `space->size_heap)`.
 *
 * @calls
 * None.
 */
void JET_USER* ja_space_get_heap(jet_space_id space_id)
{
   ja_x86_64_space_t* space = &ja_spaces[space_id - 1U];

   return (void JET_USER*)(POK_PARTITION_MEMORY_BASE + space->heap_end
                           - space->size_heap);
}


static jet_space_id current_space_id = 0;

/**
 * @funcname `ja_user_space_jump`
 *
 * @description
 * Enters user space in a space: sets the kernel stack of interrupts and jumps
 * to the entry (ja_x86_64_user_jump() in cswitch.S).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stack_kernel` | `jet_stack_t` | [in] The kernel stack of the thread. |
 * | `space_id` | `jet_space_id` | [in] The space (already switched to). |
 * | `entry_user` | `void (JET_USER *)(void)` | [in] The user entry. |
 * | `stack_user` | `jet_ustack_t` | [in] The user stack. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces_n` | `uint32_t` | The number of memory spaces |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Check that `(space_id != 0U) && (space_id <= ja_spaces_n)` holds (assert:
 *    stop otherwise).
 * 2. Call `ja_tss_set_rsp0()` with the argument `stack_kernel`.
 * 3. Call `ja_x86_64_user_jump()` with the arguments `stack_kernel`,
 *    `entry_user` and `stack_user`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `ja_tss_set_rsp0()`
 * - `ja_x86_64_user_jump()`
 */
void ja_user_space_jump(
    jet_stack_t stack_kernel,
    jet_space_id space_id,
    void (JET_USER * entry_user)(void),
    jet_ustack_t stack_user)
{
   assert((space_id != 0U) && (space_id <= ja_spaces_n));

   ja_tss_set_rsp0 (stack_kernel);

   ja_x86_64_user_jump(stack_kernel, entry_user, stack_user);
}

/**
 * @funcname `ja_space_init`
 *
 * @description
 * Sets up the memory at boot: the kernel's identity mapping of RAM with large
 * pages, the layout and physical memory of each space (code and data, heap,
 * stacks) with its page tables, the memory blocks; then switches to the
 * kernel's table.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 * | `ja_spaces_n` | `uint32_t` | Their number |
 * | `kernel_cr3` | `uint64_t` | The top-level table of the kernel (space 0) |
 * | `kernel_pd` | `uint64_t *` | The page directory of the kernel's identity mapping of RAM |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a space |
 * | `offset` | `size_t` | The offset of a page in the space |
 * | `pa` | `uint64_t` | A physical address of the identity mapping |
 * | `phys_base_end` | `uintptr_t` | The end of a space at a configured address |
 * | `phys_start` | `uintptr_t` | The next free physical address |
 * | `pml4` | `uint64_t *` | The space's top-level table |
 * | `size_total` | `size_t` | The size of the space so far |
 * | `space` | `ja_x86_64_space_t *` | The space |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `phys_start` to `POK_PARTITION_MEMORY_PHYS_START`.
 * 2. Call `table_alloc()`, and store the result in `kernel_pd`.
 * 3. Set `pa` to `0`; then while `pa` is less than `BOARD_RAM_SIZE`, repeat
 *    (after each pass: add `LARGE_PAGE_SIZE` to `pa`):
 *    1. Set `kernel_pd[TABLE_INDEX(pa, PD_SHIFT)]` to
 *       `pa | PTE_P | PTE_RW | PTE_PS | PTE_G`.
 * 4. Set `kernel_cr3` to `(uint64_t)pml4_create()`.
 * 5. Set `i` to `0U`; then while `i` is less than `ja_spaces_n`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `space` to the address of `ja_spaces[i]`.
 *    2. Set `size_total` to `space->size_normal`.
 *    3. If `space->size_heap` is greater than `0U`:
 *       1. Set `size_total` to `ALIGN_VAL(size_total, 16) + space->size_heap`.
 *    4. Set `space->heap_end` to `size_total`.
 *    5. Set `size_total` to `ALIGN_VAL(size_total, 16) + space->size_stack`.
 *    6. Call `ALIGN_VAL()` with the arguments `size_total` and `PAGE_SIZE`, and
 *       store the result in `space->size_total`.
 *    7. If `space->phys_base` is equal to `0U`:
 *       1. Call `ALIGN_VAL()` with the arguments `phys_start` and `PAGE_SIZE`,
 *          and store the result in `space->phys_base`.
 *       2. Set `phys_start` to `space->phys_base + space->size_total`.
 *    8. Otherwise:
 *       1. Check that `space->phys_base >= POK_PARTITION_MEMORY_PHYS_START`
 *          holds (assert: stop otherwise).
 *       2. Check that `(space->phys_base & (PAGE_SIZE - 1U)) == 0U` holds
 *          (assert: stop otherwise).
 *       3. Set `phys_base_end` to `space->phys_base + space->size_total`.
 *       4. If `phys_start` is less than `phys_base_end`:
 *          1. Set `phys_start` to `phys_base_end`.
 *    9. If `space->size_total` is greater than `POK_PARTITION_MEMORY_MAX_SIZE`
 *       or `(space->phys_base + space->size_total)` is greater than
 *       `BOARD_RAM_SIZE`:
 *       1. Call `printf()` with the arguments
 *          `"Not enough memory for space %%u (0x%%zx bytes at 0x%%lx)\n"`,
 *          `i + 1U`, `space->size_total` and `(unsigned long)space->phys_base`.
 *       2. Call `pok_fatal()` with the argument
 *          `"Not enough memory for partitions"`.
 *    10. Call `pml4_create()`, and store the result in `pml4`.
 *    11. Set `offset` to `0U`; then while `offset` is less than
 *        `space->size_total`, repeat (after each pass: add `PAGE_SIZE` to
 *        `offset`):
 *        1. Call `map_user_page()` with the arguments `pml4`,
 *           `POK_PARTITION_MEMORY_BASE + offset`, `space->phys_base + offset`,
 *           `TRUE` and `offset < space->size_normal`.
 *    12. Set `space->cr3` to `(uint64_t)pml4`.
 * 6. Call `memory_blocks_init()` with the argument `phys_start`.
 * 7. Call `cpu_write_cr3()` with the argument `kernel_cr3`.
 *
 * @calls
 * - `table_alloc()`
 * - `pml4_create()`
 * - `ALIGN_VAL()`
 * - `pok_assertion_fail()`
 * - `printf()`
 * - `pok_fatal()`
 * - `map_user_page()`
 * - `memory_blocks_init()`
 * - `cpu_write_cr3()`
 */
void ja_space_init(void)
{
    uintptr_t phys_start = POK_PARTITION_MEMORY_PHYS_START;

    kernel_pd = table_alloc();
    for (uint64_t pa = 0; pa < BOARD_RAM_SIZE; pa += LARGE_PAGE_SIZE) {
        kernel_pd[TABLE_INDEX(pa, PD_SHIFT)] = pa | PTE_P | PTE_RW | PTE_PS | PTE_G;
    }

    kernel_cr3 = (uint64_t)pml4_create();

    for(uint32_t i = 0U; i < ja_spaces_n; i++)
    {
        ja_x86_64_space_t* space = &ja_spaces[i];

        /*
         * Code and data segments should be aligned on 4k;
         */
        size_t size_total = space->size_normal;

        if(space->size_heap > 0U)
        {
            /* Heap should be aligned on 16; (why?) */
            size_total = ALIGN_VAL(size_total, 16) + space->size_heap;
        }

        // Store intermediate result.
        space->heap_end =  size_total;

        /* Stack should be aligned on 16; (why?) */
        size_total = ALIGN_VAL(size_total, 16) + space->size_stack;

        /* Such a way, next space will have alignment suitable for code and data. */
        space->size_total = ALIGN_VAL(size_total, PAGE_SIZE);

        if(space->phys_base == 0U)
        {
            space->phys_base = ALIGN_VAL(phys_start, PAGE_SIZE);
            phys_start = space->phys_base + space->size_total;
        }
        else
        {
            assert(space->phys_base >= POK_PARTITION_MEMORY_PHYS_START);
            assert((space->phys_base & (PAGE_SIZE - 1U)) == 0U);
            uintptr_t phys_base_end = space->phys_base + space->size_total;
            if(phys_start < phys_base_end) {
                phys_start = phys_base_end;
            }
        }

        if((space->size_total > POK_PARTITION_MEMORY_MAX_SIZE)
            || ((space->phys_base + space->size_total) > BOARD_RAM_SIZE))
        {
            (void) printf(
                "Not enough memory for space %u (0x%zx bytes at 0x%lx)\n",
                i + 1U, space->size_total, (unsigned long)space->phys_base);
            pok_fatal("Not enough memory for partitions");
        }

        uint64_t* pml4 = pml4_create();
        for(size_t offset = 0U; offset < space->size_total; offset += PAGE_SIZE)
        {
            // Only code and data may be executed.
            map_user_page(pml4, POK_PARTITION_MEMORY_BASE + offset,
                space->phys_base + offset, TRUE, offset < space->size_normal);
        }

        space->cr3 = (uint64_t)pml4;
    }

    memory_blocks_init(phys_start);

    cpu_write_cr3(kernel_cr3);
}

/**
 * @funcname `ja_ustack_init`
 *
 * @description
 * Frees every user stack of a space (partition restart).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The space (not 0). |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space` | `ja_x86_64_space_t *` | The space |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Check that `space_id != 0U` holds (assert: stop otherwise).
 * 2. Set `space` to the address of `ja_spaces[space_id - 1U]`.
 * 3. Set `space->size_stack_used` to `0`.
 *
 * @calls
 * - `pok_assertion_fail()`
 */
void ja_ustack_init (jet_space_id space_id)
{
    assert(space_id != 0U);

    ja_x86_64_space_t* space = &ja_spaces[space_id - 1U];

    space->size_stack_used = 0;
}

/**
 * @funcname `ja_ustack_alloc`
 *
 * @description
 * Allocates a user stack in a space's stack area (from the top down, 16-byte
 * aligned).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The space (not 0). |
 * | `stack_size` | `size_t` | [in] The size (from the user). |
 *
 * @return The head of the stack, or 0 if there is no room.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `result` | `jet_ustack_t` | The head of the stack |
 * | `size_stack_free` | `size_t` | The free stack area |
 * | `size_stack_new` | `size_t` | The stack area used after the allocation |
 * | `space` | `ja_x86_64_space_t *` | The space |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Check that `space_id != 0U` holds (assert: stop otherwise).
 * 2. Set `space` to the address of `ja_spaces[space_id - 1U]`.
 * 3. Set `size_stack_free` to `space->size_stack - space->size_stack_used`.
 * 4. If `stack_size` is greater than `(size_stack_free & ~(size_t)15U)`:
 *    1. Return `0`.
 * 5. Set `size_stack_new` to
 *    `space->size_stack_used + ALIGN_VAL(stack_size, 16)`.
 * 6. Set `result` to
 *    `POK_PARTITION_MEMORY_BASE + space->size_total - space->size_stack_used`.
 * 7. Set `space->size_stack_used` to `size_stack_new`.
 * 8. Return `result`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `ALIGN_VAL()`
 */
jet_ustack_t ja_ustack_alloc (jet_space_id space_id, size_t stack_size)
{
    assert(space_id != 0U);

    ja_x86_64_space_t* space = &ja_spaces[space_id - 1U];

    /*
     * stack_size comes from the user (CREATE_PROCESS): it is compared with
     * the free space before it is aligned, so that the sum can't wrap
     * around.
     */
    size_t size_stack_free = space->size_stack - space->size_stack_used;
    if(stack_size > (size_stack_free & ~(size_t)15U)) {
        return 0;
    }

    size_t size_stack_new = space->size_stack_used + ALIGN_VAL(stack_size, 16);

    jet_ustack_t result = POK_PARTITION_MEMORY_BASE + space->size_total - space->size_stack_used;

    space->size_stack_used = size_stack_new;

    return result;
}

/**
 * @funcname `ja_space_switch`
 *
 * @description
 * Switches to a space's page tables (0: the kernel's), if it isn't the current
 * one.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_space_id` | `jet_space_id` | The current space |
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 * | `kernel_cr3` | `uint64_t` | The top-level table of the kernel (space 0) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cr3` | `uint64_t` | The space's top-level table |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `space_id` is equal to `current_space_id`:
 *    1. Return.
 * 2. Set `cr3` to `ja_spaces[space_id - 1U].cr3` if `space_id` is not equal to
 *    `0U`, otherwise `kernel_cr3`.
 * 3. Call `cpu_write_cr3()` with the argument `cr3`.
 * 4. Set `current_space_id` to `space_id`.
 *
 * @calls
 * - `cpu_write_cr3()`
 */
void ja_space_switch (jet_space_id space_id)
{
    if(space_id == current_space_id) {
        return;
    }

    // Kernel's mappings are global, so they stay in the TLB.
    uint64_t cr3 = (space_id != 0U) ? ja_spaces[space_id - 1U].cr3 : kernel_cr3;
    cpu_write_cr3(cr3);

    current_space_id = space_id;
}

/**
 * @funcname `ja_space_get_current`
 *
 * @description
 * Returns the current space.
 *
 * @return The space (0: the kernel).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_space_id` | `jet_space_id` | The current space |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `current_space_id`.
 *
 * @calls
 * None.
 */
jet_space_id ja_space_get_current (void)
{
    return current_space_id;
}

/**
 * @funcname `ja_alloc_fp_store`
 *
 * @description
 * Allocates a zeroed FP store (16-byte aligned, for fxsave64). May be
 * called only during OS init.
 *
 * @return The FP store.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `res` | `jet_fp_store_t *` | The FP store |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `ja_mem_alloc_aligned()` with the arguments `sizeof(*res)` and `16`,
 *    and store the result in `res`.
 * 2. Call `memset()` with the arguments `res`, `0` and `sizeof(*res)`.
 * 3. Return `res`.
 *
 * @calls
 * - `ja_mem_alloc_aligned()`
 * - `memset()`
 */
jet_fp_store_t* ja_alloc_fp_store(void)
{
    jet_fp_store_t* res = ja_mem_alloc_aligned(sizeof(*res), 16);

    (void) memset(res, 0, sizeof(*res));

    return res;
}

/**
 * @funcname `ja_fp_save`
 *
 * @description
 * Saves the FP and SSE registers (fxsave64).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp_store` | `jet_fp_store_t *` | [in] Where they are saved. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `fxsave64 %0`.
 *
 * @calls
 * None.
 */
void ja_fp_save(jet_fp_store_t* fp_store)
{
    asm volatile ("fxsave64 %0" : "=m" (*fp_store));
}

/**
 * @funcname `ja_fp_restore`
 *
 * @description
 * Restores the FP and SSE registers (fxrstor64).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp_store` | `jet_fp_store_t *` | [in] Where they are restored from. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `fxrstor64 %0`.
 *
 * @calls
 * None.
 */
void ja_fp_restore(jet_fp_store_t* fp_store)
{
    asm volatile ("fxrstor64 %0" : : "m" (*fp_store));
}

/**
 * @funcname `ja_fp_init`
 *
 * @description
 * Initializes the FP and SSE registers: the control words at their reset
 * values, all registers zero.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp_init_store` | `static jet_fp_store_t` | The initial FP state |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `fp_init_store.data[0]` to `0x7fU`.
 * 2. Set `fp_init_store.data[1]` to `0x03U`.
 * 3. Set `fp_init_store.data[24]` to `0x80U`.
 * 4. Set `fp_init_store.data[25]` to `0x1fU`.
 * 5. Call `ja_fp_restore()` with the address of `fp_init_store`.
 *
 * @calls
 * - `ja_fp_restore()`
 */
void ja_fp_init(void)
{
    /* x87 and SSE control words at their reset values, all registers zero. */
    static jet_fp_store_t fp_init_store;
    // FXSAVE area, little-endian: FCW = 0x037f at byte 0, MXCSR = 0x1f80 at
    // byte 24.
    fp_init_store.data[0] = 0x7fU;
    fp_init_store.data[1] = 0x03U;
    fp_init_store.data[24] = 0x80U;
    fp_init_store.data[25] = 0x1fU;

    ja_fp_restore(&fp_init_store);
}


/**
 * @funcname `pok_virt_to_phys`
 *
 * @description
 * Returns the physical address of a user address of the current space (old POK
 * interface); a bad address is fatal.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `virt` | `uintptr_t` | [in] The user address. |
 *
 * @return The physical address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space` | `ja_x86_64_space_t *` | The current space |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `space` to the address of `ja_spaces[ja_space_get_current() - 1U]`.
 * 2. If `virt` is less than `POK_PARTITION_MEMORY_BASE` or `virt` is greater
 *    than `(POK_PARTITION_MEMORY_BASE + space->size_total)`:
 *    1. Call `printf()` with the arguments
 *       `"pok_virt_to_phys: wrong virtual address 0x%%lx\n"` and
 *       `(unsigned long)virt`.
 *    2. Call `pok_fatal()` with the argument
 *       `"wrong pointer in pok_virt_to_phys\n"`.
 * 3. Return `virt - POK_PARTITION_MEMORY_BASE + space->phys_base`.
 *
 * @calls
 * - `ja_space_get_current()`
 * - `printf()`
 * - `pok_fatal()`
 */
uintptr_t pok_virt_to_phys(uintptr_t virt)
{
    ja_x86_64_space_t* space = &ja_spaces[ja_space_get_current() - 1U];

    if((virt < POK_PARTITION_MEMORY_BASE)
        || (virt > (POK_PARTITION_MEMORY_BASE + space->size_total)))
    {
        // Fatal error despite it is called from user space!!
        (void) printf("pok_virt_to_phys: wrong virtual address 0x%lx\n",
                      (unsigned long)virt);
        pok_fatal("wrong pointer in pok_virt_to_phys\n");
    }

    return virt - POK_PARTITION_MEMORY_BASE + space->phys_base;
}

/**
 * @funcname `pok_phys_to_virt`
 *
 * @description
 * Returns the user address of a physical address of the current space (old POK
 * interface); a bad address is fatal.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `phys` | `uintptr_t` | [in] The physical address. |
 *
 * @return The user address.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_spaces` | `ja_x86_64_space_t[]` | The memory spaces of the partitions |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space` | `ja_x86_64_space_t *` | The current space |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `space` to the address of `ja_spaces[ja_space_get_current() - 1U]`.
 * 2. If `phys` is less than `space->phys_base` or `phys` is greater than or
 *    equal to `(space->phys_base + space->size_total)`:
 *    1. Call `printf()` with the arguments
 *       `"pok_phys_to_virt: wrong physical address 0x%%lx\n"` and
 *       `(unsigned long)phys`.
 *    2. Call `pok_fatal()` with the argument
 *       `"wrong pointer in pok_phys_to_virt\n"`.
 * 3. Return `phys - space->phys_base + POK_PARTITION_MEMORY_BASE`.
 *
 * @calls
 * - `ja_space_get_current()`
 * - `printf()`
 * - `pok_fatal()`
 */
uintptr_t pok_phys_to_virt(uintptr_t phys)
{
    ja_x86_64_space_t* space = &ja_spaces[ja_space_get_current() - 1U];

    if((phys < space->phys_base)
        || (phys >= (space->phys_base + space->size_total)))
    {
        // Fatal error despite it is called from user space!!
        (void) printf("pok_phys_to_virt: wrong physical address 0x%lx\n",
                      (unsigned long)phys);
        pok_fatal("wrong pointer in pok_phys_to_virt\n");
    }

    return phys - space->phys_base + POK_PARTITION_MEMORY_BASE;
}
