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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; discarded results are cast to void;
 * essentially Boolean conditions; U suffixes for unsigned constants; a
 * dead store to desc->flags is removed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: lock_preemption() and
 * unlock_preemption() lose their unused parameter.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: send_frame() returns ret_t
 * codes (it returned FALSE, which is EOK, on errors); it needs 2 free TX
 * descriptors, not 1; it rejects frames longer than the send buffer and
 * checks the buffer's address before taking descriptors. Doxygen headers for
 * the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ret_e (was ret_t).
 */

/**
 * @file
 * The VIRTIO_NET_DEV component: the driver of a legacy virtio network
 * device on PCI (one receive and one transmit virtqueue), which sends the
 * frames of its in port and passes the received frames to its out port.
 */

#include <string.h>
#include <stdio.h>
#include <pci.h>
#include <ioports.h>

#include <mem.h>
#include <smalloc.h>

#include "virtio_config.h"
#include "virtio_ids.h"
#include "virtio_pci.h"
#include "virtio_ring.h"
#include "virtio_virtqueue.h"
#include "virtio_net.h"
#include "virtio_network_device.h"

#include "VIRTIO_NET_DEV_gen.h"

#include <arinc653/process.h>

static void reclaim_send_buffers(virtio_network_device_t *info);

/**
 * @funcname `lock_preemption`
 *
 * @description
 * Locks preemption of the partition (LOCK_PREEMPTION); an error is printed.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `LOCK_LEVEL` | `LOCK_LEVEL_TYPE` | The lock level (not used) |
 * | `ret_code` | `RETURN_CODE_TYPE` | The result of the ARINC 653 service |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `LOCK_PREEMPTION()` with the arguments the address of `LOCK_LEVEL`
 *    and the address of `ret_code`.
 * 2. If `ret_code` is not equal to `NO_ERROR`:
 *    1. Evaluate `PRINTF("error in LOCK_PREEMPTION %%d\n", ret_code)`.
 *
 * @calls
 * - `LOCK_PREEMPTION()`
 * - `printf()`
 */
static void lock_preemption(void)
{
    LOCK_LEVEL_TYPE LOCK_LEVEL;
    RETURN_CODE_TYPE ret_code;
    LOCK_PREEMPTION(&LOCK_LEVEL, &ret_code);
    if (ret_code != NO_ERROR) {
        PRINTF("error in LOCK_PREEMPTION %d\n", ret_code);
    }
}


/**
 * @funcname `unlock_preemption`
 *
 * @description
 * Unlocks preemption of the partition (UNLOCK_PREEMPTION); an error is printed.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `LOCK_LEVEL` | `LOCK_LEVEL_TYPE` | The lock level (not used) |
 * | `ret_code` | `RETURN_CODE_TYPE` | The result of the ARINC 653 service |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `UNLOCK_PREEMPTION()` with the arguments the address of `LOCK_LEVEL`
 *    and the address of `ret_code`.
 * 2. If `ret_code` is not equal to `NO_ERROR`:
 *    1. Evaluate `PRINTF("error in UNLOCK_PREEMPTION %%d\n", ret_code)`.
 *
 * @calls
 * - `UNLOCK_PREEMPTION()`
 * - `printf()`
 */
static void unlock_preemption(void)
{
    LOCK_LEVEL_TYPE LOCK_LEVEL;
    RETURN_CODE_TYPE ret_code;
    UNLOCK_PREEMPTION(&LOCK_LEVEL, &ret_code);
    if (ret_code != NO_ERROR) {
        PRINTF("error in UNLOCK_PREEMPTION %d\n", ret_code);
    }

}

