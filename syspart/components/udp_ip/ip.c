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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: U suffixes for
 * unsigned constants; operands of mixed precedence are parenthesized; the
 * header checksum reads the header as bytes (the 16-bit words of the
 * packed header may be unaligned).
 */

#include <net/ip.h>

uint16_t ip_hdr_checksum(const struct ip_hdr *ip_hdr)
{
    uint32_t acc = 0;

    // The header is read as bytes (Rule 11.3 allows a pointer to a
    // character type): its 16-bit words may be unaligned in the packed
    // structure. Each word is assembled in the byte order of the CPU
    // (little-endian on x86_64 and arm64), as a uint16_t load did.
    const uint8_t *bytes = (const uint8_t *) ip_hdr;

    uint32_t i;
    for (i = 0U; i < ((ip_hdr->version_len & 0xfU) * 2U); i++) {
        acc += (uint32_t) bytes[2U * i]
               | ((uint32_t) bytes[(2U * i) + 1U] << 8);
    }

    uint32_t folded = (acc & 0xFFFFU) + (acc >> 16);
    return (uint16_t) ~folded;
}
