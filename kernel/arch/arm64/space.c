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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/space.c for arm64.
 */

/*
 * User spaces on arm64.
 *
 * Every space has its own translation table (TTBR0_EL1, 4K granule,
 * 39-bit virtual addresses, walk starts at level 1) with an ASID equal
 * to the space id, so switching spaces doesn't need TLB invalidation.
 *
 * Each table contains:
 *  - identity mapping of the board's devices and RAM with 1 GiB
 *    blocks, accessible only from EL1 (global, shared by all spaces);
 *  - for user spaces, partition's memory mapped with 4K pages at
 *    POK_PARTITION_MEMORY_BASE, accessible from EL0 (non-global).
 *
 * Space 0 (kernel) has only the identity mapping.
 *
 * Memory blocks are backed by RAM allocated after partitions' memory
 * and mapped into the spaces which have access to them, read-only or
 * read-write. (The configured PhysicalAddress is not used.)
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

#include "space.h"
#include "sysreg.h"

#define PAGE_SIZE 0x1000
#define TABLE_ENTRIES 512

#define L1_SHIFT 30
#define L2_SHIFT 21
#define L3_SHIFT 12
#define TABLE_INDEX(va, shift) (((va) >> (shift)) & (TABLE_ENTRIES - 1))

/* Descriptor bits. */
#define DESC_TABLE      (3ULL << 0)  /* level 1, 2 */
#define DESC_BLOCK      (1ULL << 0)  /* level 1, 2 */
#define DESC_PAGE       (3ULL << 0)  /* level 3 */
#define DESC_ATTR(idx)  ((uint64_t)(idx) << 2)
#define DESC_AP_EL1_RW  (0ULL << 6)
#define DESC_AP_ALL_RW  (1ULL << 6)
#define DESC_AP_ALL_RO  (3ULL << 6)
#define DESC_SH_INNER   (3ULL << 8)
#define DESC_AF         (1ULL << 10)
#define DESC_NG         (1ULL << 11)
#define DESC_PXN        (1ULL << 53)
#define DESC_UXN        (1ULL << 54)

/* MAIR_EL1 attribute indices. */
#define MAIR_IDX_DEVICE 0  /* Device-nGnRnE */
#define MAIR_IDX_NORMAL 1  /* Normal, write-back cacheable */
#define MAIR_VALUE      ((0x00ULL << (8 * MAIR_IDX_DEVICE)) | (0xffULL << (8 * MAIR_IDX_NORMAL)))

#define DESC_KERNEL_DEVICE (DESC_BLOCK | DESC_ATTR(MAIR_IDX_DEVICE) | DESC_AP_EL1_RW \
    | DESC_AF | DESC_PXN | DESC_UXN)
#define DESC_KERNEL_RAM    (DESC_BLOCK | DESC_ATTR(MAIR_IDX_NORMAL) | DESC_AP_EL1_RW \
    | DESC_SH_INNER | DESC_AF | DESC_UXN)
/* EL0-writable memory is never executable at EL1 (PXN is implied). */
#define DESC_USER_PAGE     (DESC_PAGE | DESC_ATTR(MAIR_IDX_NORMAL) \
    | DESC_SH_INNER | DESC_AF | DESC_NG | DESC_PXN)

/* TCR_EL1: T0SZ=25 (39-bit VA), WB-WA cacheable inner shareable walks, 4K granule, no TTBR1. */
#define TCR_T0SZ        25ULL
#define TCR_IRGN0_WBWA  (1ULL << 8)
#define TCR_ORGN0_WBWA  (1ULL << 10)
#define TCR_SH0_INNER   (3ULL << 12)
#define TCR_EPD1        (1ULL << 23)
#define TCR_IPS_SHIFT   32

#define SCTLR_M (1ULL << 0)
#define SCTLR_A (1ULL << 1)
#define SCTLR_C (1ULL << 2)
#define SCTLR_I (1ULL << 12)

#define TTBR_ASID_SHIFT 48

/* Translation table of the kernel (space 0). */
static uint64_t kernel_ttbr0;

static uint64_t* table_alloc(void)
{
    uint64_t* table = ja_mem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
    memset(table, 0, PAGE_SIZE);
    return table;
}

/* Allocate level-1 table with the kernel's identity mapping. */
static uint64_t* l1_table_create(void)
{
    uint64_t* l1 = table_alloc();

    for (uint64_t pa = BOARD_DEVICE_BASE; pa < BOARD_DEVICE_BASE + BOARD_DEVICE_SIZE; pa += 1ULL << L1_SHIFT)
        l1[TABLE_INDEX(pa, L1_SHIFT)] = pa | DESC_KERNEL_DEVICE;

    for (uint64_t pa = BOARD_RAM_BASE; pa < BOARD_RAM_BASE + BOARD_RAM_SIZE; pa += 1ULL << L1_SHIFT)
        l1[TABLE_INDEX(pa, L1_SHIFT)] = pa | DESC_KERNEL_RAM;

    return l1;
}