/**
 * @funcname `setup_virtqueue`
 *
 * @description
 * Sets up a virtqueue of the device: selects it, reads its size, allocates its
 * rings and gives the device their physical page number.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `virtio_network_device_t *` | [in] The device. |
 * | `index` | `int16_t` | [in] The virtqueue's number. |
 * | `vq` | `virtio_virtqueue_t *` | [out] The virtqueue. |
 *
 * @return TRUE on success, FALSE for a wrong size or address.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mem` | `void *` | Its ring memory |
 * | `phys_addr` | `uintptr_t` | The physical address of the memory |
 * | `queue_size` | `uint16_t` | The size of the virtqueue |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code
 *    `outw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr +`
 *    `VIRTIO_PCI_QUEUE_SEL, index)`.
 * 2. Set `queue_size` to
 *    `inw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr +`
 *    `VIRTIO_PCI_QUEUE_NUM)`.
 * 3. If `queue_size` is less than `1U`:
 *    1. Evaluate `PRINTF("wrong queue size\n")`.
 *    2. Return `FALSE`.
 * 4. Call `virtio_virtqueue_setup()` with the arguments `vq`, `queue_size` and
 *    `VIRTIO_PCI_VRING_ALIGN`, and store the result in `mem`.
 * 5. Call `pok_virt_to_phys()` with the argument `mem`, and store the result in
 *    `phys_addr`.
 * 6. If `phys_addr` is equal to `0U`:
 *    1. Call `printf()` with the arguments
 *       `"%%s: kernel says that virtual address is wrong\n"` and `__func__`.
 *    2. Return `FALSE`.
 * 7. Execute the assembly code
 *    `outl(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr +`
 *    `VIRTIO_PCI_QUEUE_PFN, phys_addr / VIRTIO_PCI_VRING_ALIGN)`.
 * 8. Return `TRUE`.
 *
 * @calls
 * - `printf()`
 * - `virtio_virtqueue_setup()`
 * - `pok_virt_to_phys()`
 */
static pok_bool_t setup_virtqueue(
        virtio_network_device_t *dev, 
        int16_t index, 
        virtio_virtqueue_t *vq)
{
    // queue selector
    outw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_SEL, index);

    // get queue size

    uint16_t queue_size = inw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_NUM);
    if (queue_size < 1U) {
        PRINTF("wrong queue size\n");
        return FALSE;
    }

    // allocate memory and fill in vq fields
    void *mem = virtio_virtqueue_setup(vq, queue_size, VIRTIO_PCI_VRING_ALIGN);

    // give device queue's physical address
    uintptr_t phys_addr = pok_virt_to_phys(mem);

    if (phys_addr == 0U) {
        (void) printf(
            "%s: kernel says that virtual address is wrong\n", __func__);
        return FALSE;
    }

    outl(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_PFN, phys_addr / VIRTIO_PCI_VRING_ALIGN);
    return TRUE;

}

/**
 * @funcname `set_status_bit`
 *
 * @description
 * Sets a bit of the device status register.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pcidev` | `pci_dev_t *` | [in] The PCI device. |
 * | `bit` | `uint8_t` | [in] The bit. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set the bits
 *    `inb(pcidev->resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_STATUS)` in
 *    `bit`.
 * 2. Execute the assembly code
 *    `outb(pcidev->resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_STATUS,`
 *    `bit)`.
 *
 * @calls
 * None.
 */
static void set_status_bit(pci_dev_t *pcidev, uint8_t bit)
{
    bit |= inb(pcidev->resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_STATUS);
    outb(pcidev->resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_STATUS, bit);
}

/**
 * @funcname `read_mac_address`
 *
 * @description
 * Reads the MAC address of the device from its configuration space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `virtio_network_device_t *` | [in] The device. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The index of a byte |
 * | `mac` | `uint8_t *` | The MAC address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `mac` to `dev->mac`.
 * 2. Set `i` to `0`; then while `i` is less than `ETH_ALEN`, repeat (after each
 *    pass: increment `i`):
 *    1. Set `mac[i]` to
 *       `inb(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr +`
 *       `VIRTIO_PCI_CONFIG_OFF(FALSE) + i)`.
 *
 * @calls
 * None.
 */
static void read_mac_address(virtio_network_device_t *dev)
{
    uint8_t *mac = dev->mac;

    int32_t i;
    for (i = 0; i < ETH_ALEN; i++) {
        mac[i] = inb(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_CONFIG_OFF(FALSE) + i);
    }
}

