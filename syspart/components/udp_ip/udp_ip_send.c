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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; U suffixes for unsigned constants; deviations
 * are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ret_e (was ret_t).
 */

/**
 * @file
 * The UDP_IP_SENDER component: UDP and IPv4 headers are added before the
 * payload (in the room reserved for them), and the packet goes to the out
 * port.
 */

#include <net/byteorder.h>
#include <net/ip.h>
#include <net/udp.h>

#include <stdio.h>

#include "UDP_IP_SENDER_gen.h"

/**
 * @funcname `fill_in_udp_ip_header`
 *
 * @description
 * Fills in the IPv4 header (with its checksum) and the UDP header (no checksum)
 * of a packet, from the component's addresses and ports.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `state` | `UDP_IP_SENDER_state *` | [in] The component's state. |
 * | `packet` | `udp_ip_packet_t *` | [in,out] The packet. |
 * | `payload_size` | `size_t` | [in] The size of the payload. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `packet->ip_hdr.version_len` to `0x45U`.
 * 2. Set `packet->ip_hdr.dscp` to `0`.
 * 3. Call `hton16()` with the argument
 *    `sizeof(ip_hdr_t) + sizeof(udp_hdr_t) + payload_size`, and store
 *    the result in `packet->ip_hdr.length`.
 * 4. Set `packet->ip_hdr.checksum` to `0`.
 * 5. Set `packet->ip_hdr.id` to `0`.
 * 6. Set `packet->ip_hdr.offset` to `0`.
 * 7. Set `packet->ip_hdr.ttl` to `32`.
 * 8. Set `packet->ip_hdr.proto` to `IPPROTO_UDP`.
 * 9. Call `hton32()` with the argument `state->src_ip`, and store the result in
 *    `packet->ip_hdr.src`.
 * 10. Call `hton32()` with the argument `state->dst_ip`, and store the result
 *     in `packet->ip_hdr.dst`.
 * 11. Call `ip_hdr_checksum()` with the address of `packet->ip_hdr`, and store
 *     the result in `packet->ip_hdr.checksum`.
 * 12. Call `hton16()` with the argument `state->src_port`, and store the result
 *     in `packet->udp_hdr.src_port`.
 * 13. Call `hton16()` with the argument `state->dst_port`, and store the result
 *     in `packet->udp_hdr.dst_port`.
 * 14. Call `hton16()` with the argument
 *     `payload_size + sizeof(udp_hdr_t)`, and store the result in
 *     `packet->udp_hdr.length`.
 * 15. Set `packet->udp_hdr.checksum` to `0`.
 *
 * @calls
 * - `hton16()`
 * - `hton32()`
 * - `ip_hdr_checksum()`
 */
static void fill_in_udp_ip_header(
        UDP_IP_SENDER_state *state,
        udp_ip_packet_t *packet,
        size_t payload_size
        )
{
    // ...next, IP heaader
    packet->ip_hdr.version_len = 0x45U; // IPv4, 5 32-bit words
    packet->ip_hdr.dscp = 0;
    packet->ip_hdr.length = hton16(
            sizeof(ip_hdr_t) +
            sizeof(udp_hdr_t) +
            payload_size
    );
    packet->ip_hdr.checksum = 0; // it's filled in just below
    packet->ip_hdr.id = 0;
    packet->ip_hdr.offset = 0;
    packet->ip_hdr.ttl = 32;
    packet->ip_hdr.proto = IPPROTO_UDP;
    packet->ip_hdr.src = hton32(state->src_ip);
    packet->ip_hdr.dst = hton32(state->dst_ip);

    packet->ip_hdr.checksum = ip_hdr_checksum(&packet->ip_hdr);

    // ... and UDP header
    packet->udp_hdr.src_port = hton16(state->src_port);
    packet->udp_hdr.dst_port = hton16(state->dst_port);
    packet->udp_hdr.length = hton16(payload_size + sizeof(udp_hdr_t));
    packet->udp_hdr.checksum = 0; // no checksum
}

/**
 * @funcname `udp_ip_send`
 *
 * @description
 * The send operation of the component: adds the UDP and IPv4 headers before the
 * payload and sends the packet to the component's destination MAC address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `UDP_IP_SENDER *` | [in] The component. |
 * | `payload` | `char *` | [in,out] The payload. |
 * | `payload_size` | `size_t` | [in] Its size. |
 * | `max_backstep` | `size_t` | [in] The room before the payload for the headers. |
 *
 * @return The result of the out port, or EINVAL if there is no room for the
 *         headers.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `udp_packet` | `void *` | The packet |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `max_backstep` is less than `UDP_IP_HEADER_SIZE`:
 *    1. Return `EINVAL`.
 * 2. Set `udp_packet` to `payload - UDP_IP_HEADER_SIZE`.
 * 3. Call `fill_in_udp_ip_header()` with the arguments the address of
 *    `self->state`, `udp_packet` and `payload_size`.
 * 4. Call `UDP_IP_SENDER_call_portB_mac_send()` with the arguments `self`,
 *    `udp_packet`, `payload_size + UDP_IP_HEADER_SIZE`,
 *    `max_backstep - UDP_IP_HEADER_SIZE`, `self->state.dst_mac` and `ETH_P_IP`,
 *    and return its result.
 *
 * @calls
 * - `fill_in_udp_ip_header()`
 * - `UDP_IP_SENDER_call_portB_mac_send()`
 */
ret_e udp_ip_send(
        UDP_IP_SENDER *self,
        char *payload,
        size_t payload_size,
        size_t max_backstep
        )
{
    if (max_backstep < UDP_IP_HEADER_SIZE) {
        return EINVAL;
    }

    void *udp_packet = payload - UDP_IP_HEADER_SIZE;
    fill_in_udp_ip_header(
        &self->state,
        udp_packet,
        payload_size
    );

    return UDP_IP_SENDER_call_portB_mac_send(self,
            udp_packet,
            payload_size + UDP_IP_HEADER_SIZE,
            max_backstep - UDP_IP_HEADER_SIZE,
            self->state.dst_mac,
            ETH_P_IP);
}

/**
 * @funcname `udp_ip_flush`
 *
 * @description
 * The flush operation of the component: flushes the out port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `UDP_IP_SENDER *` | [in] The component. |
 *
 * @return The result of the out port.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `UDP_IP_SENDER_call_portB_flush()` with the argument `self`, and
 *    return its result.
 *
 * @calls
 * - `UDP_IP_SENDER_call_portB_flush()`
 */
ret_e udp_ip_flush(UDP_IP_SENDER *self) {
    return UDP_IP_SENDER_call_portB_flush(self);
}
