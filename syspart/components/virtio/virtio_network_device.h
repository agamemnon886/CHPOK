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
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: MISRA C:2025: MAX_PACKET_SIZE
 * is an unsigned constant, as it is compared with sizes.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of virtio_network.c are
 * here; the callback has the type virtio_packet_received_fn_t; the types are
 * typedefs without a tag, documented with the macros.
 */

#ifndef VIRTIO_NETWORK_DEVICE_H
#define VIRTIO_NETWORK_DEVICE_H
#include <stdint.h>

#include "virtio_virtqueue.h"
#include "virtio_net.h"
#include "virtio_pci.h"
#include <pci.h>


/**
 * @macroname `POK_MAX_RECEIVE_BUFFERS`
 *
 * @description
 * The number of receive buffers of a virtio network device.
 */
#define POK_MAX_RECEIVE_BUFFERS 100

/**
 * @macroname `MAX_PACKET_SIZE`
 *
 * @description
 * The maximum size of a frame, in bytes.
 */
#define MAX_PACKET_SIZE 1500U

/**
 * @typename `receive_buffer_t`
 *
 * @description
 * A buffer for a received frame, with the virtio header the device writes
 * before it.
 */
typedef struct {
    virtio_net_hdr_t virtio_net_hdr; /**< The virtio header. */
    char packet[MAX_PACKET_SIZE]; /**< The frame. */
} __attribute__((packed)) receive_buffer_t;

/**
 * @typename `send_buffer_t`
 *
 * @description
 * A buffer for a frame being sent.
 */
typedef struct {
    char data[MAX_PACKET_SIZE]; /**< The frame. */
} __attribute__((packed)) send_buffer_t;

/**
 * @typename `virtio_packet_received_fn_t`
 *
 * @description
 * Gets a frame received by the device: the frame and its size.
 */
typedef void (*virtio_packet_received_fn_t)(const char *frame,
    size_t size);

/**
 * @typename `virtio_network_device_t`
 *
 * @description
 * A virtio network device: its PCI device, its queues and its buffers.
 */
typedef struct {
    pci_dev_t pci_device; /**< The PCI device. */

    virtio_virtqueue_t rx_vq; /**< The receive queue. */
    virtio_virtqueue_t tx_vq; /**< The transmit queue. */

    uint8_t mac[6]; /**< The MAC address of the device. */

    /**
     * Gets the frames received.
     */
    virtio_packet_received_fn_t packet_received_callback;

    /**
     * The receive buffers.
     */
    receive_buffer_t receive_buffers[POK_MAX_RECEIVE_BUFFERS];
    /**
     * The send buffers (one for each descriptor of the transmit queue).
     */
    send_buffer_t *send_buffers;
    int32_t inited; /**< Whether the device is initialized. */
} virtio_network_device_t;

/**
 * @macroname `VIRTIO_PCI_VENDORID`
 *
 * @description
 * The PCI vendor ID of the virtio devices.
 */
#define VIRTIO_PCI_VENDORID 0x1AF4

/**
 * @macroname `VIRTIO_NETWORK_RX_VIRTQUEUE`
 *
 * @description
 * The index of the receive queue of a virtio network device.
 */
#define VIRTIO_NETWORK_RX_VIRTQUEUE 0

/**
 * @macroname `VIRTIO_NETWORK_TX_VIRTQUEUE`
 *
 * @description
 * The index of the transmit queue of a virtio network device.
 */
#define VIRTIO_NETWORK_TX_VIRTQUEUE 1

/**
 * @macroname `PRINTF`
 *
 * @description
 * Prints a message of the virtio network component: printf() with the prefix
 * "VIRTIO_NET_DEV: ", its result discarded.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `fmt` | The format (a string literal). |
 * | `...` | Its arguments. |
 */
#define PRINTF(fmt, ...) (void) printf("VIRTIO_NET_DEV: " fmt, ##__VA_ARGS__)

#endif
