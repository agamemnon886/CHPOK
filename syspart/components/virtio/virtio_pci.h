/*
 * Virtio PCI driver
 *
 * This module allows virtio devices to be used over a virtual PCI device.
 * This can be used with QEMU based VMMs like KVM or Xen.
 *
 * Copyright IBM Corp. 2007
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Authors:
 *  Anthony Liguori  <aliguori@us.ibm.com>
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header guard
 * isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros and types are
 * documented.
 */

#ifndef POK_VIRTIO_PCI_H
#define POK_VIRTIO_PCI_H

#include "virtio_config.h"

/**
 * @macroname `VIRTIO_PCI_HOST_FEATURES`
 *
 * @description
 * A 32-bit r/o bitmask of the features supported by the host.
 */
#define VIRTIO_PCI_HOST_FEATURES	0

/**
 * @macroname `VIRTIO_PCI_GUEST_FEATURES`
 *
 * @description
 * A 32-bit r/w bitmask of features activated by the guest.
 */
#define VIRTIO_PCI_GUEST_FEATURES	4

/**
 * @macroname `VIRTIO_PCI_QUEUE_PFN`
 *
 * @description
 * A 32-bit r/w PFN for the currently selected queue.
 */
#define VIRTIO_PCI_QUEUE_PFN		8

/**
 * @macroname `VIRTIO_PCI_QUEUE_NUM`
 *
 * @description
 * A 16-bit r/o queue size for the currently selected queue.
 */
#define VIRTIO_PCI_QUEUE_NUM		12

/**
 * @macroname `VIRTIO_PCI_QUEUE_SEL`
 *
 * @description
 * A 16-bit r/w queue selector.
 */
#define VIRTIO_PCI_QUEUE_SEL		14

/**
 * @macroname `VIRTIO_PCI_QUEUE_NOTIFY`
 *
 * @description
 * A 16-bit r/w queue notifier.
 */
#define VIRTIO_PCI_QUEUE_NOTIFY		16

/**
 * @macroname `VIRTIO_PCI_STATUS`
 *
 * @description
 * An 8-bit device status register.
 */
#define VIRTIO_PCI_STATUS		18

/**
 * @macroname `VIRTIO_PCI_ISR`
 *
 * @description
 * An 8-bit r/o interrupt status register. Reading the value will return the
 * current contents of the ISR and will also clear it. This is effectively a
 * read-and-acknowledge.
 */
#define VIRTIO_PCI_ISR			19

/**
 * @macroname `VIRTIO_PCI_ISR_CONFIG`
 *
 * @description
 * The bit of the ISR which indicates a device configuration change.
 */
#define VIRTIO_PCI_ISR_CONFIG		0x2

/* MSI-X registers: only enabled if MSI-X is enabled. */

/**
 * @macroname `VIRTIO_MSI_CONFIG_VECTOR`
 *
 * @description
 * A 16-bit vector for configuration changes.
 */
#define VIRTIO_MSI_CONFIG_VECTOR        20

/**
 * @macroname `VIRTIO_MSI_QUEUE_VECTOR`
 *
 * @description
 * A 16-bit vector for selected queue notifications.
 */
#define VIRTIO_MSI_QUEUE_VECTOR         22

/**
 * @macroname `VIRTIO_MSI_NO_VECTOR`
 *
 * @description
 * Vector value used to disable MSI for queue.
 */
#define VIRTIO_MSI_NO_VECTOR            0xffff

/**
 * @macroname `VIRTIO_PCI_CONFIG_OFF`
 *
 * @description
 * The offset of the configuration space of the device (the rest of the space,
 * defined by each driver): 24 with MSI-X, 20 without.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `msix_enabled` | Whether MSI-X is enabled. |
 */
#define VIRTIO_PCI_CONFIG_OFF(msix_enabled)	((msix_enabled) ? 24 : 20)

/**
 * @macroname `VIRTIO_PCI_CONFIG`
 *
 * @description
 * Deprecated: please use VIRTIO_PCI_CONFIG_OFF instead.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `dev` | The device (with msix_enabled). |
 */
#define VIRTIO_PCI_CONFIG(dev)	VIRTIO_PCI_CONFIG_OFF((dev)->msix_enabled)

/**
 * @macroname `VIRTIO_PCI_ABI_VERSION`
 *
 * @description
 * Virtio ABI version, this must match exactly.
 */
#define VIRTIO_PCI_ABI_VERSION		0

/**
 * @macroname `VIRTIO_PCI_QUEUE_ADDR_SHIFT`
 *
 * @description
 * How many bits to shift physical queue address written to QUEUE_PFN. 12 is
 * historical, and due to x86 page size.
 */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT	12

/**
 * @macroname `VIRTIO_PCI_VRING_ALIGN`
 *
 * @description
 * The alignment to use between consumer and producer parts of vring. x86
 * pagesize again.
 */
#define VIRTIO_PCI_VRING_ALIGN		4096
#endif
