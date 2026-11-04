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

#ifndef UDP_RECEIVER_GEN_H
#define UDP_RECEIVER_GEN_H

    #include "state_structs.h"
    #include "ip_addr.h"

    #include <interfaces/message_handler_gen.h>

    #include <interfaces/udp_message_handler_gen.h>

typedef struct UDP_RECEIVER_state {
}UDP_RECEIVER_state;

typedef struct {
    UDP_RECEIVER_state state;
    struct {
            struct {
                message_handler ops;
            } portA;
    } in;
    struct {
            struct {
                udp_message_handler *ops;
                self_t *owner;
            } portB;
    } out;
} UDP_RECEIVER;



      ret_t udp_receive(
          UDP_RECEIVER *self, const char * arg1, size_t arg2);

      ret_t
      UDP_RECEIVER_call_portB_udp_message_handle(
          UDP_RECEIVER *self, const char * arg1, size_t arg2, uint32_t arg3, uint16_t arg4);





/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __UDP_RECEIVER_init__(UDP_RECEIVER *self);
void __UDP_RECEIVER_activity__(UDP_RECEIVER *self);


#endif