/**
 * @funcname `use_receive_buffer`
 *
 * @description
 * Puts a receive buffer into the available ring of the receive virtqueue (the
 * device writes it).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `virtio_network_device_t *` | [in,out] The device. |
 * | `buf` | `receive_buffer_t *` | [in] The buffer. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `avail` | `int32_t` | The position in the available ring |
 * | `desc` | `vring_desc_t *` | The descriptor used |
 * | `head` | `uint16_t` | Its index |
 * | `vq` | `virtio_virtqueue_t *` | The receive virtqueue |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `vq` to the address of `dev->rx_vq`.
 * 2. If `vq->num_free` is less than `1U`:
 *    1. Evaluate `PRINTF("no free RX descriptors\n")`.
 *    2. Return.
 * 3. Decrement `vq->num_free`.
 * 4. Set `head` to `vq->free_index`.
 * 5. Set `desc` to the address of `vq->vring.desc[head]`.
 * 6. Set `vq->free_index` to `desc->next`.
 * 7. Call `pok_virt_to_phys()` with the argument `buf`, and store the result in
 *    `desc->addr`.
 * 8. If `desc->addr` is equal to `0U`:
 *    1. Call `printf()` with the arguments
 *       `"%%s: kernel says that virtual address is wrong\n"` and `__func__`.
 *    2. Return.
 * 9. Set `desc->len` to `sizeof(*buf)`.
 * 10. Set `desc->flags` to `VRING_DESC_F_WRITE`.
 * 11. Set `avail` to `vq->vring.avail->idx & (vq->vring.num-1U)`.
 * 12. Set `vq->vring.avail->ring[avail]` to `head`.
 * 13. Call `__sync_synchronize()`.
 * 14. Increment `vq->vring.avail->idx`.
 *
 * @calls
 * - `printf()`
 * - `pok_virt_to_phys()`
 * - `__sync_synchronize()`
 */
static void use_receive_buffer(virtio_network_device_t *dev,
    receive_buffer_t *buf)
{
    virtio_virtqueue_t *vq = &dev->rx_vq;

    if (vq->num_free < 1U) {
        PRINTF("no free RX descriptors\n");
        return; // FIXME return error code
    }

    vq->num_free--;

    vring_desc_t *desc;
    uint16_t head = vq->free_index;

    desc = &vq->vring.desc[head];
    vq->free_index = desc->next;

    desc->addr = pok_virt_to_phys(buf);
    if (desc->addr == 0U) {
        (void) printf(
            "%s: kernel says that virtual address is wrong\n", __func__);
        return;
    }
    desc->len = sizeof(*buf);
    desc->flags = VRING_DESC_F_WRITE;

    int32_t avail = vq->vring.avail->idx & (vq->vring.num-1U); // wrap around
    vq->vring.avail->ring[avail] = head;
    
    __sync_synchronize();
    
    vq->vring.avail->idx++;
}

/**
 * @funcname `notify_receive_buffers`
 *
 * @description
 * Notifies the device of new receive buffers; called after one or more receive
 * buffers have been added to the available ring.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `virtio_network_device_t *` | [in] The device. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code
 *    `outw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr +`
 *    `VIRTIO_PCI_QUEUE_NOTIFY, (uint16_t) VIRTIO_NETWORK_RX_VIRTQUEUE)`.
 *
 * @calls
 * None.
 */
static void notify_receive_buffers(virtio_network_device_t *dev)
{
    outw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_NOTIFY, (uint16_t) VIRTIO_NETWORK_RX_VIRTQUEUE);
}

/**
 * @funcname `setup_receive_buffers`
 *
 * @description
 * Puts all receive buffers into the available ring and notifies the device.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `virtio_network_device_t *` | [in,out] The device. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The index of a buffer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0`; then while `i` is less than `POK_MAX_RECEIVE_BUFFERS`,
 *    repeat (after each pass: increment `i`):
 *    1. Call `use_receive_buffer()` with the arguments `dev` and the address of
 *       `dev->receive_buffers[i]`.
 * 2. Call `notify_receive_buffers()` with the argument `dev`.
 *
 * @calls
 * - `use_receive_buffer()`
 * - `notify_receive_buffers()`
 */
static void setup_receive_buffers(virtio_network_device_t *dev)
{
    int32_t i;
    for (i = 0; i < POK_MAX_RECEIVE_BUFFERS; i++) {
        // this pushes buffer to avail ring
        use_receive_buffer(dev, &dev->receive_buffers[i]);
    }
    notify_receive_buffers(dev);
}

