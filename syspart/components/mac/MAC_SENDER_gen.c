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
#include "MAC_SENDER_gen.h"



    static ret_t wrapper_mac_sender_send(
        self_t *arg0, char * arg1, size_t arg2, size_t arg3, uint8_t * arg4, enum ethertype arg5)
    {
        return mac_sender_send((MAC_SENDER*) arg0, arg1, arg2, arg3, arg4, arg5);
    }

    static ret_t wrapper_mac_flush(
        self_t *arg0)
    {
        return mac_flush((MAC_SENDER*) arg0);
    }



      ret_t MAC_SENDER_call_portB_send(MAC_SENDER *self, char * arg1, size_t arg2, size_t arg3)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "MAC_SENDER was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->send(self->out.portB.owner, arg1, arg2, arg3);
      }
      ret_t MAC_SENDER_call_portB_flush(MAC_SENDER *self)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "MAC_SENDER was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->flush(self->out.portB.owner);
      }


void __MAC_SENDER_init__(MAC_SENDER *self)
{
            self->in.portA.ops.mac_send = wrapper_mac_sender_send;
            self->in.portA.ops.flush = wrapper_mac_flush;

}

void __MAC_SENDER_activity__(MAC_SENDER *self)
{
}
