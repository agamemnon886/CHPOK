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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/space.h for arm64.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-05: struct jet_fp_store is defined
 * here (it was private to space.c), for the GDB server.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; Doxygen comments for the file and the members of its types; the
 * types and macros defined in the same way for x86_64 are documented with their
 * x86_64 definitions.
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros and the declaration of
 * ja_arm64_user_jump() of kernel/arch/arm64/space.c are moved here; the Doxygen
 * comment of the file is removed (header files are not documented).
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/space.h (the CPU support packages are in kernel/csps); the
 * comments name the new paths; the translation bits that the hypervisor uses
 * too are in include/csps/arm64/cpu/mmu.h.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_ARM64_SPACE_H
#define POK_ARM64_SPACE_H

#include <types.h>
#include <arch/deployment.h>
#include <bsp/bsp.h>
#include <stdint.h>
#include <asp/space.h>
/* The translation bits that the hypervisor uses too. */
#include <cpu/mmu.h>

/*
 * Virtual address where partition's memory starts (in every user space).
 *
 * Should coincide with 'kshd' in the partition's ldscript.
 *
 * Lower virtual addresses are occupied by the kernel's identity mapping
 * (devices and RAM), which is accessible only from EL1.
 *
 * Documented with its x86_64 definition: kernel/csps/x86_64/space.h.
 */
#define POK_PARTITION_MEMORY_BASE 0x100000000ULL

/*
 * Maximum size of the memory of one partition (one level-1 entry).
 *
 * Documented with its x86_64 definition: kernel/csps/x86_64/space.h.
 */
#define POK_PARTITION_MEMORY_MAX_SIZE 0x40000000ULL

/*
 * Beginning of the phys memory used for partitions.
 *
 * Documented with its x86_64 definition: kernel/csps/x86_64/space.h.
 */
#define POK_PARTITION_MEMORY_PHYS_START BOARD_PARTITION_PHYS_START

/* Build translation tables for all spaces and enable the MMU. */
/** \cond */
void ja_space_init(void);

/** \endcond */

/*
 * FP/SIMD registers of a user thread: the area that ja_fp_save() fills and
 * ja_fp_restore() loads. The GDB server reads and writes it too.
 *
 * Must correspond to fp.S.
 *
 * Documented with its x86_64 definition: kernel/csps/x86_64/space.h.
 */
struct jet_fp_store
{
  uint64_t q[32][2]; /**< arm64: q0 to q31 (two 64-bit halves each). */
  uint64_t fpcr; /**< arm64: FPCR. */
  uint64_t fpsr; /**< arm64: FPSR. */
};

/**
 * @name Translation tables
 * The geometry of the translation tables (4 KiB granule, 3 levels, 39-bit
 * virtual addresses).
 * @{
 */

/*
 * The same on x86_64 and arm64; documented with its x86_64 definition:
 * kernel/csps/x86_64/space.h. */
#define PAGE_SIZE 0x1000U

/*
 * The same on x86_64 and arm64; documented with its x86_64 definition:
 * kernel/csps/x86_64/space.h. */
#define TABLE_ENTRIES 512U

/*
 * The same on x86_64 and arm64; documented with its x86_64 definition:
 * kernel/csps/x86_64/space.h. */
#define TABLE_INDEX(va, shift) (((va) >> (shift)) & (TABLE_ENTRIES - 1U))

/** @} */

/**
 * @name Descriptor bits
 * Descriptor bits: the fields of a translation table descriptor.
 * @{
 */

/**
 * @macroname `DESC_AP_EL1_RW`
 *
 * @description
 * Access: read-write at EL1, none at EL0.
 */
#define DESC_AP_EL1_RW (0ULL << 6)

/**
 * @macroname `DESC_AP_ALL_RW`
 *
 * @description
 * Access: read-write at EL1 and EL0.
 */
#define DESC_AP_ALL_RW (1ULL << 6)

/**
 * @macroname `DESC_AP_ALL_RO`
 *
 * @description
 * Access: read-only at EL1 and EL0.
 */
#define DESC_AP_ALL_RO (3ULL << 6)

/**
 * @macroname `DESC_NG`
 *
 * @description
 * Not global: the mapping belongs to the ASID (space) of the table.
 */
#define DESC_NG (1ULL << 11)

