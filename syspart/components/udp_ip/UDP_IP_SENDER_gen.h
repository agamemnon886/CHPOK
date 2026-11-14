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
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen comments for the file,
 * its types and their members (the descriptions are in the component's
 * configuration: description and state_doc); the function declarations are
 * hidden from Doxygen (\cond).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef UDP_IP_SENDER_GEN_H
#define UDP_IP_SENDER_GEN_H

    #include "ip_addr.h"

    #include <interfaces/preallocated_sender_gen.h>

    #include <interfaces/ethernet_packet_sender_gen.h>

/**
 * @typename `UDP_IP_SENDER_state`
 *
 * @description
 * The state of an instance of the UDP_IP_SENDER component
 * (state_struct in its configuration).
 */
typedef struct {
    uint32_t src_ip;
    /**< The source IPv4 address of the packets. */
    uint16_t src_port;
    /**< Their source UDP port. */
    uint32_t dst_ip;
    /**< Their destination IPv4 address. */
    uint16_t dst_port;
    /**< Their destination UDP port. */
    uint8_t dst_mac[6];
    /**< The MAC address the packets are sent to. */
}UDP_IP_SENDER_state;

/**
 * @typename `UDP_IP_SENDER`
 *
 * @description
 * An instance of the UDP_IP_SENDER component: its state and its
 * ports.
 * It adds the UDP and IPv4 headers to the payloads it is given (portA) and
 * sends the packets to a MAC address (portB).
 */
typedef struct {
    UDP_IP_SENDER_state state; /**< The state of the instance. */
    struct {
            struct {
                preallocated_sender ops;
            } portA;
    } in;
    /**< The in ports, each with the operations of its interface that the
     * component implements: portA (preallocated_sender). */
    struct {
            struct {
                ethernet_packet_sender *ops;
                self_t *owner;
            } portB;
    } out;
    /**< The out ports, each with the operations of the component it is
     * connected to and that component (owner):
     * portB (ethernet_packet_sender). */
} UDP_IP_SENDER;


/** \cond */

      ret_e udp_ip_send(
          UDP_IP_SENDER *self, char * arg1, size_t arg2, size_t arg3);
      ret_e udp_ip_flush(
          UDP_IP_SENDER *self);

      ret_e
      UDP_IP_SENDER_call_portB_mac_send(
          UDP_IP_SENDER *self, char * arg1, size_t arg2, size_t arg3, uint8_t * arg4, ethertype_e arg5);
      ret_e
      UDP_IP_SENDER_call_portB_flush(
          UDP_IP_SENDER *self);





/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __UDP_IP_SENDER_init__(UDP_IP_SENDER *self);
void __UDP_IP_SENDER_activity__(UDP_IP_SENDER *self);
/** \endcond */


#endif
