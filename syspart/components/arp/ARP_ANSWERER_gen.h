/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/arp/config.yaml).
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

#ifndef ARP_ANSWERER_GEN_H
#define ARP_ANSWERER_GEN_H


    #include <interfaces/message_handler_gen.h>

    #include <interfaces/ethernet_packet_sender_gen.h>

/**
 * @typename `ARP_ANSWERER_state`
 *
 * @description
 * The state of an instance of the ARP_ANSWERER component
 * (state_struct in its configuration).
 */
typedef struct {
    uint32_t good_ips[10];
    /**< The IPv4 addresses it answers for. */
    uint32_t good_ips_len;
    /**< The number of addresses in good_ips. */
    uint8_t src_mac[6];
    /**< The MAC address it answers with. */
}ARP_ANSWERER_state;

/**
 * @typename `ARP_ANSWERER`
 *
 * @description
 * An instance of the ARP_ANSWERER component: its state and its
 * ports.
 * It answers the ARP requests it receives (portA) for its IPv4 addresses
 * with its MAC address (portB).
 */
typedef struct {
    ARP_ANSWERER_state state; /**< The state of the instance. */
    struct {
            struct {
                message_handler ops;
            } portA;
    } in;
    /**< The in ports, each with the operations of its interface that the
     * component implements: portA (message_handler). */
    struct {
            struct {
                ethernet_packet_sender *ops;
                self_t *owner;
            } portB;
    } out;
    /**< The out ports, each with the operations of the component it is
     * connected to and that component (owner):
     * portB (ethernet_packet_sender). */
} ARP_ANSWERER;


/** \cond */

      ret_e arp_receive(
          ARP_ANSWERER *self, const char * arg1, size_t arg2);

      ret_e
      ARP_ANSWERER_call_portB_mac_send(
          ARP_ANSWERER *self, char * arg1, size_t arg2, size_t arg3, uint8_t * arg4, ethertype_e arg5);
      ret_e
      ARP_ANSWERER_call_portB_flush(
          ARP_ANSWERER *self);





/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __ARP_ANSWERER_init__(ARP_ANSWERER *self);
void __ARP_ANSWERER_activity__(ARP_ANSWERER *self);
/** \endcond */


#endif
