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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros are in rtl8029.h; the
 * struct types are used by their typedef names.
 */

/**
 * @file
 * The NE2000 (RTL8029) network driver of the old POK network interface
 * (SYS_NEEDS_DRIVER_NE2000): remote DMA to the card's memory, polling for
 * received packets, and the PCI probe. Page 0 is always assumed to be
 * selected, except during initialization and polling.
 */

#include "syspart_config.h"

#ifdef SYS_NEEDS_DRIVER_NE2000

#include "rtl8029.h"
//#include <middleware/port.h>
#include <pci.h>
#include <ioports.h>
#include <net/network.h>
#include <net/netdevices.h>

#include <net/ether.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/byteorder.h>

#include <mem.h>

/**
 * @funcname `ne2000_write`
 *
 * @description
 * Writes bytes into the card's memory by remote DMA.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `const ne2000_dev_t *` | [in] The card. |
 * | `buf` | `const void *` | [in] The bytes. |
 * | `count` | `uint16_t` | [in] Their number. |
 * | `offset` | `uint16_t` | [in] The address in the card's memory. |
 *
 * @return count.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const char *` | The byte written |
 * | `ret` | `int32_t` | The number of bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `NULL`.
 * 2. Set `ret` to `count`.
 * 3. Execute the assembly code
 *    `outb_inverse((inb(dev->addr + NE2000_CR) & ~(NE2000_CR_RD0 |`
 *    `NE2000_CR_RD1)) | NE2000_CR_RD2, dev->addr)`.
 * 4. Execute the assembly code
 *    `outb_inverse(offset, dev->addr + NE2000_RSAR0)`.
 * 5. Execute the assembly code
 *    `outb_inverse(offset >> 8, dev->addr + NE2000_RSAR1)`.
 * 6. Execute the assembly code `outb_inverse(count, dev->addr + NE2000_RBCR0)`.
 * 7. Execute the assembly code
 *    `outb_inverse(count >> 8, dev->addr + NE2000_RBCR1)`.
 * 8. Execute the assembly code
 *    `outb_inverse((inb(dev->addr + NE2000_CR) & ~(NE2000_CR_RD0 |`
 *    `NE2000_CR_RD2)) | NE2000_CR_RD1, dev->addr)`.
 * 9. Set `p` to `buf`; then while `count` is greater than `0`, repeat (after
 *    each pass: decrement `count`. Increment `p`):
 *    1. Execute the assembly code
 *       `outb_inverse(*p, dev->addr + NE2000_DMA_PORT)`.
 * 10. Return `(ret)`.
 *
 * @calls
 * None.
 */
static int32_t ne2000_write(
        const ne2000_dev_t *dev,
        const void         *buf,
        uint16_t           count,
        uint16_t           offset)
{
    const char* p = NULL;
    int32_t ret = count;

    // Sets RD2 (abort/complete remote DMA)
    outb_inverse((inb(dev->addr + NE2000_CR) & ~(NE2000_CR_RD0 | NE2000_CR_RD1)) |
            NE2000_CR_RD2, dev->addr);

    /* These two registers set the start address of remote DMA. */
    outb_inverse(offset, dev->addr + NE2000_RSAR0);
    outb_inverse(offset >> 8, dev->addr + NE2000_RSAR1);

    /* These two registers set the data byte counts of remote DMA. */
    outb_inverse(count, dev->addr + NE2000_RBCR0);
    outb_inverse(count >> 8, dev->addr + NE2000_RBCR1);

    // Sets RD1 (remote write)
    outb_inverse((inb(dev->addr + NE2000_CR) & ~(NE2000_CR_RD0 | NE2000_CR_RD2)) |
            NE2000_CR_RD1, dev->addr);

    for (p = buf; count > 0; count--, p++) {
        outb_inverse(*p, dev->addr + NE2000_DMA_PORT);
    }

    return (ret);
}

