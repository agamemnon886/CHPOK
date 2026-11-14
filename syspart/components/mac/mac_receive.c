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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ret_e (was ret_t).
 */

/**
 * @file
 * The MAC_RECEIVER component: Ethernet frames for the partition's MAC
 * address (or multicast) go to the ARP or the IP handler.
 */

#include <net/byteorder.h>
#include <net/ether.h>

#include <stdio.h>
#include <string.h>

#include "MAC_RECEIVER_gen.h"

/*
 * MISRA C:2025 Rule 11.3 deviation (this file): network packets arrive and
 * are built in byte buffers; their headers are accessed through the packed
 * header structures.
 */

/**
 * @funcname `mac_receive`
 *
 * @description
 * The handle operation of the component: checks the Ethernet header of a frame
 * and passes its payload to the ARP or the UDP (IPv4) out port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_RECEIVER *` | [in] The component. |
 * | `data` | `const char *` | [in] The frame. |
 * | `len` | `size_t` | [in] Its length. |
 *
 * @return The result of the handler, or EINVAL for a frame that is too short,
 *         not for the partition or of another type.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ether_hdr` | `const ether_hdr_t *` | The Ethernet header |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `len` is less than `sizeof(ether_hdr_t)`:
 *    1. Call `printf()` with the argument
 *       `"MAC_RECEIVER: Received packet is too small (even Ethernet "`
 *       `"header doesn't fit)."`.
 *    2. Return `EINVAL`.
 * 2. Set `ether_hdr` to `(const ether_hdr_t *) data`.
 * 3. Subtract `sizeof(*ether_hdr)` from `len`.
 * 4. If `ether_is_multicast(ether_hdr->dst)` is equal to `0` and
 *    `memcmp(ether_hdr->dst, self->state.my_mac, ETH_ALEN)` is not equal to
 *    `0`:
 *    1. Call `printf()` with the arguments
 *       `"MAC_RECEIVER: packet NOT for us: dst= %%x %%x %%x %%x %%x %%x\n"`,
 *       `ether_hdr->dst[0]`, `ether_hdr->dst[1]`, `ether_hdr->dst[2]`,
 *       `ether_hdr->dst[3]`, `ether_hdr->dst[4]` and `ether_hdr->dst[5]`.
 *    2. Return `EINVAL`.
 * 5. If `ether_hdr->ethertype` is equal to `hton16(ETH_P_ARP)`:
 *    1. Call `MAC_RECEIVER_call_port_ARP_handle()` with the arguments `self`,
 *       `ether_hdr->payload` and `len`, and return its result.
 * 6. Otherwise, if `ether_hdr->ethertype` is equal to `hton16(ETH_P_IP)`:
 *    1. Call `MAC_RECEIVER_call_port_UDP_handle()` with the arguments `self`,
 *       `ether_hdr->payload` and `len`, and return its result.
 * 7. Otherwise:
 *    1. Return `EINVAL`.
 *
 * @calls
 * - `printf()`
 * - `ether_is_multicast()`
 * - `memcmp()`
 * - `hton16()`
 * - `MAC_RECEIVER_call_port_ARP_handle()`
 * - `MAC_RECEIVER_call_port_UDP_handle()`
 */
ret_e mac_receive(MAC_RECEIVER *self, const char *data, size_t len)
{
    // TODO validate checksums, TTL, and all that stuff

    if (len < sizeof(ether_hdr_t)) {
        (void) printf(
            "MAC_RECEIVER: Received packet is too small (even Ethernet "
            "header doesn't fit).");
        return EINVAL;
    }

    const ether_hdr_t *ether_hdr = (const ether_hdr_t *) data;
    len -= sizeof(*ether_hdr);

    if ((ether_is_multicast(ether_hdr->dst) == 0) &&
        (memcmp(ether_hdr->dst, self->state.my_mac, ETH_ALEN) != 0))
    {
        (void) printf(
            "MAC_RECEIVER: packet NOT for us: dst= %x %x %x %x %x %x\n",
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