/* Return next-level table pointed by 'entry', creating it if needed. */
static uint64_t* table_next(uint64_t* entry)
{
    if (*entry == 0)
        *entry = (uint64_t)table_alloc() | DESC_TABLE;
    else if ((*entry & 3) != DESC_TABLE)
        pok_fatal("User mapping overlaps with the kernel's one");

    return (uint64_t*)(*entry & 0x0000fffffffff000ULL);
}

/* Map user page at 'va' to 'pa'. */
static void map_user_page(uint64_t* l1, uint64_t va, uint64_t pa,
    pok_bool_t writable, pok_bool_t exec)
{
    uint64_t* l2 = table_next(&l1[TABLE_INDEX(va, L1_SHIFT)]);
    uint64_t* l3 = table_next(&l2[TABLE_INDEX(va, L2_SHIFT)]);

    l3[TABLE_INDEX(va, L3_SHIFT)] = pa | DESC_USER_PAGE
        | (writable ? DESC_AP_ALL_RW : DESC_AP_ALL_RO)
        | (exec ? 0 : DESC_UXN);
}

/* Allocate RAM for memory blocks and map them into the spaces. */
static void memory_blocks_init(uintptr_t phys_start)
{
    for (size_t i = 0; i < jet_memory_blocks_n; i++)
    {
        struct memory_block* mblock = &jet_memory_blocks[i];
        uint64_t virt = mblock->virt_addr;
        size_t size = ALIGN_VAL(mblock->size, PAGE_SIZE);
        uintptr_t phys = ALIGN_VAL(phys_start, PAGE_SIZE);

        if ((virt & (PAGE_SIZE - 1)) != 0
            || virt < BOARD_RAM_BASE + BOARD_RAM_SIZE // Kernel's identity mapping.
            || virt + size > POK_PARTITION_MEMORY_BASE)
        {
            printf("Memory block '%s' at 0x%lx: virtual address should be page-aligned and within [0x%llx; 0x%llx)\n",
                mblock->name, (unsigned long)virt,
                BOARD_RAM_BASE + BOARD_RAM_SIZE, POK_PARTITION_MEMORY_BASE);
            pok_fatal("Wrong memory block configuration");
        }

        if (phys + size > BOARD_RAM_BASE + BOARD_RAM_SIZE)
            pok_fatal("Not enough memory for memory blocks");

        memset((void*)phys, 0, size);
        phys_start = phys + size;

        for (int space_id = 1; space_id <= ja_spaces_n && space_id < MAX_PID; space_id++)
        {
            enum mb_config_rights rights = mblock->pid_to_rights[space_id];
            if (rights == MB_CONFIG_NO_ACCESS) continue;

            uint64_t* l1 = (uint64_t*)(ja_spaces[space_id - 1].ttbr0 & 0x0000fffffffff000ULL);
            for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
            {
                map_user_page(l1, virt + offset, phys + offset,
                    rights == MB_CONFIG_READ_WRITE, FALSE);
            }
        }
    }
}

static void mmu_enable(void)
{
    uint64_t parange = read_sysreg(id_aa64mmfr0_el1) & 0x7;

    write_sysreg(mair_el1, MAIR_VALUE);
    write_sysreg(tcr_el1, TCR_T0SZ | TCR_IRGN0_WBWA | TCR_ORGN0_WBWA | TCR_SH0_INNER
        | TCR_EPD1 | (parange << TCR_IPS_SHIFT));
    write_sysreg(ttbr0_el1, kernel_ttbr0);
    asm volatile("dsb ish\n tlbi vmalle1\n dsb ish" ::: "memory");
    isb();

    uint64_t sctlr = read_sysreg(sctlr_el1);
    sctlr |= SCTLR_M | SCTLR_C | SCTLR_I;
    sctlr &= ~SCTLR_A;
    write_sysreg(sctlr_el1, sctlr);
    isb();
}

void ja_space_layout_get(jet_space_id space_id,
    struct jet_space_layout* space_layout)
{
    assert(space_id != 0 && space_id <= ja_spaces_n);

    space_layout->kernel_addr = (char*)ja_spaces[space_id - 1].phys_base;
    space_layout->user_addr = (char* __user)POK_PARTITION_MEMORY_BASE;
    space_layout->size = ja_spaces[space_id - 1].size_normal;
}

struct jet_kernel_shared_data* __kuser ja_space_shared_data(jet_space_id space_id)
{
    struct ja_arm64_space* space = &ja_spaces[space_id - 1];
    return (struct jet_kernel_shared_data* __kuser)space->phys_base;
}

void __user* ja_space_get_heap(jet_space_id space_id)
{
   struct ja_arm64_space* space = &ja_spaces[space_id - 1];

   return (void __user*)(POK_PARTITION_MEMORY_BASE + space->heap_end - space->size_heap);
}

static jet_space_id current_space_id = 0;

/* Defined in cswitch.S. */
void ja_arm64_user_jump(jet_stack_t stack_kernel,
    void (__user * entry_user)(void),
    jet_ustack_t stack_user);