/**
 * @funcname `ne2000_read_header`
 *
 * @description
 * Reads the header of a received packet from the card's memory by remote DMA.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `const ne2000_dev_t *` | [in] The card. |
 * | `hdr` | `ne2000_header_t *` | [out] Where the header is stored. |
 * | `offset` | `uint16_t` | [in] The address in the card's memory. |
 *
 * @return The size of the header.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `int32_t` | The size of the header |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `count` to `sizeof(ne2000_header_t)`.
 * 2. Execute the assembly code
 *    `outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD0 |`
 *    `NE2000_CR_RD1)) | NE2000_CR_RD2, (dev)->addr)`.
 * 3. Execute the assembly code
 *    `outb_inverse(offset, dev->addr + NE2000_RSAR0)`.
 * 4. Execute the assembly code
 *    `outb_inverse(offset >> 8, dev->addr + NE2000_RSAR1)`.
 * 5. Execute the assembly code `outb_inverse(count, dev->addr + NE2000_RBCR0)`.
 * 6. Execute the assembly code
 *    `outb_inverse(count >> 8, dev->addr + NE2000_RBCR1)`.
 * 7. Execute the assembly code
 *    `outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD1 |`
 *    `NE2000_CR_RD2)) | NE2000_CR_RD0, (dev)->addr)`.
 * 8. Set `hdr->status` to `inb(dev->addr + NE2000_DMA_PORT)`.
 * 9. Set `hdr->next` to `inb(dev->addr + NE2000_DMA_PORT)`.
 * 10. Set `hdr->size` to `inb(dev->addr + NE2000_DMA_PORT)`.
 * 11. Set the bits `inb(dev->addr + NE2000_DMA_PORT) << 8` in `hdr->size`.
 * 12. Return `count`.
 *
 * @calls
 * None.
 */
static int32_t ne2000_read_header(
        const ne2000_dev_t *dev,
        ne2000_header_t    *hdr,
        uint16_t            offset)
{
    int32_t count = sizeof(ne2000_header_t);
    // Sets RD2 (abort/complete remote DMA)
    outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD0 | NE2000_CR_RD1)) |
            NE2000_CR_RD2, (dev)->addr);

    /* These two registers set the start address of remote DMA. */
    outb_inverse(offset, dev->addr + NE2000_RSAR0);
    outb_inverse(offset >> 8, dev->addr + NE2000_RSAR1);

    /* These two registers set the data byte counts of remote DMA. */
    outb_inverse(count, dev->addr + NE2000_RBCR0);
    outb_inverse(count >> 8, dev->addr + NE2000_RBCR1);

    // Sets RD0 (remote read)
    outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD1 | NE2000_CR_RD2)) |
            NE2000_CR_RD0, (dev)->addr);
    hdr->status = inb(dev->addr + NE2000_DMA_PORT);
    hdr->next   = inb(dev->addr + NE2000_DMA_PORT);
    hdr->size   = inb(dev->addr + NE2000_DMA_PORT);
    hdr->size  |= inb(dev->addr + NE2000_DMA_PORT) << 8;

    return count;
}
/**
 * @funcname `ne2000_read`
 *
 * @description
 * Reads bytes from the card's memory by remote DMA.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev` | `const ne2000_dev_t *` | [in] The card. |
 * | `buf` | `void *` | [out] Where the bytes are stored. |
 * | `count` | `uint16_t` | [in] Their number. |
 * | `offset` | `uint16_t` | [in] The address in the card's memory. |
 *
 * @return count.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `char *` | The byte read |
 * | `ret` | `int32_t` | The number of bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `NULL`.
 * 2. Set `ret` to `count`.
 * 3. Execute the assembly code
 *    `outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD0 |`
 *    `NE2000_CR_RD1)) | NE2000_CR_RD2, (dev)->addr)`.
 * 4. Execute the assembly code
 *    `outb_inverse(offset, dev->addr + NE2000_RSAR0)`.
 * 5. Execute the assembly code
 *    `outb_inverse(offset >> 8, dev->addr + NE2000_RSAR1)`.
 * 6. Execute the assembly code `outb_inverse(count, dev->addr + NE2000_RBCR0)`.
 * 7. Execute the assembly code
 *    `outb_inverse(count >> 8, dev->addr + NE2000_RBCR1)`.
 * 8. Execute the assembly code
 *    `outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD1 |`
 *    `NE2000_CR_RD2)) | NE2000_CR_RD0, (dev)->addr)`.
 * 9. Set `p` to `buf`; then while `count` is greater than `0`, repeat (after
 *    each pass: decrement `count`. Increment `p`):
 *    1. Set `*p` to `inb(dev->addr + NE2000_DMA_PORT)`.
 * 10. Return `ret`.
 *
 * @calls
 * None.
 */
