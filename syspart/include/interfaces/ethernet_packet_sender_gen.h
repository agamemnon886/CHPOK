/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/include/interfaces/network.yaml).
 */
/*
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef INTERFACES_ETHERNET_PACKET_SENDER_H
#define INTERFACES_ETHERNET_PACKET_SENDER_H

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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; the parameters of the operations are
 * named.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen comments for the file,
 * its type and the type's members (the descriptions are in the interface's
 * configuration: description).
 */


#include <lib/common.h>
    #include <ret_type.h>
    #include <net/ether.h>

/**
 * @typename `ethernet_packet_sender_mac_send_fn_t`
 *
 * @description
 * The function mac_send of the ethernet_packet_sender interface.
 * Sends a payload in an Ethernet frame: arg0 is the component, arg1 the
 * payload, arg2 its size, arg3 the room before it for the headers
 * (max_backstep), arg4 the destination MAC address and arg5 the type of the
 * frame. Returns EOK or an error.
 */
typedef ret_e (*ethernet_packet_sender_mac_send_fn_t)(
    self_t * arg0,
    char * arg1,
    size_t arg2,
    size_t arg3,
    uint8_t * arg4,
    ethertype_e arg5);

/**
 * @typename `ethernet_packet_sender_flush_fn_t`
 *
 * @description
 * The function flush of the ethernet_packet_sender interface.
 * Sends what the component (arg0) has queued. Returns EOK or an error.
 */
typedef ret_e (*ethernet_packet_sender_flush_fn_t)(
    self_t * arg0);

/**
 * @typename `ethernet_packet_sender`
 *
 * @description
 * The operations of a port of the ethernet_packet_sender interface: those of
 * the component that implements it.
 * Sends a payload in an Ethernet frame to a MAC address.
 */
typedef struct {
    ethernet_packet_sender_mac_send_fn_t mac_send;
    /**< Sends a payload in an Ethernet frame: arg0 is the component, arg1
     * the payload, arg2 its size, arg3 the room before it for the
     * headers (max_backstep), arg4 the destination MAC address and arg5
     * the type of the frame. Returns EOK or an error.
     */
    ethernet_packet_sender_flush_fn_t flush;
    /**< Sends what the component (arg0) has queued. Returns EOK or an
     * error.
     */
} ethernet_packet_sender;


#endif

