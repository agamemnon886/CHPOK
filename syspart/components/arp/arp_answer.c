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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: discarded
 * results are cast to void; essentially Boolean conditions; U suffixes
 * for unsigned constants; operands of mixed precedence are parenthesized;
 * deviations are documented where a guideline can't be followed; the
 * received packet is accessed through a pointer to const.
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: arp_receive() rejects
 * packets that are too short for an ARP packet before reading them.
 */

#include <net/byteorder.h>

#include <stdio.h>
#include "ARP_ANSWERER_gen.h"

#define C_NAME "ARP: "

struct arp_packet_t {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[ETH_ALEN];
    uint32_t spa;
    uint8_t tha[ETH_ALEN];
    uint32_t tpa;
} __attribute__((packed));

static struct {
    struct ether_hdr ether_hdr;
    struct arp_packet_t arp_answer;
} __attribute__((packed)) arp_answer_buffer;


/*
 * MISRA C:2025 Rule 11.3 deviation (this file): network packets arrive and
 * are built in byte buffers; their headers are accessed through the packed
 * header structures.
 */

ret_t arp_receive(ARP_ANSWERER *self, const char *data, size_t len)
{
    const struct arp_packet_t *arp_packet = (const struct arp_packet_t *) data;

    if (len < sizeof(*arp_packet)) {
        (void) printf(C_NAME"short arp packet\n");
        return EINVAL; // Shorter than an Ethernet/IPv4 ARP packet.
    }
    if (arp_packet->htype != hton16(1)) {
        (void) printf(C_NAME"wrong arp packet\n");
        return EINVAL; // We support only Ethernet hardware type.
    }
    if (arp_packet->ptype != hton16(ETH_P_IP)) {
        (void) printf(C_NAME"wrong arp packet\n");
        return EINVAL; // We support only IPv4 protocol type.
    }
    if ((arp_packet->hlen != (uint8_t) ETH_ALEN) || (arp_packet->plen != 4U)) {
        (void) printf(C_NAME"wrong arp packet\n");
        return EINVAL; // We support Ethernet MAC and IPv4 addresses only.
    }
    if (arp_packet->oper != hton16(1)) {
        (void) printf(C_NAME"wrong arp packet\n");
        return EINVAL; // This is not an ARP request.
    }
    int32_t found = 0;
    for (uint32_t i = 0U; i < self->state.good_ips_len; i++) {
        if (arp_packet->tpa == hton32(self->state.good_ips[i])) {
            found = 1;
            break;
        }
    }
    if (found == 0) {
        (void) printf(C_NAME"bad ip\n");
        return EINVAL; // This ARP request is not for us.
    }
    (void) printf("ARP_ANSWERER: we have received a request for our MAC.\n");

    int32_t i;
    for (i = 0; i < ETH_ALEN; i++) {
        arp_answer_buffer.arp_answer.sha[i] = self->state.src_mac[i];
        arp_answer_buffer.arp_answer.tha[i] = arp_packet->sha[i];
    }

    arp_answer_buffer.arp_answer.htype = arp_packet->htype;
    arp_answer_buffer.arp_answer.ptype = arp_packet->ptype;
    arp_answer_buffer.arp_answer.hlen = arp_packet->hlen;
    arp_answer_buffer.arp_answer.plen = arp_packet->plen;
    arp_answer_buffer.arp_answer.oper = hton16(2); // This is an ARP answer.
    arp_answer_buffer.arp_answer.spa = arp_packet->tpa;
    arp_answer_buffer.arp_answer.tpa = arp_packet->spa;

    (void) ARP_ANSWERER_call_portB_mac_send(self,
            (void *)&arp_answer_buffer.arp_answer,
            sizeof(arp_answer_buffer.arp_answer),
            sizeof(struct ether_hdr),
            arp_answer_buffer.arp_answer.tha,
            ETH_P_ARP);


    (void) ARP_ANSWERER_call_portB_flush(self);

    return EOK;
}