static int32_t ne2000_read(
        const ne2000_dev_t *dev,
        void               *buf,
        uint16_t            count,
        uint16_t            offset)
{
    char* p = NULL;
    int32_t ret = count;

    // Sets RD2 (abort/complete remote DMA)
    outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD0 | NE2000_CR_RD1)) |
            NE2000_CR_RD2, (dev)->addr);

    /* These two registers set the start address of remote DMA. */
    outb_inverse(offset, dev->addr + NE2000_RSAR0);
    outb_inverse(offset >> 8, dev->addr + NE2000_RSAR1);

    /* These two registers set the data byte counts of remote DMA. */
    outb_inverse(count, dev->addr + NE2000_RBCR0);
    outb_inverse(count >> 8, dev->addr + NE2000_RBCR1);

    // Sets RD0 (remote read)
    outb_inverse((inb((dev)->addr + NE2000_CR) & ~(NE2000_CR_RD1 | NE2000_CR_RD2)) |
            NE2000_CR_RD0, (dev)->addr);

    for (p = buf; count > 0; count--, p++) {
        *p = inb(dev->addr + NE2000_DMA_PORT);
    }

    return ret;
}


/**
 * @funcname `rtl8029_polling`
 *
 * @description
 * Polls the card: watches for events, typically for receiving queued packets,
 * which go to the callback; then clears the interrupt flags.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `netdev` | `pok_netdevice_t *` | [in] The network device. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `drv_info` | `ne2000_dev_t *` | The card |
 * | `end` | `uint8_t` | The current page of the ring |
 * | `ip_hdr` | `const ip_hdr_t *` | Its IP header (for packets padded to the minimum size) |
 * | `ne2000_hdr` | `ne2000_header_t` | The header of the received packet |
 * | `offset` | `uint16_t` | The DMA offset |
 * | `packet_len` | `uint32_t` | The length of the packet |
 * | `recv_packet` | `pok_packet_t` | The received packet |
 * | `start` | `uint8_t` | The first page of the packet in the ring |
 * | `state` | `uint8_t` | The interrupt status |
 * | `tmp` | `uint32_t` | The boundary page |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `NE2000_SELECT_PAGE(drv_info, 0)`.
 * 2. If `(state = inb(drv_info->addr + NE2000_ISR))` is equal to `0`:
 *    1. Go to the label `POLL_END`.
 * 3. If `!state & NE2000_ISR_PRX` is true:
 *    1. Go to the label `POLL_END`.
 * 4. If `(inb(drv_info->addr + NE2000_RSR) & NE2000_RSR_PRX)` is equal to `0`:
 *    1. Go to the label `POLL_END`.
 * 5. Set `tmp` to `inb(drv_info->addr + NE2000_BNRY)`.
 * 6. Set `start` to `tmp + 1`.
 * 7. Execute the assembly code `NE2000_SELECT_PAGE(drv_info, 1)`.
 * 8. Set `end` to `inb(drv_info->addr + NE2000_CURR)`.
 * 9. Execute the assembly code `NE2000_SELECT_PAGE(drv_info, 0)`.
 * 10. If `(end % NE2000_MEMSZ)` is equal to `(start % NE2000_MEMSZ)`:
 *     1. Go to the label `POLL_END`.
 * 11. Set `offset` to `start << 8`.
 * 12. Add `ne2000_read_header(drv_info, &ne2000_hdr, offset)` to `offset`.
 * 13. Call `ne2000_read()` with the arguments `drv_info`, the address of
 *     `recv_packet`, `ne2000_hdr.size - sizeof(ne2000_header_t)` and `offset`.
 * 14. Set `packet_len` to `ne2000_hdr.size - sizeof(ne2000_header_t)`.
 * 15. If `ne2000_hdr.size` is equal to `NE2000_ETH_DATA_MINLEN`:
 *     1. Set `ip_hdr` to
 *        `(const ip_hdr_t *) (sizeof(ether_hdr_t) + (const char`
 *        `*)&recv_packet)`.
 *     2. Set `packet_len` to
 *        `ntoh16(ip_hdr->length) + sizeof(ether_hdr_t)`.
 * 16. Execute the assembly code
 *     `outb_inverse(ne2000_hdr.next > NE2000_MEMSZ ? NE2000_RXBUF - 1 :`
 *     `ne2000_hdr.next - 1, drv_info->addr + NE2000_BNRY)`.
 * 17. Call the function `drv_info->packet_received_callback` with the arguments
 *     `(const char *)&recv_packet` and `packet_len`.
 * 18. Execute the assembly code
 *     `outb_inverse(NE2000_ISR_PRX, drv_info->addr + NE2000_ISR)`.
 * 19. Label `POLL_END`:
 * 20. If `state & NE2000_ISR_PTX` is true:
 *     1. Execute the assembly code
 *        `outb_inverse(NE2000_ISR_PTX, drv_info->addr + NE2000_ISR)`.
 * 21. If `state & NE2000_ISR_RXE` is true:
 *     1. Execute the assembly code
 *        `outb_inverse(NE2000_ISR_RXE, drv_info->addr + NE2000_ISR)`.
 * 22. If `state & NE2000_ISR_TXE` is true:
 *     1. Execute the assembly code
 *        `outb_inverse(NE2000_ISR_TXE, drv_info->addr + NE2000_ISR)`.
 * 23. If `state & NE2000_ISR_OVW` is true:
 *     1. Execute the assembly code
 *        `outb_inverse(NE2000_ISR_OVW, drv_info->addr + NE2000_ISR)`.
 * 24. If `state & NE2000_ISR_CNT` is true:
 *     1. Execute the assembly code
 *        `outb_inverse(NE2000_ISR_CNT, drv_info->addr + NE2000_ISR)`.
 * 25. If `state & NE2000_ISR_RST` is true:
 *     1. Execute the assembly code
 *        `outb_inverse(NE2000_ISR_RST, drv_info->addr + NE2000_ISR)`.
 *
 * @calls
 * - `ne2000_read_header()`
 * - `ne2000_read()`
 * - `ntoh16()`
 * - `drv_info->packet_received_callback()` (indirect)
 */
