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
#include <asp/memory.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the allocator returns its
 * memory as a pointer to void, which is converted to the type of the object
 * allocated.
 */

#define PAGE_SIZE 0x1000U
#define LARGE_PAGE_SIZE 0x200000U
#define TABLE_ENTRIES 512U

#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT   21
#define PT_SHIFT   12
#define TABLE_INDEX(va, shift) (((va) >> (shift)) & (TABLE_ENTRIES - 1U))

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

static uint64_t* table_alloc(void)
{
    uint64_t* table = ja_mem_alloc_aligned(PAGE_SIZE, PAGE_SIZE);
    (void) memset(table, 0, PAGE_SIZE);
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
    if (*entry == 0U) {
        *entry = (uint64_t)table_alloc() | PTE_TABLE_USER;
    } else if ((*entry & PTE_US) == 0U) {
        pok_fatal("User mapping overlaps with the kernel's one");
    } else {
        // The entry already points to a table.
    }

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
        | (writable ? PTE_RW : 0U)
        | (exec ? 0U : PTE_NX);
}

/* Allocate RAM for memory blocks and map them into the spaces. */
static void memory_blocks_init(uintptr_t phys_first)
{
    uintptr_t phys_start = phys_first; // Next free physical address.

    for (size_t i = 0; i < jet_memory_blocks_n; i++)
    {
        struct memory_block* mblock = &jet_memory_blocks[i];
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
            enum mb_config_rights rights = mblock->pid_to_rights[space_id];
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

void ja_space_layout_get(jet_space_id space_id,
    struct jet_space_layout* space_layout)
{
    assert((space_id != 0U) && (space_id <= ja_spaces_n));

    space_layout->kernel_addr = (char*)ja_spaces[space_id - 1U].phys_base;
    space_layout->user_addr = (char* JET_USER)POK_PARTITION_MEMORY_BASE;
    space_layout->size = ja_spaces[space_id - 1U].size_normal;
}

struct jet_kernel_shared_data* JET_KUSER ja_space_shared_data(
    jet_space_id space_id)
{
    struct ja_x86_64_space* space = &ja_spaces[space_id - 1U];
    return (struct jet_kernel_shared_data* JET_KUSER)space->phys_base;
}

void JET_USER* ja_space_get_heap(jet_space_id space_id)
{
   struct ja_x86_64_space* space = &ja_spaces[space_id - 1U];

   return (void JET_USER*)(POK_PARTITION_MEMORY_BASE + space->heap_end
                           - space->size_heap);
}


static jet_space_id current_space_id = 0;

/* Defined in cswitch.S. */
void ja_x86_64_user_jump(jet_stack_t stack_kernel,
    void (JET_USER * entry_user)(void),
    jet_ustack_t stack_user);

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
        struct ja_x86_64_space* space = &ja_spaces[i];

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

void ja_ustack_init (jet_space_id space_id)
{
    assert(space_id != 0U);

    struct ja_x86_64_space* space = &ja_spaces[space_id - 1U];

    space->size_stack_used = 0;
}

jet_ustack_t ja_ustack_alloc (jet_space_id space_id, size_t stack_size)
{
    assert(space_id != 0U);

    struct ja_x86_64_space* space = &ja_spaces[space_id - 1U];

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

jet_space_id ja_space_get_current (void)
{
    return current_space_id;
}

/*
 * Allocate place for store floating point registers.
 *
 * May be called only during OS init.
 */
struct jet_fp_store* ja_alloc_fp_store(void)
{
    struct jet_fp_store* res = ja_mem_alloc_aligned(sizeof(*res), 16);

    (void) memset(res, 0, sizeof(*res));

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
    // FXSAVE area, little-endian: FCW = 0x037f at byte 0, MXCSR = 0x1f80 at
    // byte 24.
    fp_init_store.data[0] = 0x7fU;
    fp_init_store.data[1] = 0x03U;
    fp_init_store.data[24] = 0x80U;
    fp_init_store.data[25] = 0x1fU;

    ja_fp_restore(&fp_init_store);
}


uintptr_t pok_virt_to_phys(uintptr_t virt)
{
    struct ja_x86_64_space* space = &ja_spaces[ja_space_get_current() - 1U];

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

uintptr_t pok_phys_to_virt(uintptr_t phys)
{
    struct ja_x86_64_space* space = &ja_spaces[ja_space_get_current() - 1U];

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