/**
 * @funcname `send_frame`
 *
 * @description
 * The send operation of the component: copies a frame into a send buffer and
 * puts it, after a virtio network header, into the transmit virtqueue (two
 * descriptors); sent buffers are reclaimed first.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `VIRTIO_NET_DEV *` | [in,out] The component. |
 * | `buffer` | `char *` | [in] The frame. |
 * | `size` | `size_t` | [in] Its size. |
 * | `max_back_step` | `size_t` | [in] The room before the frame (must be 0). |
 *
 * @return EOK, EAGAIN if there are no free descriptors, or EINVAL.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `avail` | `int32_t` | The position in the available ring |
 * | `data_addr` | `uintptr_t` | The physical address of the send buffer |
 * | `desc` | `vring_desc_t *` | A descriptor |
 * | `dev` | `virtio_network_device_t *` | The device |
 * | `head` | `uint16_t` | The first descriptor |
 * | `net_hdr` | `static virtio_net_hdr_t` | The virtio network header |
 * | `vq` | `virtio_virtqueue_t *` | The transmit virtqueue |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `self->state.info.inited` is equal to `0`:
 *    1. Return `EINVAL`.
 * 2. If `max_back_step` is not equal to `0U`:
 *    1. Return `EINVAL`.
 * 3. Set `dev` to the address of `self->state.info`.
 * 4. If `size` is greater than `MAX_PACKET_SIZE`:
 *    1. Evaluate `PRINTF("frame of %%zu bytes is too long\n", size)`.
 *    2. Return `EINVAL`.
 * 5. Call `reclaim_send_buffers()` with the argument `dev`.
 * 6. Set `vq` to the address of `dev->tx_vq`.
 * 7. If `vq->num_free` is less than `2U`:
 *    1. Evaluate `PRINTF("no free TX descriptors\n")`.
 *    2. Return `EAGAIN`.
 * 8. Set `head` to `vq->free_index`.
 * 9. Call `pok_virt_to_phys()` with the argument
 *    `dev->send_buffers[head].data`, and store the result in `data_addr`.
 * 10. If `data_addr` is equal to `0U`:
 *     1. Call `printf()` with the arguments
 *        `"%%s: kernel says that virtual address is wrong\n"` and `__func__`.
 *     2. Return `EINVAL`.
 * 11. Call `memset()` with the arguments the address of `net_hdr`, `0` and
 *     `sizeof(net_hdr)`.
 * 12. Subtract `2U` from `vq->num_free`.
 * 13. Set `desc` to the address of `vq->vring.desc[head]`.
 * 14. Call `pok_virt_to_phys()` with the address of `net_hdr`, and store the
 *     result in `desc->addr`.
 * 15. Set `desc->len` to `sizeof(net_hdr)`.
 * 16. Set `desc->flags` to `VRING_DESC_F_NEXT`.
 * 17. Call `memcpy()` with the arguments `dev->send_buffers[head].data`,
 *     `buffer` and `size`.
 * 18. Set `desc` to the address of `vq->vring.desc[desc->next]`.
 * 19. Set `desc->addr` to `data_addr`.
 * 20. Set `desc->len` to `size`.
 * 21. Set `desc->flags` to `0U`.
 * 22. Set `vq->free_index` to `desc->next`.
 * 23. Set `avail` to `vq->vring.avail->idx & (vq->vring.num-1U)`.
 * 24. Set `vq->vring.avail->ring[avail]` to `head`.
 * 25. Call `__sync_synchronize()`.
 * 26. Increment `vq->vring.avail->idx`.
 * 27. Return `EOK`.
 *
 * @calls
 * - `printf()`
 * - `reclaim_send_buffers()`
 * - `pok_virt_to_phys()`
 * - `memset()`
 * - `memcpy()`
 * - `__sync_synchronize()`
 */