void rtl8029_polling(pok_netdevice_t * netdev)
{
    uint8_t state; // ISR state
    ne2000_header_t  ne2000_hdr;     // ne2000 packet header
    uint16_t         offset;         // dma offset
    uint8_t          start, end;     // pointers for the ring buffer
    pok_packet_t     recv_packet;

    ne2000_dev_t *drv_info = netdev->info;

    NE2000_SELECT_PAGE(drv_info, 0);

    // do we have an interrupt flag set?
    if ((state = inb(drv_info->addr + NE2000_ISR)) == 0)
        goto POLL_END;

    if (!state & NE2000_ISR_PRX)
        goto POLL_END;

    if ((inb(drv_info->addr + NE2000_RSR) & NE2000_RSR_PRX) == 0)
        goto POLL_END;

    /* This register is used to prevent overwrite of the receive buffer ring.
       It is typically used as a pointer indicating the last receive buffer
       page the host has read.*/
    uint32_t tmp = inb(drv_info->addr + NE2000_BNRY);
    start = tmp + 1;

    /* This register points to the page address of the first receive
       buffer page to be used for a packet reception. */
    NE2000_SELECT_PAGE(drv_info, 1);
    end = inb(drv_info->addr + NE2000_CURR);
    NE2000_SELECT_PAGE(drv_info, 0);

    if ((end % NE2000_MEMSZ) == (start % NE2000_MEMSZ))
        goto POLL_END;

    /* et on decapsule! */

    offset = start << 8;
    offset += ne2000_read_header(drv_info, &ne2000_hdr, offset);

    ne2000_read(drv_info, &recv_packet,
            ne2000_hdr.size - sizeof(ne2000_header_t), offset);
    uint32_t packet_len = ne2000_hdr.size - sizeof(ne2000_header_t);

    if (ne2000_hdr.size == NE2000_ETH_DATA_MINLEN) {
        // NIC add trailing zeros to packets shorter than 64 bytes. They should be ignored
        const ip_hdr_t *ip_hdr = (const ip_hdr_t *)
            (sizeof(ether_hdr_t) + (const char *)&recv_packet);

        packet_len = ntoh16(ip_hdr->length) + sizeof(ether_hdr_t);
    }

    // update the BNRY register... almost forgot that
    outb_inverse(ne2000_hdr.next > NE2000_MEMSZ ?
            NE2000_RXBUF - 1 : ne2000_hdr.next - 1, drv_info->addr + NE2000_BNRY);

    //XXX callback should create copy of recv_packet
    drv_info->packet_received_callback((const char *)&recv_packet, packet_len);

    outb_inverse(NE2000_ISR_PRX, drv_info->addr + NE2000_ISR); // Clear PRX flag

POLL_END:
    //Clear flags
    if (state & NE2000_ISR_PTX) outb_inverse(NE2000_ISR_PTX, drv_info->addr + NE2000_ISR);
    if (state & NE2000_ISR_RXE) outb_inverse(NE2000_ISR_RXE, drv_info->addr + NE2000_ISR);
    if (state & NE2000_ISR_TXE) outb_inverse(NE2000_ISR_TXE, drv_info->addr + NE2000_ISR);
    if (state & NE2000_ISR_OVW) outb_inverse(NE2000_ISR_OVW, drv_info->addr + NE2000_ISR);
    if (state & NE2000_ISR_CNT) outb_inverse(NE2000_ISR_CNT, drv_info->addr + NE2000_ISR);
    if (state & NE2000_ISR_RST) outb_inverse(NE2000_ISR_RST, drv_info->addr + NE2000_ISR);
}


