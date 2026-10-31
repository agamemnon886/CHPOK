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
 * Modified by Kirill Brazhnikov, 2026-10-31: x86_64 support; removed
 * PowerPC, x86 (32-bit) and SPARC support.
 */

#ifndef __JET__IOPORTS_H__
#define __JET__IOPORTS_H__

#include <string.h>

#ifdef __x86_64__
#include <arch/x86_64/ioports.h>
#endif

/*
 * MMIO accessors (used by the qemu_vga and radeon drivers). They are not
 * implemented for x86_64 yet: reads return 0 and writes are ignored.
 */
static inline uint8_t  ioread8 (uint8_t  *addr)
{
    (void) addr;
    return 0;
}

static inline uint16_t ioread16(uint16_t *addr)
{
    (void) addr;
    return 0;
}

static inline uint32_t ioread32(uint32_t *addr)
{
    (void) addr;
    return 0;
}

static inline void iowrite8(uint8_t value, uint8_t *addr)
{
    (void) value;
    (void) addr;
}

static inline void iowrite16(uint16_t value, uint16_t *addr)
{
    (void) value;
    (void) addr;
}

static inline void iowrite32(uint32_t value, uint32_t *addr)
{
    (void) value;
    (void) addr;
}

static inline void memset_io(volatile void *addr, unsigned char val, size_t count)
{
    memset((void *)addr, val, count);
}

static inline void memcpy_fromio(void *dst, const volatile void *src, size_t count)
{
    memcpy(dst, (const void *)src, count);
}

static inline void memcpy_toio(volatile void *dst, const void *src, size_t count)
{
    memcpy((void *)dst, src, count);
}

#endif
