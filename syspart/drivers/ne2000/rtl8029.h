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
 * Modified by Kirill Brazhnikov, 2026-10-31: fixed an #endif comment.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of rtl8029.c are here;
 * the callback has the type ne2000_packet_received_fn_t; s_ne2000_dev and
 * s_ne2000_header are ne2000_dev_t and ne2000_header_t; the macros and types
 * are documented.
 */

//TODO move define to config.h or delete

#include "syspart_config.h"
#include <stdint.h>

#ifdef SYS_NEEDS_DRIVER_NE2000

#  include <libc/stdio.h>
#  include <libc/string.h>
#  include <pci.h>
//#  include <arch/x86/ioports.h>
#  include <middleware/port.h>

//TODO: should be 2**16?

/**
 * @macroname `ETH_DATA_MAXLEN`
 *
 * @description
 * The maximum size of the data of a frame (pok_packet_t).
 */
#  define ETH_DATA_MAXLEN	65536

/**
 * @macroname `ETH_DATA_MINLEN`
 *
 * @description
 * The minimum size of the data of an Ethernet frame.
 */
#  define ETH_DATA_MINLEN	46

/**
 * @macroname `ETH_MAC_LEN`
 *
 * @description
 * The size of a MAC address.
 */
#  define ETH_MAC_LEN		6

/**
 * @macroname `RECV_BUF_SZ`
 *
 * @description
 * The number of receive buffers.
 */
#  define RECV_BUF_SZ		20

/**
 * @macroname `NE2000_ETH_DATA_MINLEN`
 *
 * @description
 * The minimum size of a frame the card sends.
 */
#  define NE2000_ETH_DATA_MINLEN 64

/**
 * @typename `pok_packet_t`
 *
 * @description
 * The data of a frame.
 */
typedef struct
{
    char data[ETH_DATA_MAXLEN]; /**< The data. */
} pok_packet_t;

/**
 * @typename `ne2000_packet_received_fn_t`
 *
 * @description
 * Gets a frame received by the card: the frame and its size.
 */
typedef void (*ne2000_packet_received_fn_t)(const char *frame,
    size_t size);

/**
 * @typename `ne2000_dev_t`
 *
 * @description
 * An NE2000 network card.
 */
typedef struct
{
    pci_dev_t  pci; /**< The PCI device. */
    uint32_t      addr; /**< The base I/O port of the card. */
    uint8_t       mac[6]; /**< The MAC address of the card. */
    /**
     * Gets the frames received.
     */
    ne2000_packet_received_fn_t packet_received_callback;
} ne2000_dev_t;

/**
 * @typename `ne2000_header_t`
 *
 * @description
 * The header the card writes before each received frame in its memory.
 */
typedef struct
{
    uint8_t status; /**< The receive status (NE2000_RSR_*). */
    uint8_t next; /**< The page of the next frame. */
    uint16_t size; /**< The size of the frame, with this header. */
} __attribute__ ((packed)) ne2000_header_t;

/**
 * @name NE2000 command register
 * CR: Command Register
 * @{
 */

/**
 * @macroname `NE2000_CR`
 *
 * @description
 * The address of the Command Register (CR).
 */
#  define NE2000_CR	0x00

/**
 * @macroname `NE2000_CR_STP`
 *
 * @description
 * This bit is the STOP command. When it is set, no packets will be received or
 * transmitted. POWER UP=1.
 */
#  define NE2000_CR_STP	(1 << 0)

/**
 * @macroname `NE2000_CR_STA`
 *
 * @description
 * The STA bit controls nothing. It only reflects the value written to this bit.
 * POWER UP=0.
 */
#  define NE2000_CR_STA	(1 << 1)

/**
 * @macroname `NE2000_CR_TXP`
 *
 * @description
 * This bit must be set to transmit a packet. It is internally reset either
 * after the transmission is completed or aborted.
 */
#  define NE2000_CR_TXP	(1 << 2)

/**
 * @macroname `NE2000_CR_RD0`
 *
 * @description
 * See RD2-0 in the datasheet.
 */
#  define NE2000_CR_RD0	(1 << 3)

/**
 * @macroname `NE2000_CR_RD1`
 *
 * @description
 * See RD2-0 in the datasheet.
 */
#  define NE2000_CR_RD1	(1 << 4)

/**
 * @macroname `NE2000_CR_RD2`
 *
 * @description
 * See RD2-0 in the datasheet.
 */
