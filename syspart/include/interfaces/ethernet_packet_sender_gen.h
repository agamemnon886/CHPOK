/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/include/interfaces/network.yaml).
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
 */


#include <lib/common.h>
    #include <ret_type.h>
    #include <net/ether.h>

typedef struct {
    ret_t (*mac_send)(
        self_t * arg0,
        char * arg1,
        size_t arg2,
        size_t arg3,
        uint8_t * arg4,
        enum ethertype arg5);
    ret_t (*flush)(
        self_t * arg0);
} ethernet_packet_sender;


#endif

