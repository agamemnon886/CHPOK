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
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: discarded
 * results are cast to void; the header guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 */

#ifndef JET_IOPORTS_H
#define JET_IOPORTS_H

#include <string.h>

#ifdef __x86_64__
#include <arch/x86_64/ioports.h>
#endif

/*
 * Reads an 8-bit device register (not implemented for x86_64 yet).
 */
static inline uint8_t  ioread8 (uint8_t  *addr)
{
    (void) addr;
    return 0;
}

/*
 * Reads a 16-bit device register (not implemented for x86_64 yet).
 */
static inline uint16_t ioread16(uint16_t *addr)
{
    (void) addr;
    return 0;
}

/*
 * Reads a 32-bit device register (not implemented for x86_64 yet).
 */
static inline uint32_t ioread32(uint32_t *addr)
{
    (void) addr;
    return 0;
}

/*
 * Writes an 8-bit device register (not implemented for x86_64 yet: ignored).
 */
static inline void iowrite8(uint8_t value, uint8_t *addr)
{
    (void) value;
    (void) addr;
}

/*
 * Writes a 16-bit device register (not implemented for x86_64 yet: ignored).
 */
static inline void iowrite16(uint16_t value, uint16_t *addr)
{
    (void) value;
    (void) addr;
}

/*
 * Writes a 32-bit device register (not implemented for x86_64 yet: ignored).
 */
static inline void iowrite32(uint32_t value, uint32_t *addr)
{
    (void) value;
    (void) addr;
}

/*
 * Fills device memory with a byte value.
 */
static inline void memset_io(volatile void *addr, uint8_t val, size_t count)
{
    (void) memset((void *)addr, val, count);
}

/*
 * Copies bytes from device memory.
 */
static inline void memcpy_fromio(void *dst, const volatile void *src, size_t count)
{
    (void) memcpy(dst, (const void *)src, count);
}

/*
 * Copies bytes to device memory.
 */
static inline void memcpy_toio(volatile void *dst, const void *src, size_t count)
{
    (void) memcpy((void *)dst, src, count);
}

#endif
