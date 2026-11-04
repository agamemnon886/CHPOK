/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/mac/config.yaml).
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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: discarded
 * printf() results are cast to void; the static wrappers aren't named with
 * reserved identifiers.
 */

#include <lib/common.h>
#include "MAC_RECEIVER_gen.h"



    static ret_t wrapper_mac_receive(
        self_t *arg0, const char * arg1, size_t arg2)
    {
        return mac_receive((MAC_RECEIVER*) arg0, arg1, arg2);
    }



      ret_t MAC_RECEIVER_call_port_UDP_handle(MAC_RECEIVER *self, const char * arg1, size_t arg2)
      {
         if (self->out.port_UDP.ops == NULL) {
             (void) printf("WRONG CONFIG: out port port_UDP of component "
                           "MAC_RECEIVER was not initialized\n");
             //fatal_error?
         }
         return self->out.port_UDP.ops->handle(self->out.port_UDP.owner, arg1, arg2);
      }
      ret_t MAC_RECEIVER_call_port_ARP_handle(MAC_RECEIVER *self, const char * arg1, size_t arg2)
      {
         if (self->out.port_ARP.ops == NULL) {
             (void) printf("WRONG CONFIG: out port port_ARP of component "
                           "MAC_RECEIVER was not initialized\n");
             //fatal_error?
         }
         return self->out.port_ARP.ops->handle(self->out.port_ARP.owner, arg1, arg2);
      }


void __MAC_RECEIVER_init__(MAC_RECEIVER *self)
{
            self->in.portA.ops.handle = wrapper_mac_receive;

}

void __MAC_RECEIVER_activity__(MAC_RECEIVER *self)
{
}
