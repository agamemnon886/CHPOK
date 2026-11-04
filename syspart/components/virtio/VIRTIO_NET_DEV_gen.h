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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; the parameters of the prototypes are
 * named; __<component>_init__() and __<component>_activity__() are
 * declared.
 */

#ifndef VIRTIO_NET_DEV_GEN_H
#define VIRTIO_NET_DEV_GEN_H

    #include "virtio_network_device.h"

    #include <interfaces/preallocated_sender_gen.h>

    #include <interfaces/message_handler_gen.h>

typedef struct VIRTIO_NET_DEV_state {
    uint8_t pci_bus;
    uint8_t pci_dev;
    uint8_t pci_fn;
    struct virtio_network_device info;
}VIRTIO_NET_DEV_state;

typedef struct {
    VIRTIO_NET_DEV_state state;
    struct {
            struct {
                preallocated_sender ops;
            } portA;
    } in;
    struct {
            struct {
                message_handler *ops;
                self_t *owner;
            } portB;
    } out;
} VIRTIO_NET_DEV;



      ret_t send_frame(
          VIRTIO_NET_DEV *self, char * arg1, size_t arg2, size_t arg3);
      ret_t flush_send(
          VIRTIO_NET_DEV *self);

      ret_t
      VIRTIO_NET_DEV_call_portB_handle(
          VIRTIO_NET_DEV *self, const char * arg1, size_t arg2);



    void virtio_init(VIRTIO_NET_DEV *self);

    void virtio_receive_activity(VIRTIO_NET_DEV *self);

/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __VIRTIO_NET_DEV_init__(VIRTIO_NET_DEV *self);
void __VIRTIO_NET_DEV_activity__(VIRTIO_NET_DEV *self);


#endif