ret_e send_frame(VIRTIO_NET_DEV * self,
        char *buffer,
        size_t size,
        size_t max_back_step)
{
    if (self->state.info.inited == 0) {
        return EINVAL; //FIXME
    }

    if (max_back_step != 0U) {
        return EINVAL;
    }

    static virtio_net_hdr_t net_hdr;
    vring_desc_t *desc;

    virtio_network_device_t *dev = &self->state.info;

    // The frame has to fit in the send buffer.
    if (size > MAX_PACKET_SIZE) {
        PRINTF("frame of %zu bytes is too long\n", size);
        return EINVAL;
    }

    //XXX need carefully think when do we need to call this reclaim func
    reclaim_send_buffers(dev);

    // now, send it to the virtqueue
    virtio_virtqueue_t *vq = &dev->tx_vq;
    // we use 2 desc. One for virtio specific hdr, the other one for message
    if (vq->num_free < 2U) {
        PRINTF("no free TX descriptors\n");
        return EAGAIN;
    }

    uint16_t head = vq->free_index;
    uintptr_t data_addr = pok_virt_to_phys(dev->send_buffers[head].data);
    if (data_addr == 0U) {
        (void) printf(
            "%s: kernel says that virtual address is wrong\n", __func__);
        return EINVAL; // No descriptor is taken yet.
    }

    (void) memset(&net_hdr, 0, sizeof(net_hdr));

    vq->num_free -= 2U;

    /* Setup first descriptor as virtio_net_hdr */
    desc = &vq->vring.desc[head];
    desc->addr = pok_virt_to_phys(&net_hdr);
    desc->len = sizeof(net_hdr);
    desc->flags = VRING_DESC_F_NEXT;


    (void) memcpy(dev->send_buffers[head].data, buffer, size);

    desc = &vq->vring.desc[desc->next];
    desc->addr = data_addr;
    desc->len = size;
    desc->flags = 0U;
    vq->free_index = desc->next;

    int32_t avail = vq->vring.avail->idx & (vq->vring.num-1U); // wrap around
    vq->vring.avail->ring[avail] = head;

    __sync_synchronize();

    vq->vring.avail->idx++;

    return EOK;
}

/**
 * @funcname `reclaim_send_buffers`
 *
 * @description
 * Returns the descriptors of the frames that the device has sent to the free
 * list, with preemption locked (it may be called by any thread).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `info` | `virtio_network_device_t *` | [in,out] The device. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `e` | `vring_used_elem_t *` | The used element |
 * | `head` | `vring_desc_t *` | Its first descriptor |
 * | `index` | `uint16_t` | The position in the used ring |
 * | `tail` | `vring_desc_t *` | Its last descriptor |
 * | `total_descriptors` | `uint16_t` | The number of its descriptors |
 * | `vq` | `virtio_virtqueue_t *` | The transmit virtqueue |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `vq` to the address of `(info->tx_vq)`.
 * 2. Call `lock_preemption()`.
 * 3. While `vq->last_seen_used` is not equal to `vq->vring.used->idx`:
 *    1. Set `index` to `vq->last_seen_used & (vq->vring.num-1U)`.
 *    2. Set `e` to the address of `vq->vring.used->ring[index]`.
 *    3. Set `head` to the address of `vq->vring.desc[e->id]`.
 *    4. Set `tail` to `head`.
 *    5. Set `total_descriptors` to `1`.
 *    6. While `(tail->flags & VRING_DESC_F_NEXT)` is not equal to `0U`:
 *       1. Increment `total_descriptors`.
 *       2. Set `tail` to the address of `vq->vring.desc[tail->next]`.
 *    7. Add `total_descriptors` to `vq->num_free`.
 *    8. Set `tail->next` to `vq->free_index`.
 *    9. Set `vq->free_index` to `e->id`.
 *    10. Increment `vq->last_seen_used`.
 * 4. Call `unlock_preemption()`.
 *
 * @calls
 * - `lock_preemption()`
 * - `unlock_preemption()`
 */
static void reclaim_send_buffers(virtio_network_device_t *info)
{
    virtio_virtqueue_t *vq = &(info->tx_vq);
    
    // this function can be called by any thread
    // callbacks don't do much work, so we can run them all
    // in single critical section without worrying too much
    
    lock_preemption();
    while (vq->last_seen_used != vq->vring.used->idx) {
        uint16_t index = vq->last_seen_used & (vq->vring.num-1U);
        vring_used_elem_t *e = &vq->vring.used->ring[index];
        vring_desc_t *head = &vq->vring.desc[e->id];
        vring_desc_t *tail = head;

        // reclaim descriptor
        uint16_t total_descriptors = 1;
        while ((tail->flags & VRING_DESC_F_NEXT) != 0U) {
            total_descriptors++;
            tail = &vq->vring.desc[tail->next];
        }

        vq->num_free += total_descriptors;
        
        // insert chain in the beginning of the free desc. list
        tail->next = vq->free_index; 
        vq->free_index = e->id; // id of head

        vq->last_seen_used++;
    }
    
    unlock_preemption();
}

