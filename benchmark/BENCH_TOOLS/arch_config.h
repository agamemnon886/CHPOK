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
 * Processor description used by the benchmarks: cache and TLB geometry
 * and the architecture selection. The values are chosen by the compiler's
 * target (x86_64 or aarch64), so the same sources build for both boards.
 *
 * QEMU (TCG) does not model caches or TLB sizes, so these values only
 * matter on real hardware: adjust them for the processor the benchmarks
 * run on.
 */

#ifndef __BENCH_ARCH_CONFIG_H_
#define __BENCH_ARCH_CONFIG_H_

#if defined(__x86_64__)

#define ARCH_X86 1                    /* x86_64 */
#define ARCH_PPC 0
#define ARCH_ARM 0

/* Intel Core i5-750 (the x86 processor the benchmarks were tuned for) */
#define CACHE_SIZE_L1_INST 32768      /* Bytes */
#define CACHE_SIZE_L1_DATA 32768      /* Bytes */
#define CACHE_SIZE_L2      262144     /* Bytes */
#define CACHE_SIZE_L3      8388608    /* Bytes */

#define CACHE_LINE_SIZE_L1_INST 64    /* Bytes */
#define CACHE_LINE_SIZE_L1_DATA 64    /* Bytes */
#define CACHE_LINE_SIZE_L2      64    /* Bytes */
#define CACHE_LINE_SIZE_L3      64    /* Bytes */

#define CACHE_ASSOC_NUMBER_L1_INST 4  /* Ways */
#define CACHE_ASSOC_NUMBER_L1_DATA 8  /* Ways */
#define CACHE_ASSOC_NUMBER_L2      8  /* Ways */
#define CACHE_ASSOC_NUMBER_L3      16 /* Ways */

#define TLB_SIZE_INST   128           /* 4K entries */
#define TLB_SIZE_DATA   64            /* 4K entries */
#define TLB_SIZE_SHARED 512           /* 4K entries */

#define TLB_ASSOC_NUMBER_INST   4     /* Ways */
#define TLB_ASSOC_NUMBER_DATA   4     /* Ways */
#define TLB_ASSOC_NUMBER_SHARED 4     /* Ways */

#elif defined(__aarch64__)

#define ARCH_X86 0
#define ARCH_PPC 0
#define ARCH_ARM 1                    /* AArch64 */

/*
 * ARM Cortex-A53 (QEMU virt -cpu cortex-a53). The L2 size is chosen by the
 * SoC (128 KB to 2 MB); 512 KB is common. There is no L3.
 */
#define CACHE_SIZE_L1_INST 32768      /* Bytes */
#define CACHE_SIZE_L1_DATA 32768      /* Bytes */
#define CACHE_SIZE_L2      524288     /* Bytes */
#define CACHE_SIZE_L3      0          /* Bytes, none */

#define CACHE_LINE_SIZE_L1_INST 64    /* Bytes */
#define CACHE_LINE_SIZE_L1_DATA 64    /* Bytes */
#define CACHE_LINE_SIZE_L2      64    /* Bytes */
#define CACHE_LINE_SIZE_L3      0     /* Bytes, none */

#define CACHE_ASSOC_NUMBER_L1_INST 2  /* Ways */
#define CACHE_ASSOC_NUMBER_L1_DATA 4  /* Ways */
#define CACHE_ASSOC_NUMBER_L2      16 /* Ways */
#define CACHE_ASSOC_NUMBER_L3      0  /* Ways, none */

/* 10-entry instruction and data micro-TLBs, 512-entry main TLB */
#define TLB_SIZE_INST   10            /* 4K entries */
#define TLB_SIZE_DATA   10            /* 4K entries */
#define TLB_SIZE_SHARED 512           /* 4K entries */

#define TLB_ASSOC_NUMBER_INST   10    /* Ways (fully associative) */
#define TLB_ASSOC_NUMBER_DATA   10    /* Ways (fully associative) */
#define TLB_ASSOC_NUMBER_SHARED 4     /* Ways */

#else
#error "Benchmarks: unsupported architecture (x86_64 and aarch64 only)"
#endif

#define SYSTEM_PAGE_SIZE 4096         /* Bytes */

/*
 * Last-level cache: L3 where there is one, L2 otherwise. Benchmarks that
 * need "more data than all caches hold" size their data from these.
 */
#if CACHE_SIZE_L3 > 0
#define CACHE_SIZE_LLC      CACHE_SIZE_L3
#define CACHE_LINE_SIZE_LLC CACHE_LINE_SIZE_L3
#else
#define CACHE_SIZE_LLC      CACHE_SIZE_L2
#define CACHE_LINE_SIZE_LLC CACHE_LINE_SIZE_L2
#endif

/* Number of sets: S / (N * L) */
#define CACHE_SETS_NUMBER_L1_INST (CACHE_SIZE_L1_INST /     \
    (CACHE_ASSOC_NUMBER_L1_INST * CACHE_LINE_SIZE_L1_INST))

#define CACHE_SETS_NUMBER_L1_DATA (CACHE_SIZE_L1_DATA /     \
    (CACHE_ASSOC_NUMBER_L1_DATA * CACHE_LINE_SIZE_L1_DATA))

#define CACHE_SETS_NUMBER_L2 (CACHE_SIZE_L2 /     \
    (CACHE_ASSOC_NUMBER_L2 * CACHE_LINE_SIZE_L2))

#if CACHE_SIZE_L3 > 0
#define CACHE_SETS_NUMBER_L3 (CACHE_SIZE_L3 /     \
    (CACHE_ASSOC_NUMBER_L3 * CACHE_LINE_SIZE_L3))
#else
#define CACHE_SETS_NUMBER_L3 0
#endif

#endif /* __BENCH_ARCH_CONFIG_H_ */
