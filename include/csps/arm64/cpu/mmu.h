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
 * AArch64 address translation with a 4 KiB granule and 39-bit addresses:
 * the shifts of the table levels, the bits of the descriptors and the
 * bits of the TCR and SCTLR registers, for the kernel (kernel/csps/arm64,
 * at EL1) and the hypervisor (hypervisor/csps/arm64, at EL2). What only
 * one of them uses is in its own header.
 *
 * The definitions were in kernel/csps/arm64/space.h and in
 * hypervisor/csps/arm64/include/arch/internal.h, each with its own copy.
 */

#ifndef JET_CPU_MMU_H
#define JET_CPU_MMU_H

/**
 * @macroname `L1_SHIFT`
 *
 * @description
 * The shift of the level 1 index in a virtual address (a level 1 block maps 1
 * GiB).
 */
#define L1_SHIFT            30U

/**
 * @macroname `L2_SHIFT`
 *
 * @description
 * The shift of the level 2 index in a virtual address (a level 2 block maps 2
 * MiB).
 */
#define L2_SHIFT            21U

/**
 * @macroname `L3_SHIFT`
 *
 * @description
 * The shift of the level 3 index in a virtual address (a page maps 4 KiB).
 */
#define L3_SHIFT            12U

/**
 * @macroname `DESC_BLOCK`
 *
 * @description
 * A block descriptor (level 1, 2).
 */
#define DESC_BLOCK          0x1UL

/**
 * @macroname `DESC_TABLE`
 *
 * @description
 * A descriptor of the next level table (level 1, 2).
 */
#define DESC_TABLE          0x3UL

/**
 * @macroname `DESC_PAGE`
 *
 * @description
 * A page descriptor (level 3).
 */
#define DESC_PAGE           0x3UL

/**
 * @macroname `DESC_ATTR`
 *
 * @description
 * The memory attributes of a block or page: an index in the MAIR
 * register of the exception level.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `i` | The index of the attributes in the MAIR register. |
 */
#define DESC_ATTR(i)        ((uint64_t) (i) << 2)

/**
 * @macroname `DESC_SH_INNER`
 *
 * @description
 * Inner shareable.
 */
#define DESC_SH_INNER       (3UL << 8)

/**
 * @macroname `DESC_AF`
 *
 * @description
 * The access flag (set, so that the first access doesn't fault).
 */
#define DESC_AF             (1UL << 10)

/**
 * @macroname `TCR_T0SZ`
 *
 * @description
 * T0SZ: the size of the address space of TTBR0 is 2^(64 - 25) bytes (39-bit
 * virtual addresses).
 */
#define TCR_T0SZ            25UL

/**
 * @macroname `TCR_IRGN0_WBWA`
 *
 * @description
 * Table walks through TTBR0: inner write-back write-allocate cacheable.
 */
#define TCR_IRGN0_WBWA      (1UL << 8)

/**
 * @macroname `TCR_ORGN0_WBWA`
 *
 * @description
 * Table walks through TTBR0: outer write-back write-allocate cacheable.
 */
#define TCR_ORGN0_WBWA      (1UL << 10)

/**
 * @macroname `TCR_SH0_INNER`
 *
 * @description
 * Table walks through TTBR0: inner shareable.
 */
#define TCR_SH0_INNER       (3UL << 12)

/**
 * @macroname `SCTLR_M`
 *
 * @description
 * The MMU is enabled.
 */
#define SCTLR_M             (1UL << 0)

/**
 * @macroname `SCTLR_C`
 *
 * @description
 * The data cache is enabled.
 */
#define SCTLR_C             (1UL << 2)

/**
 * @macroname `SCTLR_I`
 *
 * @description
 * The instruction cache is enabled.
 */
#define SCTLR_I             (1UL << 12)

#endif /* JET_CPU_MMU_H */
