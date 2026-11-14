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

#ifndef ROUTER_GEN_H
#define ROUTER_GEN_H

    #include "state_structs.h"
    #include "ip_addr.h"

    #include <interfaces/udp_message_handler_gen.h>

    #include <interfaces/message_handler_gen.h>

/**
 * @typename `ROUTER_state`
 *
 * @description
 * The state of an instance of the ROUTER component
 * (state_struct in its configuration).
 */
typedef struct {
    udp_ip_pair_t map_ip_port_to_idx[10];
    /**< The destinations (IPv4 address and UDP port) and the index of
     * their port in portArray. */
    size_t map_ip_port_to_idx_len;
    /**< The number of destinations in map_ip_port_to_idx. */
}ROUTER_state;

/**
 * @typename `ROUTER`
 *
 * @description
 * An instance of the ROUTER component: its state and its
 * ports.
 * It passes each UDP payload it receives (portA) to the port of portArray
 * that its destination address and port select.
 */
typedef struct {
    ROUTER_state state; /**< The state of the instance. */
    struct {
            struct {
                udp_message_handler ops;
            } portA;
    } in;
    /**< The in ports, each with the operations of its interface that the
     * component implements: portA (udp_message_handler). */
    struct {
            struct {
                message_handler *ops;
                self_t *owner;
            } *portArray;
    } out;
    /**< The out ports, each with the operations of the component it is
     * connected to and that component (owner):
     * portArray (message_handler). */
} ROUTER;


/** \cond */

      ret_e receive_packet(
          ROUTER *self, const char * arg1, size_t arg2, uint32_t arg3, uint16_t arg4);

      ret_e
      ROUTER_call_portArray_handle_by_index(
          int32_t idx, ROUTER *self, const char * arg1, size_t arg2);





/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __ROUTER_init__(ROUTER *self);
void __ROUTER_activity__(ROUTER *self);
/** \endcond */


#endif
