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

#include <net/byteorder.h>

#include <stdio.h>
#include <string.h>

#include "MAC_SENDER_gen.h"


#define MAC_HEADER_SIZE 14

struct mac_packet {
    struct ether_hdr ether_hdr;
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

ret_t mac_send(MAC_SENDER *self,
        char *payload,
        size_t payload_size,
        size_t max_backstep,
        uint8_t *dst_mac_addr,
        enum ethertype ethertype
        )
{
    if (max_backstep < MAC_HEADER_SIZE)
        return EINVAL;

    void *mac_packet = payload - MAC_HEADER_SIZE;
    fill_in_mac_header(
        &self->state,
        mac_packet,
        dst_mac_addr,
        ethertype
        );

    MAC_SENDER_call_portB_send(self,
            mac_packet,
            payload_size + MAC_HEADER_SIZE,
            max_backstep - MAC_HEADER_SIZE
            );
    return EOK;
}


ret_t mac_flush(MAC_SENDER *self) {
    MAC_SENDER_call_portB_flush(self);
    return EOK;
}
