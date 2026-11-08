/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * EL2 address translation: the hypervisor sees the physical address space
 * through an identity map of 1 GiB blocks (one level 1 table, 39-bit
 * addresses, 4 KiB granule). The RAM of QEMU 'virt' (1 GiB to 4 GiB) is
 * normal write-back memory, the devices (first GiB) and the PCI
 * configuration space (256 GiB, QEMU's high ECAM) are device memory;
 * nothing else is mapped. The MMU and the caches are needed for the
 * exclusive accesses of the console lock and for coherency with the
 * guests, which run with their caches on.
 */

#include <arch/internal.h>

#define L1_ENTRIES          512U
#define L1_SHIFT            30U

// Block descriptor fields.
#define DESC_BLOCK          0x1UL
#define DESC_ATTR(i)        ((uint64_t) (i) << 2)
#define DESC_AP_RW          (1UL << 6)      // AP[1] is RES1 at EL2
#define DESC_SH_INNER       (3UL << 8)
#define DESC_AF             (1UL << 10)
#define DESC_XN             (1UL << 54)

// MAIR_EL2 attributes: 0 device-nGnRE, 1 normal write-back.
#define MAIR_DEVICE         0U
#define MAIR_NORMAL         1U
#define MAIR_VALUE          ((0x04UL << (8U * MAIR_DEVICE)) \
                             | (0xFFUL << (8U * MAIR_NORMAL)))

#define DESC_DEVICE         (DESC_BLOCK | DESC_ATTR(MAIR_DEVICE) \
                             | DESC_AP_RW | DESC_AF | DESC_XN)
#define DESC_NORMAL         (DESC_BLOCK | DESC_ATTR(MAIR_NORMAL) \
                             | DESC_AP_RW | DESC_SH_INNER | DESC_AF)

// TCR_EL2: 39-bit addresses, write-back walks, 4 KiB granule, 40-bit PA.
#define TCR_T0SZ            25UL
#define TCR_IRGN0_WBWA      (1UL << 8)
#define TCR_ORGN0_WBWA      (1UL << 10)
#define TCR_SH0_INNER       (3UL << 12)
#define TCR_PS_40BIT        (2UL << 16)
#define TCR_RES1            ((1UL << 31) | (1UL << 23))
#define TCR_VALUE           (TCR_T0SZ | TCR_IRGN0_WBWA | TCR_ORGN0_WBWA \
                             | TCR_SH0_INNER | TCR_PS_40BIT | TCR_RES1)

// SCTLR_EL2: MMU, data and instruction caches, stack alignment check.
#define SCTLR_RES1          0x30C50830UL
#define SCTLR_M             (1UL << 0)
#define SCTLR_C             (1UL << 2)
#define SCTLR_SA            (1UL << 3)
#define SCTLR_I             (1UL << 12)
#define SCTLR_VALUE         (SCTLR_RES1 | SCTLR_M | SCTLR_C | SCTLR_SA \
                             | SCTLR_I)

// What is mapped: the GiB numbers (physical address >> 30).
#define GIB_DEVICES         0U
#define GIB_RAM_FIRST       1U
#define GIB_RAM_LAST        3U
#define GIB_HIGH_ECAM       256U

hv_mmu_regs_t hv_el2_mmu;

/*
 * Build the identity map and turn the MMU on, on the boot CPU (called
 * first, with the MMU off: the tables and hv_el2_mmu are written to
 * memory, where the other CPUs read them with their MMU still off).
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (hv_mmu_init()): the CPU is given the
 * physical address of the table (identity-mapped), converted from a
 * pointer.
 */
void hv_mmu_init(void)
{
    static uint64_t el2_l1[L1_ENTRIES] HV_ALIGNED(4096);

    for (uint32_t i = 0U; i < L1_ENTRIES; i++) {
        el2_l1[i] = 0U;
    }
    el2_l1[GIB_DEVICES] = ((uint64_t) GIB_DEVICES << L1_SHIFT)
                          | DESC_DEVICE;
    for (uint32_t i = GIB_RAM_FIRST; i <= GIB_RAM_LAST; i++) {
        el2_l1[i] = ((uint64_t) i << L1_SHIFT) | DESC_NORMAL;
    }
    el2_l1[GIB_HIGH_ECAM] = ((uint64_t) GIB_HIGH_ECAM << L1_SHIFT)
                            | DESC_DEVICE;

    hv_el2_mmu.mair = MAIR_VALUE;
    hv_el2_mmu.tcr = TCR_VALUE;
    hv_el2_mmu.ttbr0 = (uint64_t) (uintptr_t) el2_l1;
    hv_el2_mmu.sctlr = SCTLR_VALUE;
    arm64_dsb_ish();
    hv_mmu_enable(&hv_el2_mmu);
}