#  define NE2000_CR_RD2	(1 << 5)

/**
 * @macroname `NE2000_CR_PS0`
 *
 * @description
 * See PS0 in the datasheet.
 */
#  define NE2000_CR_PS0	(1 << 6)

/**
 * @macroname `NE2000_CR_PS1`
 *
 * @description
 * See PS1 in the datasheet.
 */
#  define NE2000_CR_PS1	(1 << 7)

/** @} */


/**
 * @name NE2000 interrupt status register
 * ISR: Interrupt Status Register
 * @{
 */

/**
 * @macroname `NE2000_ISR`
 *
 * @description
 * The address of the Interrupt Status Register (ISR).
 */
#  define NE2000_ISR	0x07

/**
 * @macroname `NE2000_ISR_PRX`
 *
 * @description
 * This bit indicates packet received with no errors.
 */
#  define NE2000_ISR_PRX	(1 << 0)

/**
 * @macroname `NE2000_ISR_PTX`
 *
 * @description
 * This bit indicates packet transmitted with no errors.
 */
#  define NE2000_ISR_PTX	(1 << 1)

/**
 * @macroname `NE2000_ISR_RXE`
 *
 * @description
 * This bit is set when a packet is received with one or more of the following
 * errors: a CRC error, a frame alignment error or a missed packet.
 */
#  define NE2000_ISR_RXE	(1 << 2)

/**
 * @macroname `NE2000_ISR_TXE`
 *
 * @description
 * Transmit error bit is set when a packet transmission is aborted due to
 * excessive collisions.
 */
#  define NE2000_ISR_TXE	(1 << 3)

/**
 * @macroname `NE2000_ISR_OVW`
 *
 * @description
 * This bit is set when the receive buffer has been exhausted.
 */
#  define NE2000_ISR_OVW	(1 << 4)

/**
 * @macroname `NE2000_ISR_CNT`
 *
 * @description
 * Set when MSB of one or more of the network tally counters has been set.
 */
#  define NE2000_ISR_CNT	(1 << 5)

/**
 * @macroname `NE2000_ISR_RDC`
 *
 * @description
 * Set when remote DMA operation has been completed.
 */
#  define NE2000_ISR_RDC	(1 << 6)

/**
 * @macroname `NE2000_ISR_RST`
 *
 * @description
 * This bit is set when NIC enters reset state and is cleared when a start
 * command is issued to the CR. It is also set when receive buffer overflows and
 * is cleared when one or more packets have been read from the buffer.
 */
#  define NE2000_ISR_RST (1 << 7)

/** @} */


/**
 * @name NE2000 data configuration register
 * DCR: Data Configuration Register
 * @{
 */

/**
 * @macroname `NE2000_DCR`
 *
 * @description
 * The address of the Data Configuration Register (DCR).
 */
#  define NE2000_DCR	0x0E

/**
 * @macroname `NE2000_DCR_WTS`
 *
 * @description
 * Word Transfer Select 0: byte-wide DMA transfer 1: word-wide DMA transfer.
 */
#  define NE2000_DCR_WTS	(1 << 0)

/**
 * @macroname `NE2000_DCR_BOS`
 *
 * @description
 * Byte Order Select 0: MS byte placed on MD15-8 and LS byte on MD7-0.
 * (32xxx,80x86) 1: MS byte placed on MD7-0 and LS byte on MD15-8. (680x0).
 */
#  define NE2000_DCR_BOS	(1 << 1)

/**
 * @macroname `NE2000_DCR_LAS`
 *
 * @description
 * This bit must be set to zero. NIC only supports dual 16-bit DMA mode. POWER
 * UP =1.
 */
#  define NE2000_DCR_LAS	(1 << 2)

/**
 * @macroname `NE2000_DCR_LS`
 *
 * @description
 * Loopback Select 0: Loopback mode selected. Bits 1 and 2 of the TCR must also
 * be programmed for Loopback operation. 1: Normal Operation.
 */
#  define NE2000_DCR_LS	(1 << 3)

/**
 * @macroname `NE2000_DCR_ARM`
 *
 * @description
 * Auto-initialize Remote 0: Send Packet Command not executed. 1: Send Packet
 * Command executed. This bit is set when the receive buffer.
 */
#  define NE2000_DCR_ARM	(1 << 4)

