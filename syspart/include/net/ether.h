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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants;
 * deviations are documented where a guideline can't be followed.
 */

#ifndef POK_NET_ETHER_H
#define POK_NET_ETHER_H

#include <types.h>

#define ETH_ALEN 6
#define ETH_DATA_LENGTH 1500

enum ethertype {
    ETH_P_IP = 0x0800,
    ETH_P_ARP = 0x0806
};

struct ether_hdr {
    uint8_t dst[ETH_ALEN];
    uint8_t src[ETH_ALEN];
    uint16_t ethertype;
    /* MISRA C:2025 Rule 18.7 deviation: the frame's payload follows the
     * header. */
    char payload[];
} __attribute__((packed));

static inline int32_t ether_is_multicast(const uint8_t addr[ETH_ALEN])
{
    return (addr[0] & 0x01U) != 0U;
}

#endif
