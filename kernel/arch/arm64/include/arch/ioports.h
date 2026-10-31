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

/* Memory-mapped device registers (arm64 has no I/O ports). */

#ifndef __JET_ARM64_IOPORTS_H__
#define __JET_ARM64_IOPORTS_H__

#include <stdint.h>

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

#endif /* __JET_ARM64_IOPORTS_H__ */
