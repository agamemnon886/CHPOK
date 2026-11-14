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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: the function declarations are
 * hidden from Doxygen (\cond), which would otherwise document the functions a
 * second time, in the section of this header.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 */

#ifndef POK_NET_IP_H
#define POK_NET_IP_H

#include <types.h>

/**
 * @macroname `IPPROTO_ICMP`
 *
 * @description
 * The protocol number of ICMP.
 */
#define IPPROTO_ICMP 1

/**
 * @macroname `IPPROTO_UDP`
 *
 * @description
 * The protocol number of UDP.
 */
#define IPPROTO_UDP 17U

/**
 * @typename `ip_hdr_t`
 *
 * @description
 * An IPv4 header, followed by its options.
 */
typedef struct {
    /**
     * The version (4) and the header length in 32-bit words.
     */
    uint8_t version_len;
    uint8_t dscp;  /**< The type of service. */
    uint16_t length; /**< The total length (network order). */
    uint16_t id;  /**< The identification. */
    uint16_t offset;  /**< The flags and the fragment offset. */

    uint8_t ttl; /**< The time to live. */
    uint8_t proto; /**< The protocol of the payload (IPPROTO_*). */
    uint16_t checksum; /**< The header checksum. */

    uint32_t src; /**< The source address. */
    uint32_t dst; /**< The destination address. */
    
    /* MISRA C:2025 Rule 18.7 deviation: the options follow the fixed header. */
    char options[]; /**< The options. */
} __attribute__((packed)) ip_hdr_t; 

/** \cond */
uint16_t ip_hdr_checksum(const ip_hdr_t *ip_hdr);
/** \endcond */

/*
 * Returns the payload of an IPv4 packet (after its header and options).
 */
static inline void* ip_hdr_payload(ip_hdr_t *ip_hdr)
{
    char *p = (char *) ip_hdr;
    return &p[(ip_hdr->version_len & 0xfU) * 4U];
}

#endif
