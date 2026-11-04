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
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; deviations are documented where a
 * guideline can't be followed.
 */

/* Memory-mapped device registers (arm64 has no I/O ports). */

#ifndef JET_ARM64_IOPORTS_H
#define JET_ARM64_IOPORTS_H

#include <stdint.h>

/*
 * MISRA C:2025 Rule 11.4 deviation (the functions below): a device register
 * is accessed at its physical address, which is converted to a pointer to
 * volatile. Dir 1.2 and Dir 4.3: the memory barriers are inline assembly,
 * a GNU extension, kept in these small functions.
 */
static inline uint32_t ja_mmio_read32(uintptr_t addr)
{
    uint32_t val = *(volatile uint32_t*)addr;
    asm volatile("dmb oshld" ::: "memory");
    return val;
}

static inline void ja_mmio_write32(uintptr_t addr, uint32_t val)
{
    asm volatile("dmb oshst" ::: "memory");
    *(volatile uint32_t*)addr = val;
}

static inline void ja_mmio_write8(uintptr_t addr, uint8_t val)
{
    asm volatile("dmb oshst" ::: "memory");
    *(volatile uint8_t*)addr = val;
}

#endif /* JET_ARM64_IOPORTS_H */
