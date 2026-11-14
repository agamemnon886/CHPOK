/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/include/interfaces/network.yaml).
 */
/*
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef INTERFACES_UDP_MESSAGE_HANDLER_H
#define INTERFACES_UDP_MESSAGE_HANDLER_H

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

/**
 * @typename `udp_message_handler_udp_message_handle_fn_t`
 *
 * @description
 * The function udp_message_handle of the udp_message_handler interface.
 * Processes the payload of a UDP datagram: arg0 is the component, arg1 the
 * payload, arg2 its size, arg3 the destination IPv4 address and arg4 the
 * destination UDP port. Returns EOK or an error.
 */
typedef ret_e (*udp_message_handler_udp_message_handle_fn_t)(
    self_t * arg0,
    const char * arg1,
    size_t arg2,
    uint32_t arg3,
    uint16_t arg4);

/**
 * @typename `udp_message_handler`
 *
 * @description
 * The operations of a port of the udp_message_handler interface: those of
 * the component that implements it.
 * Receives the payload of a UDP datagram for processing.
 */
typedef struct {
    udp_message_handler_udp_message_handle_fn_t udp_message_handle;
    /**< Processes the payload of a UDP datagram: arg0 is the component,
     * arg1 the payload, arg2 its size, arg3 the destination IPv4 address
     * and arg4 the destination UDP port. Returns EOK or an error.
     */
} udp_message_handler;


#endif

