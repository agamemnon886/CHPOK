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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 */

#ifndef POK_NET_ETHER_H
#define POK_NET_ETHER_H

#include <types.h>

/**
 * @macroname `ETH_ALEN`
 *
 * @description
 * The size of a MAC address.
 */
#define ETH_ALEN 6

/**
 * @macroname `ETH_DATA_LENGTH`
 *
 * @description
 * The maximum size of the payload of a frame (the MTU).
 */
#define ETH_DATA_LENGTH 1500

/**
 * @typename `ethertype_e`
 *
 * @description
 * The type of the payload of an Ethernet frame.
 */
typedef enum {
    ETH_P_IP = 0x0800, /**< IPv4. */
    ETH_P_ARP = 0x0806 /**< ARP. */
} ethertype_e;

/**
 * @typename `ether_hdr_t`
 *
 * @description
 * An Ethernet header, followed by the payload.
 */
typedef struct {
    uint8_t dst[ETH_ALEN]; /**< The destination MAC address. */
    uint8_t src[ETH_ALEN]; /**< The source MAC address. */
    /**
     * The type of the payload (ethertype_e, network order).
     */
    uint16_t ethertype;
    /* MISRA C:2025 Rule 18.7 deviation: the frame's payload follows the
     * header. */
    char payload[]; /**< The payload. */
} __attribute__((packed)) ether_hdr_t;

/*
 * Tells whether an Ethernet address is a multicast (or broadcast) one.
 */
static inline int32_t ether_is_multicast(const uint8_t addr[ETH_ALEN])
{
    return (addr[0] & 0x01U) != 0U;
}

/**
 * @macroname `MAC_HEADER_SIZE`
 *
 * @description
 * The size of an Ethernet header, in bytes.
 */
#define MAC_HEADER_SIZE 14U

/**
 * @typename `mac_packet_t`
 *
 * @description
 * An Ethernet frame: its header and its payload.
 */
typedef struct {
    ether_hdr_t ether_hdr; /**< The Ethernet header. */
    /*
     * MISRA C:2025 Rule 18.7 deviation: the payload of a packet follows its
     * header; its length is that of the received or sent frame.
     */
    char payload[]; /**< The payload. */
} __attribute__((packed)) mac_packet_t;

/**
 * @typename `arp_packet_t`
 *
 * @description
 * An ARP packet for IPv4 over Ethernet.
 */
typedef struct {
    uint16_t htype; /**< The hardware type (1: Ethernet). */
    uint16_t ptype; /**< The protocol type (ETH_P_IP). */
    uint8_t hlen; /**< The length of a hardware address (ETH_ALEN). */
    uint8_t plen; /**< The length of a protocol address (4). */
    uint16_t oper; /**< The operation (1: request, 2: reply). */
    uint8_t sha[ETH_ALEN]; /**< The sender's hardware address. */
    uint32_t spa; /**< The sender's protocol address. */
    uint8_t tha[ETH_ALEN]; /**< The target's hardware address. */
    uint32_t tpa; /**< The target's protocol address. */
} __attribute__((packed)) arp_packet_t;

/**
 * @typename `arp_answer_t`
 *
 * @description
 * An ARP reply in an Ethernet frame (the buffer of the ARP component).
 */
typedef struct {
    ether_hdr_t ether_hdr; /**< The Ethernet header. */
    arp_packet_t arp_answer; /**< The ARP reply. */
} __attribute__((packed)) arp_answer_t;

#endif