/**
 * @funcname `reclaim_receive_buffers`
 *
 * @description
 * Passes the frames that the device has received to the out port, puts their
 * buffers back into the available ring and notifies the device; with preemption
 * locked, unlocked between the frames.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `VIRTIO_NET_DEV *` | [in,out] The component. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buf` | `receive_buffer_t *` | The receive buffer |
 * | `desc` | `vring_desc_t *` | Its descriptor |
 * | `dev` | `virtio_network_device_t *` | The device |
 * | `e` | `vring_used_elem_t *` | The used element |
 * | `index` | `uint16_t` | The position in the used ring |
 * | `old_last_seen_used` | `uint16_t` | The used index before |
 * | `vq` | `virtio_virtqueue_t *` | The receive virtqueue |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `dev` to the address of `self->state.info`.
 * 2. Set `vq` to the address of `dev->rx_vq`.
 * 3. Call `lock_preemption()`.
 * 4. Set `old_last_seen_used` to `vq->last_seen_used`.
 * 5. While `vq->last_seen_used` is not equal to `vq->vring.used->idx`:
 *    1. Set `index` to `vq->last_seen_used & (vq->vring.num-1U)`.
 *    2. Set `e` to the address of `vq->vring.used->ring[index]`.
 *    3. Set `desc` to the address of `vq->vring.desc[e->id]`.
 *    4. Call `pok_phys_to_virt()` with the argument `desc->addr`, and store the
 *       result in `buf`.
 *    5. If `buf` is NULL:
 *       1. Call `printf()` with the arguments
 *          `"%%s: kernel says that physical address is wrong\n"` and
 *          `__func__`.
 *       2. Call `unlock_preemption()`.
 *       3. Return.
 *    6. Call `VIRTIO_NET_DEV_call_portB_handle()` with the arguments `self`,
 *       `(const char *)&buf->packet` and
 *       `e->len - sizeof(virtio_net_hdr_t)`.
 *    7. Increment `vq->num_free`.
 *    8. Set `desc->next` to `vq->free_index`.
 *    9. Set `vq->free_index` to `e->id`.
 *    10. Increment `vq->last_seen_used`.
 *    11. Call `use_receive_buffer()` with the arguments `dev` and `buf`.
 *    12. Call `unlock_preemption()`.
 *    13. Call `lock_preemption()`.
 * 6. If `old_last_seen_used` is not equal to `vq->last_seen_used`:
 *    1. Call `notify_receive_buffers()` with the argument `dev`.
 * 7. Call `unlock_preemption()`.
 *
 * @calls
 * - `lock_preemption()`
 * - `pok_phys_to_virt()`
 * - `printf()`
 * - `unlock_preemption()`
 * - `VIRTIO_NET_DEV_call_portB_handle()`
 * - `use_receive_buffer()`
 * - `notify_receive_buffers()`
 */
static void reclaim_receive_buffers(VIRTIO_NET_DEV *self)
{
    virtio_network_device_t *dev = &self->state.info;
    virtio_virtqueue_t *vq = &dev->rx_vq;

    lock_preemption();

    uint16_t old_last_seen_used = vq->last_seen_used;

    while (vq->last_seen_used != vq->vring.used->idx) {
        uint16_t index = vq->last_seen_used & (vq->vring.num-1U);
        vring_used_elem_t *e = &vq->vring.used->ring[index];
        vring_desc_t *desc = &vq->vring.desc[e->id];

        receive_buffer_t *buf = pok_phys_to_virt(desc->addr);
        if (buf == NULL) {
            (void) printf(
                "%s: kernel says that physical address is wrong\n", __func__);
            unlock_preemption();
            return;
        }

        (void) VIRTIO_NET_DEV_call_portB_handle(
            self, (const char *)&buf->packet,
                e->len - sizeof(virtio_net_hdr_t));

        // reclaim descriptor
        // FIXME support chained descriptors as well
        vq->num_free++;
        desc->next = vq->free_index;
        vq->free_index = e->id;

        vq->last_seen_used++;

        // reclaim buffer
        // i.e. push it back to avail. ring
        use_receive_buffer(dev, buf);

        // preemption point
        unlock_preemption();
        lock_preemption();
    }

    if (old_last_seen_used != vq->last_seen_used) {
        // this means we moved at least one buffer from
        // used to avail ring
        notify_receive_buffers(dev);
    }

    unlock_preemption();
}

