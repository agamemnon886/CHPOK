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
 * Modified by Kirill Brazhnikov, 2026-10-31: printf format fix for
 * 64-bit targets.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; discarded results are cast to void; U suffixes
 * for unsigned constants; operands of mixed precedence are parenthesized.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ret_e (was ret_t).
 */

/**
 * @file
 * The ROUTER component: UDP packets go to the out port of their
 * destination IP address and port.
 */

#include <net/byteorder.h>
#include <net/ip.h>
#include <net/udp.h>
#include <stdio.h>

#include "ROUTER_gen.h"

/**
 * @funcname `get_ip_port_index`
 *
 * @description
 * Returns the index of an (IP address, port) pair in the component's map.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `state` | `ROUTER_state *` | [in] The component's state. |
 * | `ip` | `uint32_t` | [in] The IP address. |
 * | `port` | `uint16_t` | [in] The port. |
 *
 * @return The index, or -1 if it isn't there.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cur_pair` | `udp_ip_pair_t *` | The pair |
 * | `i` | `size_t` | The index of a pair |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than
 *    `state->map_ip_port_to_idx_len`, repeat (after each pass: increment `i`):
 *    1. Set `cur_pair` to the address of `state->map_ip_port_to_idx[i]`.
 *    2. If `cur_pair->ip` is equal to `ip` and `cur_pair->port` is equal to
 *       `port`:
 *       1. Return `(int32_t) i`.
 * 2. Return `-1`.
 *
 * @calls
 * None.
 */
static int32_t get_ip_port_index(ROUTER_state *state, uint32_t ip, uint16_t port)
{
    for (size_t i = 0U; i < state->map_ip_port_to_idx_len; i++) {
        udp_ip_pair_t *cur_pair = &state->map_ip_port_to_idx[i];
        if ((cur_pair->ip == ip) && (cur_pair->port == port)) {
            return (int32_t) i;
        }
    }
    return -1;
}

/**
 * @funcname `receive_packet`
 *
 * @description
 * The udp_message_handle operation of the component: passes a packet to the out
 * port of its destination.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ROUTER *` | [in] The component. |
 * | `payload` | `const char *` | [in] The UDP payload. |
 * | `payload_size` | `size_t` | [in] Its size. |
 * | `ip` | `uint32_t` | [in] The destination IP address. |
 * | `port` | `uint16_t` | [in] The destination port. |
 *
 * @return EOK, or EINVAL if the destination isn't in the map.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `idx` | `int32_t` | The index of the destination |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_ip_port_index()` with the arguments the address of
 *    `self->state`, `ip` and `port`, and store the result in `idx`.
 * 2. If `idx` is less than `0`:
 *    1. Call `printf()` with the arguments
 *       `"ROUTER: packet not for us (from %%u.%%u.%%u.%%u:%%d)\n"`,
 *       `IP_PRINT(ip)`, `IP_PRINT(ip)`, `IP_PRINT(ip)`, `IP_PRINT(ip)` and
 *       `port`.
 *    2. Return `EINVAL`.
 * 3. Call `ROUTER_call_portArray_handle_by_index()` with the arguments `idx`,
 *    `self`, `payload` and `payload_size`.
 * 4. Return `EOK`.
 *
 * @calls
 * - `get_ip_port_index()`
 * - `printf()`
 * - `ROUTER_call_portArray_handle_by_index()`
 */
ret_e receive_packet(ROUTER *self, const char *payload, size_t payload_size,
    uint32_t ip, uint16_t port)
{
    int32_t idx = get_ip_port_index(&self->state, ip, port);

    if (idx < 0) {
        (void) printf(
            "ROUTER: packet not for us (from %u.%u.%u.%u:%d)\n", IP_PRINT(ip),
                port);
        return EINVAL;
    }
    (void) ROUTER_call_portArray_handle_by_index(
        idx, self, payload, payload_size);

    return EOK;
}
