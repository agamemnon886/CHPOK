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

#ifndef UDP_RECEIVER_GEN_H
#define UDP_RECEIVER_GEN_H

    #include "state_structs.h"
    #include "ip_addr.h"

    #include <interfaces/message_handler_gen.h>

    #include <interfaces/udp_message_handler_gen.h>

/**
 * @typename `UDP_RECEIVER_state`
 *
 * @description
 * The state of an instance of the UDP_RECEIVER component
 * (state_struct in its configuration).
 */
typedef struct {
}UDP_RECEIVER_state;

/**
 * @typename `UDP_RECEIVER`
 *
 * @description
 * An instance of the UDP_RECEIVER component: its state and its
 * ports.
 * It checks the IPv4 and UDP headers of the packets it receives (portA) and
 * passes their payload, with its destination address and port, on (portB).
 */
typedef struct {
    UDP_RECEIVER_state state; /**< The state of the instance. */
    struct {
            struct {
                message_handler ops;
            } portA;
    } in;
    /**< The in ports, each with the operations of its interface that the
     * component implements: portA (message_handler). */
    struct {
            struct {
                udp_message_handler *ops;
                self_t *owner;
            } portB;
    } out;
    /**< The out ports, each with the operations of the component it is
     * connected to and that component (owner):
     * portB (udp_message_handler). */
} UDP_RECEIVER;


/** \cond */

      ret_e udp_receive(
          UDP_RECEIVER *self, const char * arg1, size_t arg2);

      ret_e
      UDP_RECEIVER_call_portB_udp_message_handle(
          UDP_RECEIVER *self, const char * arg1, size_t arg2, uint32_t arg3, uint16_t arg4);





/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __UDP_RECEIVER_init__(UDP_RECEIVER *self);
void __UDP_RECEIVER_activity__(UDP_RECEIVER *self);
/** \endcond */


#endif