/**
 * @macroname `DESC_PXN`
 *
 * @description
 * Not executable at EL1.
 */
#define DESC_PXN (1ULL << 53)

/**
 * @macroname `DESC_UXN`
 *
 * @description
 * Not executable at EL0.
 */
#define DESC_UXN (1ULL << 54)

/** @} */

/**
 * @name Memory attributes
 * MAIR_EL1 attribute indices, and the value of MAIR_EL1 that defines them.
 * @{
 */

/**
 * @macroname `MAIR_IDX_DEVICE`
 *
 * @description
 * Device-nGnRnE memory: the devices.
 */
#define MAIR_IDX_DEVICE 0U

/**
 * @macroname `MAIR_IDX_NORMAL`
 *
 * @description
 * Normal, write-back cacheable memory: the RAM.
 */
#define MAIR_IDX_NORMAL 1U

/**
 * @macroname `MAIR_VALUE`
 *
 * @description
 * The value of MAIR_EL1: attribute MAIR_IDX_DEVICE is Device-nGnRnE (0x00),
 * MAIR_IDX_NORMAL is normal write-back cacheable (0xff).
 */
#define MAIR_VALUE      ((0x00ULL << (8U * MAIR_IDX_DEVICE)) \
                         | (0xffULL << (8U * MAIR_IDX_NORMAL)))

/** @} */

/**
 * @name Kernel and user descriptors
 * The descriptors of the mappings that the kernel makes.
 * @{
 */

/**
 * @macroname `DESC_KERNEL_DEVICE`
 *
 * @description
 * A 1 GiB block of the board's devices: EL1 only, never executable.
 */
#define DESC_KERNEL_DEVICE (DESC_BLOCK | DESC_ATTR(MAIR_IDX_DEVICE) \
    | DESC_AP_EL1_RW | DESC_AF | DESC_PXN | DESC_UXN)

/**
 * @macroname `DESC_KERNEL_RAM`
 *
 * @description
 * A 1 GiB block of RAM, mapped one-to-one for the kernel: EL1 only, not
 * executable at EL0.
 */
#define DESC_KERNEL_RAM    (DESC_BLOCK | DESC_ATTR(MAIR_IDX_NORMAL) \
    | DESC_AP_EL1_RW | DESC_SH_INNER | DESC_AF | DESC_UXN)

/**
 * @macroname `DESC_USER_PAGE`
 *
 * @description
 * A page of a partition (its access bits are added): non-global. EL0-writable
 * memory is never executable at EL1 (PXN is implied).
 */
#define DESC_USER_PAGE     (DESC_PAGE | DESC_ATTR(MAIR_IDX_NORMAL) \
    | DESC_SH_INNER | DESC_AF | DESC_NG | DESC_PXN)

/** @} */

/**
 * @name TCR_EL1 fields
 * TCR_EL1: T0SZ=25 (39-bit VA), WB-WA cacheable inner shareable walks, 4K
 * granule, no TTBR1.
 * @{
 */

/**
 * @macroname `TCR_EPD1`
 *
 * @description
 * No table walks through TTBR1 (the upper address range isn't used).
 */
#define TCR_EPD1 (1ULL << 23)

/**
 * @macroname `TCR_IPS_SHIFT`
 *
 * @description
 * The shift of the IPS field (the physical address size, copied from
 * ID_AA64MMFR0_EL1).
 */
#define TCR_IPS_SHIFT 32

/** @} */

/**
 * @name SCTLR_EL1 bits
 * The bits of SCTLR_EL1 that the kernel sets or clears when it enables the MMU.
 * @{
 */

/**
 * @macroname `SCTLR_A`
 *
 * @description
 * Alignment checks (cleared: unaligned accesses are allowed).
 */
#define SCTLR_A (1ULL << 1)

/** @} */

/**
 * @macroname `TTBR_ASID_SHIFT`
 *
 * @description
 * The shift of the ASID in TTBR0_EL1: the space's index + 1, so that spaces
 * don't need TLB invalidation when they are switched.
 */
#define TTBR_ASID_SHIFT 48

/** \cond */
/* Defined in cswitch.S. */
void ja_arm64_user_jump(jet_stack_t stack_kernel,
    void (JET_USER * entry_user)(void),
    jet_ustack_t stack_user);
/** \endcond */

#endif /* !POK_ARM64_SPACE_H */
