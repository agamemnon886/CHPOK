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
 */

#include <net/ip.h>

uint16_t ip_hdr_checksum(const struct ip_hdr *ip_hdr)
{
    uint32_t acc = 0;

    // TODO does it break aliasing?
    const uint16_t *words = (const uint16_t*) ip_hdr;

    int32_t i;
    for (i = 0; i < (ip_hdr->version_len & 0xf) * 2; i++) {
        acc += words[i];
    }

    return ~((acc & 0xFFFF) + (acc >> 16));
}
