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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ret_e (was ret_t).
 */

/**
 * @file
 * The UDP_RECEIVER component: IPv4 packets are checked and their UDP
 * payload goes to the out port.
 */

#include <net/byteorder.h>
#include <net/ip.h>
#include <net/udp.h>
#include <stdio.h>

#include "UDP_RECEIVER_gen.h"

/*
 * MISRA C:2025 Rule 11.3 deviation (this file): network packets arrive and
 * are built in byte buffers; their headers are accessed through the packed
 * header structures.
 */

/**
 * @funcname `udp_receive`
 *
 * @description
 * The handle operation of the component: checks the IPv4 header (size, length,
 * checksum, protocol) and the UDP header, then passes the payload with its
 * destination address and port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `UDP_RECEIVER *` | [in] The component. |
 * | `data` | `const char *` | [in] The IPv4 packet. |
 * | `len` | `size_t` | [in] Its length. |
 *
 * @return The result of the out port, or EINVAL for a bad packet.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ip_hdr` | `const ip_hdr_t *` | The IPv4 header |
 * | `udp_hdr` | `const udp_hdr_t *` | The UDP header |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ip_hdr` to `(const ip_hdr_t *) data`.
 * 2. If `len` is less than `sizeof(ip_hdr_t)` or `len` is less than
 *    `((size_t) (ip_hdr->version_len & 0xfU) * 4U)`:
 *    1. Call `printf()` with the argument
 *       `"UDP_RECEIVER: Received packet is too small "`
 *       `"(IP header doesn't fit).\n"`.
 *    2. Return `EINVAL`.
 * 3. If `len` is not equal to `ntoh16(ip_hdr->length)`:
 *    1. If `len` is greater than `ntoh16(ip_hdr->length)`:
 *       1. Call `ntoh16()` with the argument `ip_hdr->length`, and store the
 *          result in `len`.
 *    2. Otherwise:
 *       1. Call `printf()` with the argument
 *          `"UDP_RECEIVER: Packet length mismatch "`
 *          `"(received buffer size vs. "` `"specified in IP header).\n"`.
 *       2. Return `EINVAL`.
 * 4. Add `(ip_hdr->version_len & 0xfU) * 4U` to `data`.
 * 5. Subtract `(ip_hdr->version_len & 0xfU) * 4U` from `len`.
 * 6. If `ip_hdr_checksum(ip_hdr)` is not equal to `0U`:
 *    1. Call `printf()` with the argument
 *       `"UDP_RECEIVER: Discarded IP packet with incorrect "`
 *       `"header checksum.\n"`.
 *    2. Return `EINVAL`.
 * 7. If `ip_hdr->proto` is not equal to `IPPROTO_UDP`:
 *    1. Call `printf()` with the argument `"UDP_RECEIVER: it is not UDP\n"`.
 *    2. Return `EINVAL`.
 * 8. If `len` is less than `sizeof(udp_hdr_t)`:
 *    1. Call `printf()` with the argument
 *       `"UDP_RECEIVER: Received IP packet is too small "`
 *       `"(UDP header doesn't fit).\n"`.
 *    2. Return `EINVAL`.
 * 9. Set `udp_hdr` to `(const udp_hdr_t *) data`.
 * 10. If `ntoh16(udp_hdr->length)` is not equal to `len`:
 *     1. Call `printf()` with the argument
 *        `"UDP_RECEIVER: Packet length mismatch "`
 *        `"(received buffer size vs. specified "` `"in UDP header).\n"`.
 *     2. Return `EINVAL`.
 * 11. Call `UDP_RECEIVER_call_portB_udp_message_handle()` with the arguments
 *     `self`, `udp_hdr->payload`, `len-sizeof(udp_hdr_t)`,
 *     `ntoh32(ip_hdr->dst)` and `ntoh16(udp_hdr->dst_port)`, and return its
 *     result.
 *
 * @calls
 * - `printf()`
 * - `ntoh16()`
 * - `ip_hdr_checksum()`
 * - `UDP_RECEIVER_call_portB_udp_message_handle()`
 * - `ntoh32()`
 */
ret_e udp_receive(UDP_RECEIVER *self, const char *data, size_t len)
{
    const ip_hdr_t *ip_hdr = (const ip_hdr_t *) data;

    if ((len < sizeof(ip_hdr_t))
        || (len < ((size_t) (ip_hdr->version_len & 0xfU) * 4U))) {
        (void) printf("UDP_RECEIVER: Received packet is too small "
                      "(IP header doesn't fit).\n");
        return EINVAL;
    }

    if (len != ntoh16(ip_hdr->length)) {
        if (len > ntoh16(ip_hdr->length)) {
            len = ntoh16(ip_hdr->length);
        } else {
            (void) printf("UDP_RECEIVER: Packet length mismatch "
                          "(received buffer size vs. "
                          "specified in IP header).\n");
            return EINVAL;
        }
    }

    data += (ip_hdr->version_len & 0xfU) * 4U;
    len -= (ip_hdr->version_len & 0xfU) * 4U;

    if (ip_hdr_checksum(ip_hdr) != 0U) {
        (void) printf("UDP_RECEIVER: Discarded IP packet with incorrect "
                      "header checksum.\n");
        return EINVAL;
    }

    if (ip_hdr->proto != IPPROTO_UDP) {
        (void) printf("UDP_RECEIVER: it is not UDP\n");
        return EINVAL;
    }

    if (len < sizeof(udp_hdr_t)) {
        (void) printf("UDP_RECEIVER: Received IP packet is too small "
                      "(UDP header doesn't fit).\n");
        return EINVAL;
    }

    const udp_hdr_t *udp_hdr = (const udp_hdr_t *) data;

    if (ntoh16(udp_hdr->length) != len) {
        (void) printf("UDP_RECEIVER: Packet length mismatch "
                      "(received buffer size vs. specified "
                      "in UDP header).\n");
        return EINVAL;
    }

    return UDP_RECEIVER_call_portB_udp_message_handle(self,
            udp_hdr->payload,
            len-sizeof(udp_hdr_t),
            ntoh32(ip_hdr->dst),
            ntoh16(udp_hdr->dst_port));
}