/**
 * @funcname `rtl8029_init`
 *
 * @description
 * Initializes the card: its registers (receive ring, no interrupts: POK uses
 * polling) and its MAC address, read from its PROM (each byte is there twice).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pci_dev` | `pci_dev_t *` | [in] The PCI device. |
 * | `drv_info` | `ne2000_dev_t *` | [in,out] The card. |
 *
 * @return TRUE.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buf` | `uint8_t[12]` | The PROM bytes |
 * | `i` | `uint8_t` | The index of a byte of the MAC address |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `drv_info->addr` to `pci_dev`.
 * 2. Set `i` to `0`.
 * 3. Execute the assembly code `NE2000_SELECT_PAGE(drv_info, 0)`.
 * 4. Execute the assembly code
 *    `outb_inverse(NE2000_CR_STP, drv_info->addr + NE2000_CR)`.
 * 5. Execute the assembly code
 *    `outb_inverse(0x00, drv_info->addr + NE2000_TCR)`.
 * 6. Execute the assembly code
 *    `outb_inverse(NE2000_RCR_AB|NE2000_RCR_AR, drv_info->addr + NE2000_RCR)`.
 * 7. Execute the assembly code
 *    `outb_inverse(NE2000_DCR_LS | NE2000_DCR_FT1, drv_info->addr +`
 *    `NE2000_DCR)`.
 * 8. Execute the assembly code
 *    `outb_inverse(NE2000_RXBUF, drv_info->addr + NE2000_PSTART)`.
 * 9. Execute the assembly code
 *    `outb_inverse(NE2000_MEMSZ, drv_info->addr + NE2000_PSTOP)`.
 * 10. Execute the assembly code
 *     `outb_inverse(NE2000_RXBUF, drv_info->addr + NE2000_BNRY)`.
 * 11. Execute the assembly code
 *     `outb_inverse(0, drv_info->addr + NE2000_RBCR0)`.
 * 12. Execute the assembly code
 *     `outb_inverse(0, drv_info->addr + NE2000_RBCR1)`.
 * 13. Execute the assembly code `NE2000_SELECT_PAGE(drv_info, 1)`.
 * 14. Execute the assembly code
 *     `outb_inverse(NE2000_RXBUF + 1, drv_info->addr + NE2000_CURR)`.
 * 15. Call `ne2000_read()` with the arguments `drv_info`, `buf`, `6 * 2` and
 *     `0`.
 * 16. Set `i` to `0`; then while `i` is less than `6`, repeat (after each pass:
 *     increment `i`):
 *     1. Set `drv_info->mac[i]` to `buf[i * 2]`.
 * 17. Execute the assembly code
 *     `outb_inverse(drv_info->mac[0], drv_info->addr + NE2000_PAR0)`.
 * 18. Execute the assembly code
 *     `outb_inverse(drv_info->mac[1], drv_info->addr + NE2000_PAR1)`.
 * 19. Execute the assembly code
 *     `outb_inverse(drv_info->mac[2], drv_info->addr + NE2000_PAR2)`.
 * 20. Execute the assembly code
 *     `outb_inverse(drv_info->mac[3], drv_info->addr + NE2000_PAR3)`.
 * 21. Execute the assembly code
 *     `outb_inverse(drv_info->mac[4], drv_info->addr + NE2000_PAR4)`.
 * 22. Execute the assembly code
 *     `outb_inverse(drv_info->mac[5], drv_info->addr + NE2000_PAR5)`.
 * 23. Execute the assembly code `NE2000_SELECT_PAGE(drv_info, 0)`.
 * 24. Execute the assembly code
 *     `outb_inverse(NE2000_CR_STA, drv_info->addr + NE2000_CR)`.
 * 25. Execute the assembly code
 *     `outb_inverse(0xFF, drv_info->addr + NE2000_ISR)`.
 * 26. Execute the assembly code
 *     `outb_inverse(0x00, drv_info->addr + NE2000_IMR)`.
 * 27. Return `TRUE`.
 *
 * @calls
 * - `ne2000_read()`
 */
