/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/arinc/config.yaml).
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
#include "ARINC_SENDER_gen.h"





      ret_t ARINC_SENDER_call_portA_send(ARINC_SENDER *self, char * arg1, size_t arg2, size_t arg3)
      {
         if (self->out.portA.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portA of component "
                           "ARINC_SENDER was not initialized\n");
             //fatal_error?
         }
         return self->out.portA.ops->send(self->out.portA.owner, arg1, arg2, arg3);
      }
      ret_t ARINC_SENDER_call_portA_flush(ARINC_SENDER *self)
      {
         if (self->out.portA.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portA of component "
                           "ARINC_SENDER was not initialized\n");
             //fatal_error?
         }
         return self->out.portA.ops->flush(self->out.portA.owner);
      }


void __ARINC_SENDER_init__(ARINC_SENDER *self)
{

        arinc_sender_init(self);
}

void __ARINC_SENDER_activity__(ARINC_SENDER *self)
{
        arinc_sender_activity(self);
}