/**
 * @funcname `flush_send`
 *
 * @description
 * The flush operation of the component: notifies the device of the frames in
 * the transmit virtqueue.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `VIRTIO_NET_DEV *` | [in] The component. |
 *
 * @return EOK, or EINVAL if the device isn't initialized.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `virtio_network_device_t *` | The device |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `self->state.info.inited` is equal to `0`:
 *    1. Return `EINVAL`.
 * 2. Set `dev` to the address of `self->state.info`.
 * 3. Execute the assembly code
 *    `outw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr +`
 *    `VIRTIO_PCI_QUEUE_NOTIFY, (uint16_t) VIRTIO_NETWORK_TX_VIRTQUEUE)`.
 * 4. Return `EOK`.
 *
 * @calls
 * None.
 */
ret_e flush_send(VIRTIO_NET_DEV *self)
{
    if (self->state.info.inited == 0) {
        return EINVAL; //FIXME
    }

    virtio_network_device_t *dev = &self->state.info;

    outw(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_QUEUE_NOTIFY, (uint16_t) VIRTIO_NETWORK_TX_VIRTQUEUE);
    return EOK;
}


/*
 * PCI part
 */

/**
 * @funcname `init_device`
 *
 * @description
 * Initializes the device (PCI part): finds it, resets it, sets up its
 * virtqueues and receive buffers, negotiates the MAC feature (required), and
 * allocates the send buffers.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `state` | `VIRTIO_NET_DEV_state *` | [in,out] The component's state. |
 *
 * @return TRUE on success, FALSE otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `virtio_network_device_t *` | The device |
 * | `features` | `uint32_t` | The device's features |
 * | `recognized_features` | `uint32_t` | The features used |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `dev` to the address of `state->info`.
 * 2. Call `pci_get_dev_by_bdf()` with the arguments `state->pci_bus`,
 *    `state->pci_dev`, `state->pci_fn` and the address of `dev->pci_device`.
 * 3. If `dev->pci_device.vendor_id` is equal to `0xFFFFU`:
 *    1. Evaluate `PRINTF("have not found virtio device\n")`.
 *    2. Return `FALSE`.
 * 4. Keep only the bits `~0xFU` of
 *    `dev->pci_device.resources[PCI_RESOURCE_BAR0].addr`.
 * 5. Execute the assembly code
 *    `outb(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr +`
 *    `VIRTIO_PCI_STATUS, 0x0)`.
 * 6. Call `set_status_bit()` with the arguments the address of
 *    `dev->pci_device` and `VIRTIO_CONFIG_S_ACKNOWLEDGE`.
 * 7. Call `set_status_bit()` with the arguments the address of
 *    `dev->pci_device` and `VIRTIO_CONFIG_S_DRIVER`.
 * 8. If `setup_virtqueue(dev, VIRTIO_NETWORK_RX_VIRTQUEUE, &dev->rx_vq)` is
 *    false or `setup_virtqueue(dev, VIRTIO_NETWORK_TX_VIRTQUEUE, &dev->tx_vq)`
 *    is false:
 *    1. Return `FALSE`.
 * 9. Call `setup_receive_buffers()` with the argument `dev`.
 * 10. Set `features` to
 *     `inl(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr +`
 *     `VIRTIO_PCI_HOST_FEATURES)`.
 * 11. Set `recognized_features` to `0`.
 * 12. If `(features & (1U << VIRTIO_NET_F_MAC))` is not equal to `0U`:
 *     1. Call `read_mac_address()` with the argument `dev`.
 *     2. Set the bits `(1U << VIRTIO_NET_F_MAC)` in `recognized_features`.
 * 13. Otherwise:
 *     1. Evaluate `PRINTF("MAC address is not configured\n")`.
 *     2. Call `set_status_bit()` with the arguments the address of
 *        `dev->pci_device` and `VIRTIO_CONFIG_S_FAILED`.
 *     3. Return `FALSE`.
 * 14. Execute the assembly code
 *     `outl(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr +`
 *     `VIRTIO_PCI_GUEST_FEATURES, recognized_features)`.
 * 15. Call `set_status_bit()` with the arguments the address of
 *     `dev->pci_device` and `VIRTIO_CONFIG_S_DRIVER_OK`.
 * 16. Call `smalloc()` with the argument
 *     `sizeof(*dev->send_buffers) * dev->tx_vq.vring.num`, and store the result
 *     in `dev->send_buffers`.
 * 17. Return `TRUE`.
 *
 * @calls
 * - `pci_get_dev_by_bdf()`
 * - `printf()`
 * - `set_status_bit()`
 * - `setup_virtqueue()`
 * - `setup_receive_buffers()`
 * - `read_mac_address()`
 * - `smalloc()`
 */