static int32_t rtl8029_init (pci_dev_t *pci_dev, ne2000_dev_t *drv_info)
{
    // dev.pci.vendorid = 0x10ec;
    // dev.pci.deviceid = 0x8029;
    // dev.pci.io_range = 0x10;

    drv_info->addr = pci_dev->ioaddr;

    uint8_t i = 0;
    uint8_t buf[6 * 2]; // used for MAC address

    NE2000_SELECT_PAGE(drv_info, 0);

    /* This bit is the STOP command. When it is set, no packets will be
       received or transmitted. POWER UP=1. */
    outb_inverse(NE2000_CR_STP, drv_info->addr + NE2000_CR);

    // Sets several options... Read the datasheet!
    outb_inverse(0x00, drv_info->addr + NE2000_TCR);
    outb_inverse(NE2000_RCR_AB|NE2000_RCR_AR, drv_info->addr + NE2000_RCR);
    outb_inverse(NE2000_DCR_LS | NE2000_DCR_FT1, drv_info->addr + NE2000_DCR);

    /* The Page Start register sets the start page address
       of the receive buffer ring. */
    outb_inverse(NE2000_RXBUF, drv_info->addr + NE2000_PSTART);
    /* The Page Stop register sets the stop page address
       of the receive buffer ring. */
    outb_inverse(NE2000_MEMSZ, drv_info->addr + NE2000_PSTOP);
    /* This register is used to prevent overwrite of the receive buffer ring.
       It is typically used as a pointer indicating the last receive buffer
       page the host has read. */
    outb_inverse(NE2000_RXBUF, drv_info->addr + NE2000_BNRY);

    /* These two registers set the data byte counts of remote DMA. */
    outb_inverse(0, drv_info->addr + NE2000_RBCR0);
    outb_inverse(0, drv_info->addr + NE2000_RBCR1);

    NE2000_SELECT_PAGE(drv_info, 1);

    /* This register points to the page address of the first receive buffer
       page to be used for a packet reception. */
    outb_inverse(NE2000_RXBUF + 1, drv_info->addr + NE2000_CURR);

    // Init mac address
    /* Here's something I do not understand... Section 6.2.2 of the datasheet
       says bytes 00H-05H of the PROM corresponds to the Ethernet ID. But it
       looks like each byte of the MAC address is written twice...
       Therefore I read 2 * sizeof(mac) and select one of the two bytes
       corresponding to the MAC... Weird... Really... */
    ne2000_read(drv_info, buf, 6 * 2, 0);
    for (i = 0; i < 6; i++) {
        drv_info->mac[i] = buf[i * 2];
    }

    /* These registers contain my Ethernet node address and are used to compare
       the destination address of incoming packets for acceptation or rejection.*/
    outb_inverse(drv_info->mac[0], drv_info->addr + NE2000_PAR0);
    outb_inverse(drv_info->mac[1], drv_info->addr + NE2000_PAR1);
    outb_inverse(drv_info->mac[2], drv_info->addr + NE2000_PAR2);
    outb_inverse(drv_info->mac[3], drv_info->addr + NE2000_PAR3);
    outb_inverse(drv_info->mac[4], drv_info->addr + NE2000_PAR4);
    outb_inverse(drv_info->mac[5], drv_info->addr + NE2000_PAR5);

    NE2000_SELECT_PAGE(drv_info, 0);

    // Start command
    outb_inverse(NE2000_CR_STA, drv_info->addr + NE2000_CR);

    // Reactivating interrupts
    /* ISR register must be cleared after power up. */
    outb_inverse(0xFF, drv_info->addr + NE2000_ISR);
    /* All bits correspond to the bits in the ISR register. POWER UP=all 0s.
       Setting individual bits will enable the corresponding interrupts. */
    /* Since POK use polling, ALL interrupts are disabled */
    outb_inverse(0x00, drv_info->addr + NE2000_IMR);

    return TRUE;
}

