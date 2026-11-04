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

#ifndef ROUTER_GEN_H
#define ROUTER_GEN_H

    #include "state_structs.h"
    #include "ip_addr.h"

    #include <interfaces/udp_message_handler_gen.h>

    #include <interfaces/message_handler_gen.h>

typedef struct ROUTER_state {
    struct udp_ip_pair map_ip_port_to_idx[10];
    size_t map_ip_port_to_idx_len;
}ROUTER_state;

typedef struct {
    ROUTER_state state;
    struct {
            struct {
                udp_message_handler ops;
            } portA;
    } in;
    struct {
            struct {
                message_handler *ops;
                self_t *owner;
            } *portArray;
    } out;
} ROUTER;



      ret_t receive_packet(
          ROUTER *self, const char * arg1, size_t arg2, uint32_t arg3, uint16_t arg4);

      ret_t
      ROUTER_call_portArray_handle_by_index(
          int32_t idx, ROUTER *self, const char * arg1, size_t arg2);





/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __ROUTER_init__(ROUTER *self);
void __ROUTER_activity__(ROUTER *self);


#endif
