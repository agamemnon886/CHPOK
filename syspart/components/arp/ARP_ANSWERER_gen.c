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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: discarded
 * printf() results are cast to void; the static wrappers aren't named with
 * reserved identifiers.
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: the activity function of a
 * component without an activity discards its unused parameter.
 */

#include <lib/common.h>
#include "ARP_ANSWERER_gen.h"



    static ret_t wrapper_arp_receive(
        self_t *arg0, const char * arg1, size_t arg2)
    {
        return arp_receive((ARP_ANSWERER*) arg0, arg1, arg2);
    }



      ret_t ARP_ANSWERER_call_portB_mac_send(ARP_ANSWERER *self, char * arg1, size_t arg2, size_t arg3, uint8_t * arg4, enum ethertype arg5)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "ARP_ANSWERER was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->mac_send(self->out.portB.owner, arg1, arg2, arg3, arg4, arg5);
      }
      ret_t ARP_ANSWERER_call_portB_flush(ARP_ANSWERER *self)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "ARP_ANSWERER was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->flush(self->out.portB.owner);
      }


void __ARP_ANSWERER_init__(ARP_ANSWERER *self)
{
            self->in.portA.ops.handle = wrapper_arp_receive;

}

void __ARP_ANSWERER_activity__(ARP_ANSWERER *self)
{
        (void) self; // the component has no activity
}
