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
 * AArch64 instructions used by the hypervisor: system registers,
 * barriers, cache and TLB maintenance, and the exclusive accesses of the
 * console lock.
 */

/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (this file): system
 * registers and privileged instructions are reached with GNU inline
 * assembly, isolated in these macros and small static inline functions;
 * HV_READ_SYSREG() is a GNU statement expression, so that it yields the
 * value read. Rule 20.10 (advisory): # makes the register name a part of
 * the instruction.
 */

#ifndef HV_ARCH_ARM64_H
#define HV_ARCH_ARM64_H

#include <stdint.h>
#include <stdbool.h>

#define HV_READ_SYSREG(reg) ({                              \
    uint64_t sysreg_value;                                  \
    asm volatile ("mrs %0, " #reg : "=r" (sysreg_value));   \
    sysreg_value;                                           \
})

#define HV_WRITE_SYSREG(reg, value) do {                    \
    uint64_t sysreg_value = (uint64_t) (value);             \
    asm volatile ("msr " #reg ", %0" : : "r" (sysreg_value) \
                  : "memory");                              \
} while (false)

// Instruction synchronization barrier.
static inline void arm64_isb(void)
{
    asm volatile ("isb" : : : "memory");
}

// Data synchronization barrier (inner shareable domain).
static inline void arm64_dsb_ish(void)
{
    asm volatile ("dsb ish" : : : "memory");
}

// Data memory barrier (inner shareable domain).
static inline void arm64_dmb_ish(void)
{
    asm volatile ("dmb ish" : : : "memory");
}

// Mask all exceptions (D, A, I, F) at the current exception level.
static inline void arm64_mask_all(void)
{
    asm volatile ("msr daifset, #0xf" : : : "memory");
}

// Wait for an interrupt.
static inline void arm64_wfi(void)
{
    asm volatile ("wfi" : : : "memory");
}

// Spin-wait hint.
static inline void arm64_yield(void)
{
    asm volatile ("yield" : : : "memory");
}

// Clean the data cache line at a virtual address to the point of coherency.
static inline void arm64_dc_cvac(uint64_t address)
{
    asm volatile ("dc cvac, %0" : : "r" (address) : "memory");
}

// Invalidate all instruction caches to the point of unification.
static inline void arm64_ic_iallu(void)
{
    asm volatile ("ic iallu" : : : "memory");
}

/*
 * Invalidate the stage 1 and stage 2 TLB entries of the current VMID (the
 * one in VTTBR_EL2) on this CPU.
 */
static inline void arm64_tlbi_vmalls12e1(void)
{
    asm volatile ("tlbi vmalls12e1" : : : "memory");
}

// Atomic exchange of a 32-bit word, with acquire semantics (spin locks).
static inline uint32_t arm64_xchg32(volatile uint32_t *word, uint32_t value)
{
    uint32_t old;
    uint32_t failed;

    asm volatile ("1: ldaxr %w0, %2\n"
                  "   stxr %w1, %w3, %2\n"
                  "   cbnz %w1, 1b"
                  : "=&r" (old), "=&r" (failed), "+Q" (*word)
                  : "r" (value)
                  : "memory");
    return old;
}

#endif // HV_ARCH_ARM64_H