/**
 * @funcname `send_frame_gather`
 *
 * @description
 * Sends a frame (one buffer only: real gathering isn't supported) by writing it
 * into the card's memory and starting the transmission; the callback is called
 * at once.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `netdev` | `pok_netdevice_t *` | [in] The network device. |
 * | `sg_list` | `const pok_network_sg_list_t *` | [in] The buffers. |
 * | `sg_list_len` | `size_t` | [in] Their number (1). |
 * | `callback` | `pok_network_buffer_callback_t` | [in] Called when the buffer can be reused. |
 * | `callback_arg` | `void *` | [in] Its argument. |
 *
 * @return TRUE, or FALSE for more than one buffer.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buf` | `char *` | The frame |
 * | `buf_len` | `size_t` | Its length |
 * | `drv_info` | `ne2000_dev_t *` | The card |
 * | `state` | `uint8_t` | The interrupt status |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `sg_list_len` is not equal to `1`:
 *    1. Call `printf()` with the argument
 *       `"In ne2000 driver real gathering is not supported yet"`.
 *    2. Return `FALSE`.
 * 2. Call `ne2000_write()` with the arguments `drv_info`, `buf`, `buf_len` and
 *    `NE2000_TXBUF * 256`.
 * 3. Repeat the following while `(state & NE2000_ISR_RDC)` is not equal to
 *    `NE2000_ISR_RDC` (at least once):
 *    1. Set `state` to `inb(drv_info->addr + NE2000_ISR)`.
 * 4. Execute the assembly code
 *    `outb_inverse(NE2000_TXBUF, drv_info->addr + NE2000_TPSR)`.
 * 5. Execute the assembly code
 *    `outb_inverse(buf_len, drv_info->addr + NE2000_TBCR0)`.
 * 6. Execute the assembly code
 *    `outb_inverse(buf_len >> 8, drv_info->addr + NE2000_TBCR1)`.
 * 7. Execute the assembly code
 *    `outb_inverse(inb(drv_info->addr + NE2000_CR) | NE2000_CR_TXP,`
 *    `drv_info->addr + NE2000_CR)`.
 * 8. Execute the assembly code
 *    `outb_inverse(NE2000_ISR_RDC, drv_info->addr + NE2000_ISR)`.
 * 9. Return `TRUE`.
 *
 * @calls
 * - `printf()`
 * - `ne2000_write()`
 */
static pok_bool_t send_frame_gather(
        pok_netdevice_t *netdev,
        const pok_network_sg_list_t *sg_list,
        size_t sg_list_len,
        pok_network_buffer_callback_t callback,
        void *callback_arg)
{
    if (sg_list_len != 1) {
        printf("In ne2000 driver real gathering is not supported yet");
        return FALSE;
    }

    ne2000_dev_t *drv_info = netdev->info;

    char *buf = sg_list[0].buffer;
    size_t buf_len = sg_list[0].size;

    uint8_t state; // ISR state

    ne2000_write(drv_info, buf, buf_len, NE2000_TXBUF * 256);

    do {
        state = inb(drv_info->addr + NE2000_ISR);
    } while ((state & NE2000_ISR_RDC) != NE2000_ISR_RDC);

    /* This register sets the start page address of
       the packet to the transmitted. */
    outb_inverse(NE2000_TXBUF, drv_info->addr + NE2000_TPSR); //?

    /* These two registers set the byte counts of
       the packet to be transmitted. */
    outb_inverse(buf_len, drv_info->addr + NE2000_TBCR0);
    outb_inverse(buf_len >> 8, drv_info->addr + NE2000_TBCR1);

    /* This bit must be set to transmit a packet. */
    outb_inverse(inb(drv_info->addr + NE2000_CR) | NE2000_CR_TXP,
            drv_info->addr + NE2000_CR);

    outb_inverse(NE2000_ISR_RDC, drv_info->addr + NE2000_ISR); // Clear RDC bit

    //XXX: Currently ne2000 driver ignores reclaim_*
    callback(callback_arg);

    return TRUE;
}


/**
 * @funcname `set_packet_received_callback`
 *
 * @description
 * Sets the function called for each received packet.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `netdev` | `pok_netdevice_t *` | [in] The network device. |
 * | `f` | `void (*)(const char *, size_t)` | [in] The function. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `drv_info` | `ne2000_dev_t *` | The card |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `drv_info->packet_received_callback` to `f`.
 *
 * @calls
 * None.
 */
static void set_packet_received_callback(
        pok_netdevice_t *netdev,
        void (*f)(const char *, size_t))
{
    ne2000_dev_t *drv_info = netdev->info;
    drv_info->packet_received_callback = f;
}


/**
 * @funcname `send_frame`
 *
 * @description
 * Sends a frame from one buffer.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `netdev` | `pok_netdevice_t *` | [in] The network device. |
 * | `buffer` | `char *` | [in] The frame. |
 * | `size` | `size_t` | [in] Its size. |
 * | `callback` | `pok_network_buffer_callback_t` | [in] Called when the buffer can be reused. |
 * | `callback_arg` | `void *` | [in] Its argument. |
 *
 * @return The result of send_frame_gather().
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
 * 1. Do nothing.
 *
 * @calls
 * None.
 */
