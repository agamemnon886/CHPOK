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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; discarded results are cast to void; U suffixes
 * for unsigned constants; deviations are documented where a guideline
 * can't be followed; mac_send() is renamed mac_sender_send() (the
 * interface member is named mac_send).
 */

#include <net/byteorder.h>

#include <stdio.h>
#include <string.h>

#include "MAC_SENDER_gen.h"


#define MAC_HEADER_SIZE 14U

/*
 * MISRA C:2025 Rule 11.3 deviation (this file): network packets arrive and
 * are built in byte buffers; their headers are accessed through the packed
 * header structures.
 */

struct mac_packet {
    struct ether_hdr ether_hdr;
    /*
     * MISRA C:2025 Rule 18.7 deviation: the payload of a packet follows its
     * header; its length is that of the received or sent frame.
     */
    char payload[];
} __attribute__((packed));

static void fill_in_mac_header(
        MAC_SENDER_state *state,
        struct mac_packet *packet,
        uint8_t *dst_mac,
        enum ethertype type)
{
    struct ether_hdr *ether_hdr = &packet->ether_hdr;

    for (int32_t i = 0; i < ETH_ALEN; i++) {
        ether_hdr->src[i] = state->src_mac[i];
        ether_hdr->dst[i] = dst_mac[i];
    }
    ether_hdr->ethertype = hton16(type);

}

ret_t mac_sender_send(MAC_SENDER *self,
        char *payload,
        size_t payload_size,
        size_t max_backstep,
        uint8_t *dst_mac_addr,
        enum ethertype ethertype
        )
{
    if (max_backstep < MAC_HEADER_SIZE) {
        return EINVAL;
    }

    void *mac_packet = payload - MAC_HEADER_SIZE;
    fill_in_mac_header(
        &self->state,
        mac_packet,
        dst_mac_addr,
        ethertype
        );

    (void) MAC_SENDER_call_portB_send(self,
            mac_packet,
            payload_size + MAC_HEADER_SIZE,
            max_backstep - MAC_HEADER_SIZE
            );
    return EOK;
}


ret_t mac_flush(MAC_SENDER *self) {
    (void) MAC_SENDER_call_portB_flush(self);
    return EOK;
}
