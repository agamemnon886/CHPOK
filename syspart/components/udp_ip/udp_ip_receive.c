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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: discarded
 * results are cast to void; U suffixes for unsigned constants; operands
 * of mixed precedence are parenthesized; deviations are documented where
 * a guideline can't be followed.
 */

#include <net/byteorder.h>
#include <net/ip.h>
#include <net/udp.h>
#include <stdio.h>

#include "UDP_RECEIVER_gen.h"

#define C_NAME "UDP_RECEIVER: "

/*
 * MISRA C:2025 Rule 11.3 deviation (this file): network packets arrive and
 * are built in byte buffers; their headers are accessed through the packed
 * header structures.
 */

ret_t udp_receive(UDP_RECEIVER *self, const char *data, size_t len)
{
    const struct ip_hdr *ip_hdr = (const struct ip_hdr *) data;

    if ((len < sizeof(struct ip_hdr))
        || (len < ((size_t) (ip_hdr->version_len & 0xfU) * 4U))) {
        (void) printf(
            C_NAME"Received packet is too small (IP header doesn't fit).\n");
        return EINVAL;
    }

    if (len != ntoh16(ip_hdr->length)) {
        if (len > ntoh16(ip_hdr->length)) {
            len = ntoh16(ip_hdr->length);
        } else {
            (void) printf(
                C_NAME"Packet length mismatch (received buffer size vs. "
                      "specified in IP header).\n");
            return EINVAL;
        }
    }

    data += (ip_hdr->version_len & 0xfU) * 4U;
    len -= (ip_hdr->version_len & 0xfU) * 4U;

    if (ip_hdr_checksum(ip_hdr) != 0U) {
        (void) printf(
            C_NAME"Discarded IP packet with incorrect header checksum.\n");
        return EINVAL;
    }


    if (ip_hdr->proto != IPPROTO_UDP) {
        (void) printf(C_NAME"it is not UDP\n");
        return EINVAL;
    }

    if (len < sizeof(struct udp_hdr)) {
        (void) printf(C_NAME"Received IP packet is too small (UDP header "
                      "doesn't fit).\n");
        return EINVAL;
    }

    const struct udp_hdr *udp_hdr = (const struct udp_hdr *) data;

    if (ntoh16(udp_hdr->length) != len) {
        (void) printf(
            C_NAME"Packet length mismatch (received buffer size vs. specified "
                  "in UDP header).\n");
        return EINVAL;
    }

    return UDP_RECEIVER_call_portB_udp_message_handle(self,
            udp_hdr->payload,
            len-sizeof(struct udp_hdr),
            ntoh32(ip_hdr->dst),
            ntoh16(udp_hdr->dst_port));
}
