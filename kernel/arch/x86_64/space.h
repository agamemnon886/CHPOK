/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/space.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-04: struct jet_fp_store is defined
 * here (it was private to space.c), for the GDB server.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; Doxygen comments for the file, its types (with their members) and
 * its macros.
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros and the declaration of
 * ja_x86_64_user_jump() of kernel/arch/x86_64/space.c are moved here; the
 * Doxygen comment of the file is removed (header files are not documented).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_X86_64_SPACE_H
#define POK_X86_64_SPACE_H

#include <types.h>
#include "thread.h"
#include <arch/deployment.h>
#include <bsp/bsp.h>
#include <stdint.h>
#include <asp/space.h>

/**
 * @macroname `POK_PARTITION_MEMORY_BASE`
 *
 * @description
 * Virtual address where partition's memory starts (in every user space).
 *
 * Should coincide with 'kshd' in the partition's ldscript.
 *
 * On x86_64 (0x40000000, 1 GiB), partitions are below 2 GiB, as required by
 * the default ('small') code model; lower virtual addresses are occupied by
 * the kernel's identity mapping of RAM, which is not accessible from user
 * space.
 *
 * On arm64 (0x100000000, 4 GiB, kernel/arch/arm64/space.h), lower virtual
 * addresses are occupied by the kernel's identity mapping (devices and RAM),
 * which is accessible only from EL1.
 */
#define POK_PARTITION_MEMORY_BASE 0x40000000ULL

/**
 * @macroname `POK_PARTITION_MEMORY_MAX_SIZE`
 *
 * @description
 * Maximum size of the memory of one partition (one PDPT entry on x86_64, one
 * level-1 entry on arm64: the same on both).
 */
#define POK_PARTITION_MEMORY_MAX_SIZE 0x40000000ULL

/**
 * @macroname `POK_PARTITION_MEMORY_PHYS_START`
 *
 * @description
 * Beginning of the phys memory used for partitions (the board's, on x86_64 and
 * arm64).
 */
#define POK_PARTITION_MEMORY_PHYS_START BOARD_PARTITION_PHYS_START

/* Build page tables for all spaces and switch to the kernel's ones. */
/** \cond */
void ja_space_init(void);

/** \endcond */

/*
 * Area for fxsave64/fxrstor64: the x87, MMX and SSE registers of a user
 * thread (see ja_fp_save()). The GDB server reads and writes it too.
 *
 * MISRA C:2025 Dir 1.2 deviation: __attribute__((aligned)) is a GNU
 * extension; fxsave64 needs a 16-byte aligned area.
 */
/**
 * @typename `jet_fp_store_t`
 *
 * @description
 * The floating point registers of a user thread, which ja_fp_save() fills and
 * ja_fp_restore() loads; the GDB server reads and writes them too. On x86_64
 * (kernel/arch/x86_64/space.h), the area of fxsave64/fxrstor64 (the x87, MMX
 * and SSE registers), 16-byte aligned; on arm64 (kernel/arch/arm64/space.h),
 * the FP/SIMD registers, which must correspond to fp.S.
 */
struct jet_fp_store
{
  uint8_t data[512]; /**< x86_64: the FXSAVE area. */
} __attribute__((aligned(16)));

/**
 * @name Page tables
 * The geometry of the 4-level page tables (4 KiB pages, 2 MiB large pages).
 * @{
 */

/**
 * @macroname `PAGE_SIZE`
 *
 * @description
 * The size of a page and of a page table, in bytes. The same on arm64
 * (kernel/arch/arm64/space.h: a page and a translation table).
 */
#define PAGE_SIZE 0x1000U

/**
 * @macroname `LARGE_PAGE_SIZE`
 *
 * @description
 * The size of a large page (a page directory entry with PTE_PS), in bytes: the
 * kernel maps the RAM with them.
 */
#define LARGE_PAGE_SIZE 0x200000U

/**
 * @macroname `TABLE_ENTRIES`
 *
 * @description
 * The number of entries of a page table (the same for a translation table on
 * arm64, kernel/arch/arm64/space.h).
 */
#define TABLE_ENTRIES 512U

/**
 * @macroname `PML4_SHIFT`
 *
 * @description
 * The shift of the PML4 index in a virtual address.
 */
#define PML4_SHIFT 39

/**
 * @macroname `PDPT_SHIFT`
 *
 * @description
 * The shift of the page directory pointer table index in a virtual address.
 */
#define PDPT_SHIFT 30

/**
 * @macroname `PD_SHIFT`
 *
 * @description
 * The shift of the page directory index in a virtual address.
 */
#define PD_SHIFT 21

/**
 * @macroname `PT_SHIFT`
 *
 * @description
 * The shift of the page table index in a virtual address.
 */
#define PT_SHIFT 12

/**
 * @macroname `TABLE_INDEX`
 *
 * @description
 * The index of the entry of a virtual address in the table of a level (the
 * same on arm64, kernel/arch/arm64/space.h, with its L1_SHIFT,
 * L2_SHIFT and L3_SHIFT).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `va` | The virtual address. |
 * | `shift` | PML4_SHIFT, PDPT_SHIFT, PD_SHIFT or PT_SHIFT. |
 */
#define TABLE_INDEX(va, shift) (((va) >> (shift)) & (TABLE_ENTRIES - 1U))

/** @} */

/**
 * @name Page table entry bits
 * Page table entry bits.
 * @{
 */

/**
 * @macroname `PTE_P`
 *
 * @description
 * Present.
 */
#define PTE_P   (1ULL << 0)

/**
 * @macroname `PTE_RW`
 *
 * @description
 * Writable.
 */
#define PTE_RW  (1ULL << 1)

/**
 * @macroname `PTE_US`
 *
 * @description
 * Accessible from user mode (partitions).
 */
#define PTE_US  (1ULL << 2)

/**
 * @macroname `PTE_PS`
 *
 * @description
 * A large page (in a page directory entry).
 */
#define PTE_PS  (1ULL << 7)

/**
 * @macroname `PTE_G`
 *
 * @description
 * Global: kept in the TLB when CR3 changes (the kernel's mappings).
 */
#define PTE_G   (1ULL << 8)

/**
 * @macroname `PTE_NX`
 *
 * @description
 * Not executable.
 */
#define PTE_NX  (1ULL << 63)

/**
 * @macroname `PTE_ADDR_MASK`
 *
 * @description
 * The bits of an entry that hold the physical address of the page or of the
 * next table.
 */
#define PTE_ADDR_MASK 0x000ffffffffff000ULL

/** @} */

/**
 * @name Non-leaf entries
 * Access is the intersection of all levels, so non-leaf entries allow
 * everything.
 * @{
 */

/**
 * @macroname `PTE_TABLE_USER`
 *
 * @description
 * A non-leaf entry of a partition's tables.
 */
#define PTE_TABLE_USER (PTE_P | PTE_RW | PTE_US)

/**
 * @macroname `PTE_TABLE_KERNEL`
 *
 * @description
 * A non-leaf entry of the kernel's mapping.
 */
#define PTE_TABLE_KERNEL (PTE_P | PTE_RW)

/** @} */

/** \cond */
/* Defined in cswitch.S. */
void ja_x86_64_user_jump(jet_stack_t stack_kernel,
    void (JET_USER * entry_user)(void),
    jet_ustack_t stack_user);
/** \endcond */

#endif /* !POK_X86_64_SPACE_H */
