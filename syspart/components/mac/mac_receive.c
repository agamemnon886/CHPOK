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
 * results are cast to void; essentially Boolean conditions; U suffixes
 * for unsigned constants; deviations are documented where a guideline
 * can't be followed; an unused update of data is removed.
 */

#include <net/byteorder.h>
#include <net/ether.h>

#include <stdio.h>
#include <string.h>

#include "MAC_RECEIVER_gen.h"

#define MAC_HEADER_SIZE 14U

/*
 * MISRA C:2025 Rule 11.3 deviation (this file): network packets arrive and
 * are built in byte buffers; their headers are accessed through the packed
 * header structures.
 */

#define C_NAME "MAC_RECEIVER: "
ret_t mac_receive(MAC_RECEIVER *self, const char *data, size_t len)
{
    // TODO validate checksums, TTL, and all that stuff

    if (len < sizeof(struct ether_hdr)) {
        (void) printf(
            C_NAME"Received packet is too small (even Ethernet header doesn't "
                  "fit).");
        return EINVAL;
    }

    const struct ether_hdr *ether_hdr = (const struct ether_hdr *) data;
    len -= sizeof(*ether_hdr);

    if ((ether_is_multicast(ether_hdr->dst) == 0) &&
        (memcmp(ether_hdr->dst, self->state.my_mac, ETH_ALEN) != 0))
    {
        (void) printf(C_NAME"packet NOT for us: dst= %x %x %x %x %x %x\n",
            ether_hdr->dst[0],
            ether_hdr->dst[1],
            ether_hdr->dst[2],
            ether_hdr->dst[3],
            ether_hdr->dst[4],
            ether_hdr->dst[5]);
        // it's not for us
        return EINVAL;
    }

    if (ether_hdr->ethertype == hton16(ETH_P_ARP)) {
        return MAC_RECEIVER_call_port_ARP_handle(self, ether_hdr->payload, len);
    } else if(ether_hdr->ethertype == hton16(ETH_P_IP)){
        return MAC_RECEIVER_call_port_UDP_handle(self, ether_hdr->payload, len);
    } else {
        // we don't know anything except IPv4
        return EINVAL;
    }



}
