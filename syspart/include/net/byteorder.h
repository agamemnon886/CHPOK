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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 */

#ifndef POK_NET_BYTEORDER_H
#define POK_NET_BYTEORDER_H

#include <stdint.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

/*
 * Converts a 16-bit value from the CPU's byte order to the network's.
 */
static inline uint16_t hton16(uint16_t x)
{
    return (uint16_t) (((x >> 8) & 0xFFU) |
                       ((x & 0xFFU) << 8));
}

/*
 * Converts a 16-bit value from the network's byte order to the CPU's.
 */
static inline uint16_t ntoh16(uint16_t x)
{
    return hton16(x);
}

/*
 * Converts a 32-bit value from the CPU's byte order to the network's.
 */
static inline uint32_t hton32(uint32_t x)
{
    return __builtin_bswap32(x);
}

/*
 * Converts a 32-bit value from the network's byte order to the CPU's.
 */
static inline uint32_t ntoh32(uint32_t x)
{
    return __builtin_bswap32(x);
}


#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

/**
 * @macroname `F`
 *
 * @description
 * Defines the static inline function `NAME`, which returns its argument as it
 * is (byte order conversion on a big endian CPU); undefined after its uses.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `TYPE` | The type of the value. |
 * | `NAME` | The name of the function. |
 */
#define F(TYPE, NAME) \
    static inline TYPE NAME(TYPE x) { return x; }

F(uint16_t, hton16)
F(uint16_t, ntoh16)
F(uint32_t, ntoh32)
F(uint32_t, hton32)

#undef F

#else
    #error "unrecognized endianness"
#endif

#endif
