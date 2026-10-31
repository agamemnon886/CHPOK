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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 */

#ifndef __VIRTIO_NETWORK_DEVICE_H__
#define __VIRTIO_NETWORK_DEVICE_H__
#include <stdint.h>

#include "virtio_virtqueue.h"
#include "virtio_net.h"
#include "virtio_pci.h"
#include <pci.h>


#define POK_MAX_RECEIVE_BUFFERS 100

#define MAX_PACKET_SIZE 1500

struct receive_buffer {
    struct virtio_net_hdr virtio_net_hdr;
    char packet[MAX_PACKET_SIZE];
} __attribute__((packed));

struct send_buffer {
    char data[MAX_PACKET_SIZE];
} __attribute__((packed));

struct virtio_network_device {
    s_pci_dev pci_device;

    struct virtio_virtqueue rx_vq, tx_vq;

    uint8_t mac[6];

    void (*packet_received_callback)(const char *, size_t);

    struct receive_buffer receive_buffers[POK_MAX_RECEIVE_BUFFERS];
    struct send_buffer *send_buffers;
    int32_t inited;
};

#endif