/**
 * @macroname `NE2000_DCR_FT0`
 *
 * @description
 * FIFO threshold select bit 0.
 */
#  define NE2000_DCR_FT0	(1 << 5)

/**
 * @macroname `NE2000_DCR_FT1`
 *
 * @description
 * FIFO threshold select bit 1.
 */
#  define NE2000_DCR_FT1	(1 << 6)

/** @} */


/**
 * @name NE2000 receive configuration register
 * RCR: Receive Configuration Register
 * @{
 */

/**
 * @macroname `NE2000_RCR`
 *
 * @description
 * The address of the Receive Configuration Register (RCR).
 */
#  define NE2000_RCR	0x0C

/**
 * @macroname `NE2000_RCR_AB`
 *
 * @description
 * If AB=1, packets with broadcast destination address are accepted. If AB=0,
 * packets with broadcast destination address are rejected.
 */
#  define NE2000_RCR_AB	(1 << 2)

/**
 * @macroname `NE2000_RCR_AR`
 *
 * @description
 * If AR=1, packets with length fewer than 64 bytes are accepted. If AR=0,
 * packets with length fewer than 64 bytes are rejected.
 */
#  define NE2000_RCR_AR (1)

/** @} */


/**
 * @name NE2000 receive status register
 * RSR: Receive Status Register
 * @{
 */

/**
 * @macroname `NE2000_RSR`
 *
 * @description
 * The address of the Receive Status Register (RSR).
 */
#  define NE2000_RSR	0x0C

/**
 * @macroname `NE2000_RSR_PRX`
 *
 * @description
 * This bit indicates packet received with no errors.
 */
#  define NE2000_RSR_PRX	(1 << 0)

/** @} */


/**
 * @macroname `NE2000_IMR`
 *
 * @description
 * The address of the Interrupt Mask Register (IMR).
 */
#  define NE2000_IMR	0x0F

/**
 * @macroname `NE2000_PSTART`
 *
 * @description
 * The address of the Page Start Register (PSTART).
 */
#  define NE2000_PSTART	0x01

/**
 * @macroname `NE2000_PSTOP`
 *
 * @description
 * The address of the Page Stop Register (PSTOP).
 */
#  define NE2000_PSTOP	0x02

/**
 * @macroname `NE2000_BNRY`
 *
 * @description
 * The address of the Boundary Register (BNRY).
 */
#  define NE2000_BNRY	0x03

/**
 * @macroname `NE2000_TPSR`
 *
 * @description
 * The address of the Transmit Page Start Register (TPSR).
 */
#  define NE2000_TPSR	0x04

/**
 * @macroname `NE2000_TCR`
 *
 * @description
 * The address of the Transmit Configuration Register (TCR).
 */
#  define NE2000_TCR	0x0D

/**
 * @macroname `NE2000_CURR`
 *
 * @description
 * The address of the Current Page Register (CURR).
 */
#  define NE2000_CURR	0x07

/**
 * @name NE2000 transmit byte count registers
 * TBCR0,1: Transmit Byte Count Registers
 * @{
 */

/**
 * @macroname `NE2000_TBCR0`
 *
 * @description
 * The address of the low byte of the transmit byte count (TBCR0).
 */
#  define NE2000_TBCR0	0x05

/**
 * @macroname `NE2000_TBCR1`
 *
 * @description
 * The address of the high byte of the transmit byte count (TBCR1).
 */
#  define NE2000_TBCR1	0x06

/** @} */



/**
 * @name NE2000 remote start address registers
 * RSAR0,1: Remote Start Address Registers
 * @{
 */

/**
 * @macroname `NE2000_RSAR0`
 *
 * @description
 * The address of the low byte of the remote start address (RSAR0).
 */
#  define NE2000_RSAR0	0x08

/**
 * @macroname `NE2000_RSAR1`
 *
 * @description
 * The address of the high byte of the remote start address (RSAR1).
 */
#  define NE2000_RSAR1	0x09

/** @} */


/**
 * @name NE2000 remote byte count registers
 * RBCR0,1: Remote Byte Count Registers
 * @{
 */

/**
 * @macroname `NE2000_RBCR0`
 *
 * @description
 * The address of the low byte of the remote byte count (RBCR0).
 */
#  define NE2000_RBCR0	0x0A

/**
 * @macroname `NE2000_RBCR1`
 *
 * @description
 * The address of the high byte of the remote byte count (RBCR1).
 */
