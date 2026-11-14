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
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen comments for the file,
 * its types and their members (the descriptions are in the component's
 * configuration: description and state_doc); the function declarations are
 * hidden from Doxygen (\cond).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef VIRTIO_NET_DEV_GEN_H
#define VIRTIO_NET_DEV_GEN_H

    #include "virtio_network_device.h"

    #include <interfaces/preallocated_sender_gen.h>

    #include <interfaces/message_handler_gen.h>

/**
 * @typename `VIRTIO_NET_DEV_state`
 *
 * @description
 * The state of an instance of the VIRTIO_NET_DEV component
 * (state_struct in its configuration).
 */
typedef struct {
    uint8_t pci_bus;
    /**< The PCI bus of the card. */
    uint8_t pci_dev;
    /**< The PCI device of the card. */
    uint8_t pci_fn;
    /**< The PCI function of the card. */
    virtio_network_device_t info;
    /**< The state of the device (set by virtio_init(), not by the
     * configuration). */
}VIRTIO_NET_DEV_state;

/**
 * @typename `VIRTIO_NET_DEV`
 *
 * @description
 * An instance of the VIRTIO_NET_DEV component: its state and its
 * ports.
 * The driver of a virtio network card: it sends the frames it is given
 * (portA) and passes the frames it receives on (portB).
 */
typedef struct {
    VIRTIO_NET_DEV_state state; /**< The state of the instance. */
    struct {
            struct {
                preallocated_sender ops;
            } portA;
    } in;
    /**< The in ports, each with the operations of its interface that the
     * component implements: portA (preallocated_sender). */
    struct {
            struct {
                message_handler *ops;
                self_t *owner;
            } portB;
    } out;
    /**< The out ports, each with the operations of the component it is
     * connected to and that component (owner):
     * portB (message_handler). */
} VIRTIO_NET_DEV;


/** \cond */

      ret_e send_frame(
          VIRTIO_NET_DEV *self, char * arg1, size_t arg2, size_t arg3);
      ret_e flush_send(
          VIRTIO_NET_DEV *self);

      ret_e
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
/** \endcond */


#endif
