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
 * guard isn't a reserved identifier; deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: udp_hdr_t is a typedef without a
 * tag, with a member per port; UDP_IP_HEADER_SIZE and udp_ip_packet_t (from the
 * udp_ip component) are here; the macros and types are documented.
 */

#ifndef POK_NET_UDP_H
#define POK_NET_UDP_H

#include <types.h>
#include <net/ip.h>

/**
 * @typename `udp_hdr_t`
 *
 * @description
 * A UDP header, followed by the payload.
 */
typedef struct {
    uint16_t src_port; /**< The source port (network order). */
    uint16_t dst_port; /**< The destination port (network order). */
    /**
     * The length of the header and payload (network order).
     */
    uint16_t length;
    uint16_t checksum; /**< The checksum. */

    /* MISRA C:2025 Rule 18.7 deviation: the UDP payload follows the header. */
    char payload[]; /**< The payload. */
} __attribute__((packed)) udp_hdr_t;

/**
 * @macroname `UDP_IP_HEADER_SIZE`
 *
 * @description
 * The size of the IPv4 and UDP headers of a datagram without IP options, in
 * bytes.
 */
#define UDP_IP_HEADER_SIZE (20U + 8U)

/**
 * @typename `udp_ip_packet_t`
 *
 * @description
 * A UDP datagram in an IPv4 packet: the two headers and the payload.
 */
typedef struct {
    ip_hdr_t ip_hdr; /**< The IPv4 header. */
    udp_hdr_t udp_hdr; /**< The UDP header. */
    /*
     * MISRA C:2025 Rule 18.7 deviation: the payload of a packet follows its
     * header; its length is that of the received or sent frame.
     */
    char payload[]; /**< The payload. */
} __attribute__((packed)) udp_ip_packet_t;

#endif