static pok_bool_t init_device(VIRTIO_NET_DEV_state *state)
{
    virtio_network_device_t *dev = &state->info;

    pci_get_dev_by_bdf(state->pci_bus,
            state->pci_dev,
            state->pci_fn,
            &dev->pci_device);
    if (dev->pci_device.vendor_id == 0xFFFFU) {
        PRINTF("have not found virtio device\n");
        return FALSE;
    }

    dev->pci_device.resources[PCI_RESOURCE_BAR0].addr &= ~0xFU;

    //subsystem = pci_read_reg(dev, PCI_REG_SUBSYSTEM) >> 16;
    //if (subsystem != VIRTIO_ID_NET)
    //    printf("WARNING: wrong subsystem in virtio net device");


    // 1. Reset the device
    outb(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_STATUS, 0x0);

    // 2. ACK status bit
    set_status_bit(&dev->pci_device, VIRTIO_CONFIG_S_ACKNOWLEDGE);

    // 3. DRIVER status bit
    set_status_bit(&dev->pci_device, VIRTIO_CONFIG_S_DRIVER);

    // 4. Device-specific setup
    if (!setup_virtqueue(dev, VIRTIO_NETWORK_RX_VIRTQUEUE, &dev->rx_vq)
        || !setup_virtqueue(dev, VIRTIO_NETWORK_TX_VIRTQUEUE, &dev->tx_vq)) {
        return FALSE;
    }

    setup_receive_buffers(dev);

    //pok_bsp_irq_register(virtio_network_device.pci_device.irq_line, virtio_interrupt_handler);

    // 5. Device feature bits

    uint32_t features = inl(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_HOST_FEATURES);
    uint32_t recognized_features = 0;

    if ((features & (1U << VIRTIO_NET_F_MAC)) != 0U) {
        read_mac_address(dev);
        recognized_features |= (1U << VIRTIO_NET_F_MAC);
    } else {
        PRINTF("MAC address is not configured\n");
        set_status_bit(&dev->pci_device, VIRTIO_CONFIG_S_FAILED);
        return FALSE;
    }

    outl(dev->pci_device.resources[PCI_RESOURCE_BAR0].addr + VIRTIO_PCI_GUEST_FEATURES, recognized_features);

    // 6. DRIVER_OK status bit
    set_status_bit(&dev->pci_device, VIRTIO_CONFIG_S_DRIVER_OK);

    // 7. send buffers allocation
    dev->send_buffers = smalloc(sizeof(*dev->send_buffers) * dev->tx_vq.vring.num);

    return TRUE;
}

/**
 * @funcname `virtio_receive_activity`
 *
 * @description
 * The activity of the component: handles the received frames, if the device is
 * initialized.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `VIRTIO_NET_DEV *` | [in,out] The component. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `self->state.info.inited` is not equal to `0`:
 *    1. Call `reclaim_receive_buffers()` with the argument `self`.
 *
 * @calls
 * - `reclaim_receive_buffers()`
 */
void virtio_receive_activity(VIRTIO_NET_DEV *self)
{
    if (self->state.info.inited != 0) {
        reclaim_receive_buffers(self);
    }
}

/*
 * init
 */
/**
 * @funcname `virtio_init`
 *
 * @description
 * The init function of the component: initializes the device.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `VIRTIO_NET_DEV *` | [in,out] The component. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `init_device(&self->state)` is true:
 *    1. Set `self->state.info.inited` to `1`.
 *
 * @calls
 * - `init_device()`
 */
void virtio_init(VIRTIO_NET_DEV *self)
{
    if (init_device(&self->state)) {
        self->state.info.inited = 1;
    }
}