void ja_user_space_jump(
    jet_stack_t stack_kernel,
    jet_space_id space_id,
    void (__user * entry_user)(void),
    jet_ustack_t stack_user)
{
    assert(space_id != 0 && space_id <= ja_spaces_n);

    ja_arm64_user_jump(stack_kernel, entry_user, stack_user);
}

void ja_space_init(void)
{
    uintptr_t phys_start = POK_PARTITION_MEMORY_PHYS_START;

    kernel_ttbr0 = (uint64_t)l1_table_create();

    for(int i = 0; i < ja_spaces_n; i++)
    {
        struct ja_arm64_space* space = &ja_spaces[i];

        size_t size_total = space->size_normal;

        if(space->size_heap > 0)
        {
            size_total = ALIGN_VAL(size_total, 16) + space->size_heap;
        }

        // Store intermediate result.
        space->heap_end =  size_total;

        size_total = ALIGN_VAL(size_total, 16) + space->size_stack;

        /* Such a way, next space will be page-aligned. */
        space->size_total = ALIGN_VAL(size_total, PAGE_SIZE);

        if(space->phys_base == 0)
        {
            space->phys_base = ALIGN_VAL(phys_start, PAGE_SIZE);
            phys_start = space->phys_base + space->size_total;
        }
        else
        {
            assert(space->phys_base >= POK_PARTITION_MEMORY_PHYS_START);
            assert((space->phys_base & (PAGE_SIZE - 1)) == 0);
            uintptr_t phys_base_end = space->phys_base + space->size_total;
            if(phys_start < phys_base_end)
                phys_start = phys_base_end;
        }

        if(space->size_total > POK_PARTITION_MEMORY_MAX_SIZE
            || space->phys_base + space->size_total > BOARD_RAM_BASE + BOARD_RAM_SIZE)
        {
            printf("Not enough memory for space %d (0x%zx bytes at 0x%lx)\n",
                i + 1, space->size_total, (unsigned long)space->phys_base);
            pok_fatal("Not enough memory for partitions");
        }

        uint64_t* l1 = l1_table_create();
        for(size_t offset = 0; offset < space->size_total; offset += PAGE_SIZE)
        {
            // As on x86, only code and data may be executed.
            map_user_page(l1, POK_PARTITION_MEMORY_BASE + offset,
                space->phys_base + offset, TRUE, offset < space->size_normal);
        }

        space->ttbr0 = (uint64_t)l1 | ((uint64_t)(i + 1) << TTBR_ASID_SHIFT);
    }

    memory_blocks_init(phys_start);

    mmu_enable();
}

void ja_ustack_init (jet_space_id space_id)
{
    assert(space_id != 0);

    struct ja_arm64_space* space = &ja_spaces[space_id - 1];

    space->size_stack_used = 0;
}

jet_ustack_t ja_ustack_alloc (jet_space_id space_id, size_t stack_size)
{
    assert(space_id != 0);

    struct ja_arm64_space* space = &ja_spaces[space_id - 1];

    size_t size_stack_new = space->size_stack_used + ALIGN_VAL(stack_size, 16);

    if(size_stack_new > space->size_stack) return 0;

    jet_ustack_t result = POK_PARTITION_MEMORY_BASE + space->size_total - space->size_stack_used;

    space->size_stack_used = size_stack_new;

    return result;
}

void ja_space_switch (jet_space_id space_id)
{
    if(space_id == current_space_id) return;

    write_sysreg(ttbr0_el1, space_id ? ja_spaces[space_id - 1].ttbr0 : kernel_ttbr0);
    isb();

    current_space_id = space_id;
}

jet_space_id ja_space_get_current (void)
{
    return current_space_id;
}

/* Must correspond to fp.S */
struct jet_fp_store
{
    uint64_t q[32][2];
    uint64_t fpcr;
    uint64_t fpsr;
};

struct jet_fp_store* ja_alloc_fp_store(void)
{
    struct jet_fp_store* res = ja_mem_alloc_aligned(sizeof(*res), 16);

    memset(res, 0, sizeof(*res));

    return res;
}

uintptr_t pok_virt_to_phys(uintptr_t virt)
{
    struct ja_arm64_space* space = &ja_spaces[ja_space_get_current() - 1];

    if((virt < POK_PARTITION_MEMORY_BASE)
        || (virt > POK_PARTITION_MEMORY_BASE + space->size_total))
    {
        // Fatal error despite it is called from user space!!
        printf("pok_virt_to_phys: wrong virtual address %p\n", (void*)virt);
        pok_fatal("wrong pointer in pok_virt_to_phys\n");
    }

    return virt - POK_PARTITION_MEMORY_BASE + space->phys_base;
}

uintptr_t pok_phys_to_virt(uintptr_t phys)
{
    struct ja_arm64_space* space = &ja_spaces[ja_space_get_current() - 1];

    if((phys < space->phys_base)
        || (phys >= space->phys_base + space->size_total))
    {
        // Fatal error despite it is called from user space!!
        printf("pok_phys_to_virt: wrong physical address %p\n", (void*)phys);
        pok_fatal("wrong pointer in pok_phys_to_virt\n");
    }

    return phys - space->phys_base + POK_PARTITION_MEMORY_BASE;
}