#  define NE2000_RBCR1	0x0B

/** @} */


/**
 * @name NE2000 physical address registers
 * PAR0-5: Physical Address Registers
 * @{
 */

/**
 * @macroname `NE2000_PAR0`
 *
 * @description
 * The address of byte 0 of the physical (MAC) address (PAR0).
 */
#  define NE2000_PAR0	0x01

/**
 * @macroname `NE2000_PAR1`
 *
 * @description
 * The address of byte 1 of the physical (MAC) address (PAR1).
 */
#  define NE2000_PAR1	0x02

/**
 * @macroname `NE2000_PAR2`
 *
 * @description
 * The address of byte 2 of the physical (MAC) address (PAR2).
 */
#  define NE2000_PAR2	0x03

/**
 * @macroname `NE2000_PAR3`
 *
 * @description
 * The address of byte 3 of the physical (MAC) address (PAR3).
 */
#  define NE2000_PAR3	0x04

/**
 * @macroname `NE2000_PAR4`
 *
 * @description
 * The address of byte 4 of the physical (MAC) address (PAR4).
 */
#  define NE2000_PAR4	0x05

/**
 * @macroname `NE2000_PAR5`
 *
 * @description
 * The address of byte 5 of the physical (MAC) address (PAR5).
 */
#  define NE2000_PAR5	0x06

/** @} */



/**
 * @macroname `NE2000_MAR`
 *
 * @description
 * The address of the first Multicast Address Register (MAR0-7).
 */
#  define NE2000_MAR	0x08

/**
 * @macroname `NE2000_DMA_PORT`
 *
 * @description
 * The address of the remote DMA port.
 */
#  define NE2000_DMA_PORT 0x10


/**
 * @macroname `NE2000_MEMSZ`
 *
 * @description
 * The size of the memory of the card, in 256-byte pages.
 */
#  define NE2000_MEMSZ	128

/**
 * @macroname `NE2000_TXBUF`
 *
 * @description
 * The offset of the transmit buffer, in 256-byte pages.
 */
#  define NE2000_TXBUF	64

/**
 * @macroname `NE2000_TXBUFSZ`
 *
 * @description
 * The size of the transmit buffer, in 256-byte pages.
 */
#  define NE2000_TXBUFSZ 8

/**
 * @macroname `NE2000_RXBUF`
 *
 * @description
 * The offset of the receive buffer, in 256-byte pages.
 */
#  define NE2000_RXBUF	(NE2000_TXBUF + NE2000_TXBUFSZ)

/**
 * @macroname `NE2000_RXBUFSZ`
 *
 * @description
 * The size of the receive buffer, in 256-byte pages.
 */
#  define NE2000_RXBUFSZ (NE2000_MEMSZ - NE2000_TXBUFSZ)
// in 256 bytes block
//

/**
 * @macroname `DRV_NAME`
 *
 * @description
 * The name of the driver.
 */
#define DRV_NAME "ne2000-net"

/**
 * @macroname `DEV_NAME_PREFIX`
 *
 * @description
 * The prefix of the names of the devices (the name of the driver).
 */
#define DEV_NAME_PREFIX DRV_NAME

/**
 * @macroname `DEV_NAME_MAXLEN`
 *
 * @description
 * The maximum length of the name of a device.
 */
#define DEV_NAME_MAXLEN 20

/**
 * @macroname `outb_inverse`
 *
 * @description
 * Writes the byte `a` to the port `b` (outb() with its arguments the other way
 * round).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `a` | The byte. |
 * | `b` | The port. |
 */
#define outb_inverse(a,b) outb((b), (a))

/**
 * @macroname `NE2000_SELECT_PAGE`
 *
 * @description
 * Selects the register page `page` of the card. Page 0 is always assumed to be
 * selected, except during the initialization and the polling: each time page 1
 * is needed, the card is switched back to page 0 when done.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `dev` | The card (ne2000_dev_t *). |
 * | `page` | The page (0 to 3). |
 */
#define NE2000_SELECT_PAGE(dev, page)                                   \
    outb_inverse((inb((dev)->addr + NE2000_CR) &                          \
                ~(NE2000_CR_PS0 | NE2000_CR_PS1)) | ((page) << 6), (dev)->addr)

#endif /*SYS_NEEDS_DRIVER_NE2000*/
