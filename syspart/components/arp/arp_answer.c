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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ret_e (was ret_t).
 */

/**
 * @file
 * The ARP_ANSWERER component: answers the ARP requests for the IP
 * addresses of the partition.
 */

#include <net/byteorder.h>

#include <stdio.h>
#include "ARP_ANSWERER_gen.h"

static arp_answer_t arp_answer_buffer;

/*
 * MISRA C:2025 Rule 11.3 deviation (this file): network packets arrive and
 * are built in byte buffers; their headers are accessed through the packed
 * header structures.
 */

/**
 * @funcname `arp_receive`
 *
 * @description
 * The handle operation of the component: checks an ARP request (Ethernet, IPv4,
 * for one of the component's addresses) and sends the answer with the
 * component's MAC address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARP_ANSWERER *` | [in] The component. |
 * | `data` | `const char *` | [in] The ARP packet. |
 * | `len` | `size_t` | [in] Its length. |
 *
 * @return EOK, or EINVAL for a packet that isn't such a request.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arp_answer_buffer` | `struct (anonymous)` | The answer: an Ethernet header and an ARP packet |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arp_packet` | `const arp_packet_t *` | The request |
 * | `found` | `int32_t` | Whether the address is one of the component's |
 * | `i` | `int32_t` | The index of an address, then of a byte of a MAC address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `arp_packet` to `(const arp_packet_t *) data`.
 * 2. If `len` is less than `sizeof(*arp_packet)`:
 *    1. Call `printf()` with the argument `"ARP: short arp packet\n"`.
 *    2. Return `EINVAL`.
 * 3. If `arp_packet->htype` is not equal to `hton16(1)`:
 *    1. Call `printf()` with the argument `"ARP: wrong arp packet\n"`.
 *    2. Return `EINVAL`.
 * 4. If `arp_packet->ptype` is not equal to `hton16(ETH_P_IP)`:
 *    1. Call `printf()` with the argument `"ARP: wrong arp packet\n"`.
 *    2. Return `EINVAL`.
 * 5. If `arp_packet->hlen` is not equal to `(uint8_t) ETH_ALEN` or
 *    `arp_packet->plen` is not equal to `4U`:
 *    1. Call `printf()` with the argument `"ARP: wrong arp packet\n"`.
 *    2. Return `EINVAL`.
 * 6. If `arp_packet->oper` is not equal to `hton16(1)`:
 *    1. Call `printf()` with the argument `"ARP: wrong arp packet\n"`.
 *    2. Return `EINVAL`.
 * 7. Set `found` to `0`.
 * 8. Set `i` to `0U`; then while `i` is less than `self->state.good_ips_len`,
 *    repeat (after each pass: increment `i`):
 *    1. If `arp_packet->tpa` is equal to `hton32(self->state.good_ips[i])`:
 *       1. Set `found` to `1`.
 *       2. Leave the loop.
 * 9. If `found` is equal to `0`:
 *    1. Call `printf()` with the argument `"ARP: bad ip\n"`.
 *    2. Return `EINVAL`.
 * 10. Call `printf()` with the argument
 *     `"ARP_ANSWERER: we have received a request for our MAC.\n"`.
 * 11. Set `i` to `0`; then while `i` is less than `ETH_ALEN`, repeat (after
 *     each pass: increment `i`):
 *     1. Set `arp_answer_buffer.arp_answer.sha[i]` to `self->state.src_mac[i]`.
 *     2. Set `arp_answer_buffer.arp_answer.tha[i]` to `arp_packet->sha[i]`.
 * 12. Set `arp_answer_buffer.arp_answer.htype` to `arp_packet->htype`.
 * 13. Set `arp_answer_buffer.arp_answer.ptype` to `arp_packet->ptype`.
 * 14. Set `arp_answer_buffer.arp_answer.hlen` to `arp_packet->hlen`.
 * 15. Set `arp_answer_buffer.arp_answer.plen` to `arp_packet->plen`.
 * 16. Call `hton16()` with the argument `2`, and store the result in
 *     `arp_answer_buffer.arp_answer.oper`.
 * 17. Set `arp_answer_buffer.arp_answer.spa` to `arp_packet->tpa`.
 * 18. Set `arp_answer_buffer.arp_answer.tpa` to `arp_packet->spa`.
 * 19. Call `ARP_ANSWERER_call_portB_mac_send()` with the arguments `self`,
 *     `(void *)&arp_answer_buffer.arp_answer`,
 *     `sizeof(arp_answer_buffer.arp_answer)`, `sizeof(ether_hdr_t)`,
 *     `arp_answer_buffer.arp_answer.tha` and `ETH_P_ARP`.
 * 20. Call `ARP_ANSWERER_call_portB_flush()` with the argument `self`.
 * 21. Return `EOK`.
 *
 * @calls
 * - `printf()`
 * - `hton16()`
 * - `hton32()`
 * - `ARP_ANSWERER_call_portB_mac_send()`
 * - `ARP_ANSWERER_call_portB_flush()`
 */
ret_e arp_receive(ARP_ANSWERER *self, const char *data, size_t len)
{
    const arp_packet_t *arp_packet = (const arp_packet_t *) data;

    if (len < sizeof(*arp_packet)) {
        (void) printf("ARP: short arp packet\n");
        return EINVAL; // Shorter than an Ethernet/IPv4 ARP packet.
    }
    if (arp_packet->htype != hton16(1)) {
        (void) printf("ARP: wrong arp packet\n");
        return EINVAL; // We support only Ethernet hardware type.
    }
    if (arp_packet->ptype != hton16(ETH_P_IP)) {
        (void) printf("ARP: wrong arp packet\n");
        return EINVAL; // We support only IPv4 protocol type.
    }
    if ((arp_packet->hlen != (uint8_t) ETH_ALEN) || (arp_packet->plen != 4U)) {
        (void) printf("ARP: wrong arp packet\n");
        return EINVAL; // We support Ethernet MAC and IPv4 addresses only.
    }
    if (arp_packet->oper != hton16(1)) {
        (void) printf("ARP: wrong arp packet\n");
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
        (void) printf("ARP: bad ip\n");
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
            sizeof(ether_hdr_t),
            arp_answer_buffer.arp_answer.tha,
            ETH_P_ARP);

    (void) ARP_ANSWERER_call_portB_flush(self);

    return EOK;
}
