/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/udp_ip/config.yaml).
 */
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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types in
 * the generated code are replaced with the fixed-width types from
 * <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; the parameters of the prototypes are
 * named; __<component>_init__() and __<component>_activity__() are
 * declared.
 */

#ifndef UDP_IP_SENDER_GEN_H
#define UDP_IP_SENDER_GEN_H

    #include "ip_addr.h"

    #include <interfaces/preallocated_sender_gen.h>

    #include <interfaces/ethernet_packet_sender_gen.h>

typedef struct UDP_IP_SENDER_state {
    uint32_t src_ip;
    uint16_t src_port;
    uint32_t dst_ip;
    uint16_t dst_port;
    uint8_t dst_mac[6];
}UDP_IP_SENDER_state;

typedef struct {
    UDP_IP_SENDER_state state;
    struct {
            struct {
                preallocated_sender ops;
            } portA;
    } in;
    struct {
            struct {
                ethernet_packet_sender *ops;
                self_t *owner;
            } portB;
    } out;
} UDP_IP_SENDER;



      ret_t udp_ip_send(
          UDP_IP_SENDER *self, char * arg1, size_t arg2, size_t arg3);
      ret_t udp_ip_flush(
          UDP_IP_SENDER *self);

      ret_t
      UDP_IP_SENDER_call_portB_mac_send(
          UDP_IP_SENDER *self, char * arg1, size_t arg2, size_t arg3, uint8_t * arg4, enum ethertype arg5);
      ret_t
      UDP_IP_SENDER_call_portB_flush(
          UDP_IP_SENDER *self);





/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __UDP_IP_SENDER_init__(UDP_IP_SENDER *self);
void __UDP_IP_SENDER_activity__(UDP_IP_SENDER *self);


#endif
