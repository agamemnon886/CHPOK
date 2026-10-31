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
 */

/*
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

#define PAGE_SIZE 0x1000
#define LARGE_PAGE_SIZE 0x200000
#define TABLE_ENTRIES 512

#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT   21
#define PT_SHIFT   12
#define TABLE_INDEX(va, shift) (((va) >> (shift)) & (TABLE_ENTRIES - 1))

/* Page table entry bits. */
#define PTE_P   (1ULL << 0)
#define PTE_RW  (1ULL << 1)
#define PTE_US  (1ULL << 2)
#define PTE_PS  (1ULL << 7)
#define PTE_G   (1ULL << 8)
#define PTE_NX  (1ULL << 63)
#define PTE_ADDR_MASK 0x000ffffffffff000ULL

/* Access is the intersection of all levels, so non-leaf entries allow everything. */
#define PTE_TABLE_USER (PTE_P | PTE_RW | PTE_US)
#define PTE_TABLE_KERNEL (PTE_P | PTE_RW)

/* Page directory with the kernel's identity mapping of RAM. */
static uint64_t* kernel_pd;
/* Top-level table of the kernel (space 0). */
static uint64_t kernel_cr3;

static uint64_t* table_alloc(void)
{
    uint64_t* table = ja_mem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
    memset(table, 0, PAGE_SIZE);
    return table;
}

/* Allocate top-level table with the kernel's identity mapping. */
static uint64_t* pml4_create(void)
{
    uint64_t* pml4 = table_alloc();
    uint64_t* pdpt = table_alloc();

    pml4[0] = (uint64_t)pdpt | PTE_TABLE_USER;
    pdpt[0] = (uint64_t)kernel_pd | PTE_TABLE_KERNEL;

    return pml4;
}

/* Return next-level table pointed by 'entry', creating it if needed. */
static uint64_t* table_next(uint64_t* entry)
{
    if (*entry == 0)
        *entry = (uint64_t)table_alloc() | PTE_TABLE_USER;
    else if ((*entry & PTE_US) == 0)
        pok_fatal("User mapping overlaps with the kernel's one");

    return (uint64_t*)(*entry & PTE_ADDR_MASK);
}

/* Map user page at 'va' to 'pa'. */
static void map_user_page(uint64_t* pml4, uint64_t va, uint64_t pa,
    pok_bool_t writable, pok_bool_t exec)
{
    uint64_t* pdpt = table_next(&pml4[TABLE_INDEX(va, PML4_SHIFT)]);
    uint64_t* pd = table_next(&pdpt[TABLE_INDEX(va, PDPT_SHIFT)]);
    uint64_t* pt = table_next(&pd[TABLE_INDEX(va, PD_SHIFT)]);

    pt[TABLE_INDEX(va, PT_SHIFT)] = pa | PTE_P | PTE_US
        | (writable ? PTE_RW : 0)
        | (exec ? 0 : PTE_NX);
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
            || virt < POK_PARTITION_MEMORY_BASE + POK_PARTITION_MEMORY_MAX_SIZE)
        {
            // As before on (32-bit) x86, access to such a block will fail.
            printf("Memory block '%s' at 0x%lx is not mapped: virtual address should be page-aligned and not below 0x%llx\n",
                mblock->name, (unsigned long)virt,
                POK_PARTITION_MEMORY_BASE + POK_PARTITION_MEMORY_MAX_SIZE);
            continue;
        }

        if (phys + size > BOARD_RAM_SIZE)
            pok_fatal("Not enough memory for memory blocks");

        memset((void*)phys, 0, size);
        phys_start = phys + size;

        for (int space_id = 1; space_id <= ja_spaces_n && space_id < MAX_PID; space_id++)
        {
            enum mb_config_rights rights = mblock->pid_to_rights[space_id];
            if (rights == MB_CONFIG_NO_ACCESS) continue;

            uint64_t* pml4 = (uint64_t*)ja_spaces[space_id - 1].cr3;
            for (size_t offset = 0; offset < size; offset += PAGE_SIZE)
            {
                map_user_page(pml4, virt + offset, phys + offset,
                    rights == MB_CONFIG_READ_WRITE, FALSE);
            }
        }
    }
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
    struct ja_x86_64_space* space = &ja_spaces[space_id - 1];
    return (struct jet_kernel_shared_data* __kuser)space->phys_base;
}

void __user* ja_space_get_heap(jet_space_id space_id)
{
   struct ja_x86_64_space* space = &ja_spaces[space_id - 1];

   return (void __user*)(POK_PARTITION_MEMORY_BASE + space->heap_end - space->size_heap);
}


static jet_space_id current_space_id = 0;

/* Defined in cswitch.S. */
void ja_x86_64_user_jump(jet_stack_t stack_kernel,
    void (__user * entry_user)(void),
    jet_ustack_t stack_user);

