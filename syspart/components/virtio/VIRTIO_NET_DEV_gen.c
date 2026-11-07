/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/virtio/config.yaml).
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
#include "VIRTIO_NET_DEV_gen.h"



    static ret_t wrapper_send_frame(
        self_t *arg0, char * arg1, size_t arg2, size_t arg3)
    {
        return send_frame((VIRTIO_NET_DEV*) arg0, arg1, arg2, arg3);
    }

    static ret_t wrapper_flush_send(
        self_t *arg0)
    {
        return flush_send((VIRTIO_NET_DEV*) arg0);
    }



      ret_t VIRTIO_NET_DEV_call_portB_handle(VIRTIO_NET_DEV *self, const char * arg1, size_t arg2)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "VIRTIO_NET_DEV was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->handle(self->out.portB.owner, arg1, arg2);
      }


void __VIRTIO_NET_DEV_init__(VIRTIO_NET_DEV *self)
{
            self->in.portA.ops.send = wrapper_send_frame;
            self->in.portA.ops.flush = wrapper_flush_send;

        virtio_init(self);
}

void __VIRTIO_NET_DEV_activity__(VIRTIO_NET_DEV *self)
{
        virtio_receive_activity(self);
}
