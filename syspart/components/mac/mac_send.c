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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ret_e (was ret_t).
 */

/**
 * @file
 * The MAC_SENDER component: an Ethernet header is added before the payload
 * (in the room reserved for it), and the frame is sent through the out
 * port.
 */

#include <net/byteorder.h>

#include <stdio.h>
#include <string.h>

#include "MAC_SENDER_gen.h"


/*
 * MISRA C:2025 Rule 11.3 deviation (this file): network packets arrive and
 * are built in byte buffers; their headers are accessed through the packed
 * header structures.
 */

/**
 * @funcname `fill_in_mac_header`
 *
 * @description
 * Fills in the Ethernet header of a frame: the component's MAC address, the
 * destination and the type.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `state` | `MAC_SENDER_state *` | [in] The component's state. |
 * | `packet` | `mac_packet_t *` | [out] The frame. |
 * | `dst_mac` | `uint8_t *` | [in] The destination MAC address. |
 * | `type` | `ethertype_e` | [in] The Ethernet type. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ether_hdr` | `ether_hdr_t *` | The Ethernet header |
 * | `i` | `int32_t` | The index of a byte of a MAC address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ether_hdr` to the address of `packet->ether_hdr`.
 * 2. Set `i` to `0`; then while `i` is less than `ETH_ALEN`, repeat (after each
 *    pass: increment `i`):
 *    1. Set `ether_hdr->src[i]` to `state->src_mac[i]`.
 *    2. Set `ether_hdr->dst[i]` to `dst_mac[i]`.
 * 3. Call `hton16()` with the argument `type`, and store the result in
 *    `ether_hdr->ethertype`.
 *
 * @calls
 * - `hton16()`
 */
static void fill_in_mac_header(
        MAC_SENDER_state *state,
        mac_packet_t *packet,
        uint8_t *dst_mac,
        ethertype_e type)
{
    ether_hdr_t *ether_hdr = &packet->ether_hdr;

    for (int32_t i = 0; i < ETH_ALEN; i++) {
        ether_hdr->src[i] = state->src_mac[i];
        ether_hdr->dst[i] = dst_mac[i];
    }
    ether_hdr->ethertype = hton16(type);

}

/**
 * @funcname `mac_sender_send`
 *
 * @description
 * The mac_send operation of the component: adds the Ethernet header before the
 * payload and sends the frame.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_SENDER *` | [in] The component. |
 * | `payload` | `char *` | [out] The payload. |
 * | `payload_size` | `size_t` | [in] Its size. |
 * | `max_backstep` | `size_t` | [in] The room before the payload for the headers. |
 * | `dst_mac_addr` | `uint8_t *` | [in] The destination MAC address. |
 * | `ethertype` | `ethertype_e` | [in] The Ethernet type. |
 *
 * @return EOK, or EINVAL if there is no room for the header.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mac_packet` | `void *` | The frame |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `max_backstep` is less than `MAC_HEADER_SIZE`:
 *    1. Return `EINVAL`.
 * 2. Set `mac_packet` to `payload - MAC_HEADER_SIZE`.
 * 3. Call `fill_in_mac_header()` with the arguments the address of
 *    `self->state`, `mac_packet`, `dst_mac_addr` and `ethertype`.
 * 4. Call `MAC_SENDER_call_portB_send()` with the arguments `self`,
 *    `mac_packet`, `payload_size + MAC_HEADER_SIZE` and
 *    `max_backstep - MAC_HEADER_SIZE`.
 * 5. Return `EOK`.
 *
 * @calls
 * - `fill_in_mac_header()`
 * - `MAC_SENDER_call_portB_send()`
 */
ret_e mac_sender_send(MAC_SENDER *self,
        char *payload,
        size_t payload_size,
        size_t max_backstep,
        uint8_t *dst_mac_addr,
        ethertype_e ethertype
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


/**
 * @funcname `mac_flush`
 *
 * @description
 * The flush operation of the component: flushes the out port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_SENDER *` | [in] The component. |
 *
 * @return EOK.
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
 * 1. Call `MAC_SENDER_call_portB_flush()` with the argument `self`.
 * 2. Return `EOK`.
 *
 * @calls
 * - `MAC_SENDER_call_portB_flush()`
 */
ret_e mac_flush(MAC_SENDER *self) {
    (void) MAC_SENDER_call_portB_flush(self);
    return EOK;
}