void ja_user_space_jump(
    jet_stack_t stack_kernel,
    jet_space_id space_id,
    void (__user * entry_user)(void),
    jet_ustack_t stack_user)
{
   assert(space_id != 0 && space_id <= ja_spaces_n);

   ja_tss_set_rsp0 (stack_kernel);

   ja_x86_64_user_jump(stack_kernel, entry_user, stack_user);
}

void ja_space_init(void)
{
    uintptr_t phys_start = POK_PARTITION_MEMORY_PHYS_START;

    kernel_pd = table_alloc();
    for (uint64_t pa = 0; pa < BOARD_RAM_SIZE; pa += LARGE_PAGE_SIZE)
        kernel_pd[TABLE_INDEX(pa, PD_SHIFT)] = pa | PTE_P | PTE_RW | PTE_PS | PTE_G;

    kernel_cr3 = (uint64_t)pml4_create();

    for(int i = 0; i < ja_spaces_n; i++)
    {
        struct ja_x86_64_space* space = &ja_spaces[i];

        /*
         * Code and data segments should be aligned on 4k;
         */
        size_t size_total = space->size_normal;

        if(space->size_heap > 0)
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
            || space->phys_base + space->size_total > BOARD_RAM_SIZE)
        {
            printf("Not enough memory for space %d (0x%zx bytes at 0x%lx)\n",
                i + 1, space->size_total, (unsigned long)space->phys_base);
            pok_fatal("Not enough memory for partitions");
        }

        uint64_t* pml4 = pml4_create();
        for(size_t offset = 0; offset < space->size_total; offset += PAGE_SIZE)
        {
            // Only code and data may be executed.
            map_user_page(pml4, POK_PARTITION_MEMORY_BASE + offset,
                space->phys_base + offset, TRUE, offset < space->size_normal);
        }

        space->cr3 = (uint64_t)pml4;
    }

    memory_blocks_init(phys_start);

    asm volatile ("mov %0, %%cr3" : : "r" (kernel_cr3) : "memory");
}

void ja_ustack_init (jet_space_id space_id)
{
    assert(space_id != 0);

    struct ja_x86_64_space* space = &ja_spaces[space_id - 1];

    space->size_stack_used = 0;
}

jet_ustack_t ja_ustack_alloc (jet_space_id space_id, size_t stack_size)
{
    assert(space_id != 0);

    struct ja_x86_64_space* space = &ja_spaces[space_id - 1];

    size_t size_stack_new = space->size_stack_used + ALIGN_VAL(stack_size, 16);

    if(size_stack_new > space->size_stack) return 0;

    jet_ustack_t result = POK_PARTITION_MEMORY_BASE + space->size_total - space->size_stack_used;

    space->size_stack_used = size_stack_new;

    return result;
}

void ja_space_switch (jet_space_id space_id)
{
    if(space_id == current_space_id) return;

    // Kernel's mappings are global, so they stay in the TLB.
    uint64_t cr3 = space_id ? ja_spaces[space_id - 1].cr3 : kernel_cr3;
    asm volatile ("mov %0, %%cr3" : : "r" (cr3) : "memory");

    current_space_id = space_id;
}

jet_space_id ja_space_get_current (void)
{
    return current_space_id;
}

/* Area for fxsave64/fxrstor64. */
struct jet_fp_store
{
  uint8_t data[512];
} __attribute__((aligned(16)));

/*
 * Allocate place for store floating point registers.
 *
 * May be called only during OS init.
 */
struct jet_fp_store* ja_alloc_fp_store(void)
{
    struct jet_fp_store* res = ja_mem_alloc_aligned(sizeof(*res), 16);

    memset(res, 0, sizeof(*res));

    return res;
}

/* Save floating point registers into given place. */
void ja_fp_save(struct jet_fp_store* fp_store)
{
    asm volatile ("fxsave64 %0" : "=m" (*fp_store));
}

/* Restore floating point registers into given place. */
void ja_fp_restore(struct jet_fp_store* fp_store)
{
    asm volatile ("fxrstor64 %0" : : "m" (*fp_store));
}

/* Initialize floating point registers with zero. */
void ja_fp_init(void)
{
    /* x87 and SSE control words at their reset values, all registers zero. */
    static struct jet_fp_store fp_init_store;
    *(uint16_t*)&fp_init_store.data[0] = 0x37f;   /* FCW */
    *(uint32_t*)&fp_init_store.data[24] = 0x1f80; /* MXCSR */

    ja_fp_restore(&fp_init_store);
}


uintptr_t pok_virt_to_phys(uintptr_t virt)
{
    struct ja_x86_64_space* space = &ja_spaces[ja_space_get_current() - 1];

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
    struct ja_x86_64_space* space = &ja_spaces[ja_space_get_current() - 1];

    if((phys < space->phys_base)
        || (phys >= space->phys_base + space->size_total))
    {
        // Fatal error despite it is called from user space!!
        printf("pok_phys_to_virt: wrong physical address %p\n", (void*)phys);
        pok_fatal("wrong pointer in pok_phys_to_virt\n");
    }

    return phys - space->phys_base + POK_PARTITION_MEMORY_BASE;
}
