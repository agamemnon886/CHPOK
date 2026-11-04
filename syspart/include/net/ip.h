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
 * guard isn't a reserved identifier; U suffixes for unsigned constants;
 * deviations are documented where a guideline can't be followed;
 * ip_hdr_payload() doesn't do arithmetic on a void pointer.
 */

#ifndef POK_NET_IP_H
#define POK_NET_IP_H

#include <types.h>

#define IPPROTO_ICMP 1
#define IPPROTO_UDP 17U

struct ip_hdr {
    uint8_t version_len;
    uint8_t dscp; 
    uint16_t length;
    uint16_t id; 
    uint16_t offset; 

    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;

    uint32_t src, dst;
    
    /* MISRA C:2025 Rule 18.7 deviation: the options follow the fixed header. */
    char options[];
} __attribute__((packed)); 

uint16_t ip_hdr_checksum(const struct ip_hdr *ip_hdr);

static inline void* ip_hdr_payload(struct ip_hdr *ip_hdr)
{
    char *p = (char *) ip_hdr;
    return &p[(ip_hdr->version_len & 0xfU) * 4U];
}

#endif