static pok_bool_t send_frame(
        pok_netdevice_t *netdev,
        char *buffer,
        size_t size,
        pok_network_buffer_callback_t callback,
        void *callback_arg)
{
    pok_network_sg_list_t sg_list[1] = {{.buffer=buffer, .size=size}};
    return send_frame_gather(netdev, sg_list, 1, callback, callback_arg);
}

/**
 * @funcname `reclaim_send_buffers`
 *
 * @description
 * Reclaims the sent buffers: nothing to do (the callback was called when
 * sending).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `netdev` | `pok_netdevice_t *` | [in] The network device (not used). |
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
 * 1. Do nothing.
 *
 * @calls
 * None.
 */
static void reclaim_send_buffers(pok_netdevice_t *netdev)
{
    (void) netdev;
}

/**
 * @funcname `reclaim_receive_buffers`
 *
 * @description
 * Handles the received packets: polls the card.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `netdev` | `pok_netdevice_t *` | [in] The network device. |
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
 * 1. Do nothing.
 *
 * @calls
 * None.
 */
static void reclaim_receive_buffers(pok_netdevice_t *netdev)
{
    rtl8029_polling(netdev);
}

/**
 * @funcname `flush_send`
 *
 * @description
 * Flushes the sent frames: nothing to do.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `netdev` | `pok_netdevice_t *` | [in] The network device (not used). |
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
 * 1. Do nothing.
 *
 * @calls
 * None.
 */
static void flush_send(pok_netdevice_t *netdev)
{
}

static const pok_network_driver_ops_t driver_ops = {
    .send_frame = send_frame,
    .send_frame_gather = send_frame_gather,
    .set_packet_received_callback = set_packet_received_callback,
    .reclaim_send_buffers = reclaim_send_buffers,
    .reclaim_receive_buffers = reclaim_receive_buffers,
    .flush_send = flush_send,
};

/**
 * @funcname `probe_device`
 *
 * @description
 * Probes a card found on PCI: initializes it and registers it as a network
 * device named `ne2000-net<n>`.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pci_dev` | `pci_dev_t *` | [in] The PCI device. |
 *
 * @return TRUE.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dev_count` | `static int32_t` | The number of cards registered |
 * | `drv_info` | `ne2000_dev_t *` | The card |
 * | `name` | `char *` | The name of the network device |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `smalloc()` with the argument `sizeof(*drv_info)`, and store the
 *    result in `drv_info`.
 * 2. Call `rtl8029_init()` with the arguments `pci_dev` and `drv_info`.
 * 3. Call `smalloc()` with the argument `DEV_NAME_MAXLEN`, and store the result
 *    in `name`.
 * 4. Call `snprintf()` with the arguments `name`, `30`, `DEV_NAME_PREFIX"%%d"`
 *    and `dev_count`.
 * 5. Add `1` to `dev_count`.
 * 6. Return `TRUE`.
 *
 * @calls
 * - `smalloc()`
 * - `rtl8029_init()`
 * - `snprintf()`
 */
static pok_bool_t probe_device(pci_dev_t *pci_dev)
{
    static int32_t dev_count = 0;
    ne2000_dev_t *drv_info =  smalloc(sizeof(*drv_info));
    rtl8029_init(pci_dev, drv_info);

    /* Register netdevice */
    pok_netdevice_t *netdevice = smalloc(sizeof(*netdevice));
    netdevice->ops = &driver_ops,
    netdevice->mac = drv_info->mac,
    netdevice->info = drv_info;
    char *name = smalloc(DEV_NAME_MAXLEN);
    snprintf(name, 30, DEV_NAME_PREFIX"%d", dev_count);
    register_netdevice(name, netdevice);
    dev_count += 1;

    return TRUE;
}


const struct pci_dev_id ne2000_pci_tbl[] = {
    { 0x10ec, 0x8029},
    //Here may be others cards from ne2000 family
};

struct pci_driver ne2000_pci_driver = {
    .name     = DRV_NAME,
    .probe    = probe_device,
    .id_table = ne2000_pci_tbl
};

/**
 * @funcname `ne2000_net_init`
 *
 * @description
 * Registers the NE2000 PCI driver.
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
 * 1. Do nothing.
 *
 * @calls
 * None.
 */
void ne2000_net_init()
{
    register_pci_driver(&ne2000_pci_driver);
}

#endif

