/*
 * Modified by Kirill Brazhnikov, 2026-11-14: the values of the flags and
 * gso_type members are defined before the struct; the types are typedefs
 * without a tag, documented with the macros.
 */

#ifndef POK_VIRTIO_NET_H
#define POK_VIRTIO_NET_H

// for ETH_ALEN
#include <net/ether.h>

/* This header is BSD licensed so anyone can use the definitions to implement
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
 * isn't a reserved identifier; deviations are documented where a guideline
 * can't be followed.
 */

/**
 * @name Virtio network feature bits
 * The feature bitmap for virtio net
 * @{
 */

/**
 * @macroname `VIRTIO_NET_F_CSUM`
 *
 * @description
 * Host handles pkts w/ partial csum.
 */
#define VIRTIO_NET_F_CSUM	0

/**
 * @macroname `VIRTIO_NET_F_GUEST_CSUM`
 *
 * @description
 * Guest handles pkts w/ partial csum.
 */
#define VIRTIO_NET_F_GUEST_CSUM	1

/**
 * @macroname `VIRTIO_NET_F_MAC`
 *
 * @description
 * Host has given MAC address.
 */
#define VIRTIO_NET_F_MAC	5

/**
 * @macroname `VIRTIO_NET_F_GSO`
 *
 * @description
 * Host handles pkts w/ any GSO type.
 */
#define VIRTIO_NET_F_GSO	6

/**
 * @macroname `VIRTIO_NET_F_GUEST_TSO4`
 *
 * @description
 * Guest can handle TSOv4 in.
 */
#define VIRTIO_NET_F_GUEST_TSO4	7

/**
 * @macroname `VIRTIO_NET_F_GUEST_TSO6`
 *
 * @description
 * Guest can handle TSOv6 in.
 */
#define VIRTIO_NET_F_GUEST_TSO6	8

/**
 * @macroname `VIRTIO_NET_F_GUEST_ECN`
 *
 * @description
 * Guest can handle TSO[6] w/ ECN in.
 */
#define VIRTIO_NET_F_GUEST_ECN	9

/**
 * @macroname `VIRTIO_NET_F_GUEST_UFO`
 *
 * @description
 * Guest can handle UFO in.
 */
#define VIRTIO_NET_F_GUEST_UFO	10

/**
 * @macroname `VIRTIO_NET_F_HOST_TSO4`
 *
 * @description
 * Host can handle TSOv4 in.
 */
#define VIRTIO_NET_F_HOST_TSO4	11

/**
 * @macroname `VIRTIO_NET_F_HOST_TSO6`
 *
 * @description
 * Host can handle TSOv6 in.
 */
#define VIRTIO_NET_F_HOST_TSO6	12

/**
 * @macroname `VIRTIO_NET_F_HOST_ECN`
 *
 * @description
 * Host can handle TSO[6] w/ ECN in.
 */
#define VIRTIO_NET_F_HOST_ECN	13

/**
 * @macroname `VIRTIO_NET_F_HOST_UFO`
 *
 * @description
 * Host can handle UFO in.
 */
#define VIRTIO_NET_F_HOST_UFO	14

/**
 * @macroname `VIRTIO_NET_F_MRG_RXBUF`
 *
 * @description
 * Host can merge receive buffers.
 */
#define VIRTIO_NET_F_MRG_RXBUF	15

/**
 * @macroname `VIRTIO_NET_F_STATUS`
 *
 * @description
 * virtio_net_config.status available.
 */
#define VIRTIO_NET_F_STATUS	16

/**
 * @macroname `VIRTIO_NET_F_CTRL_VQ`
 *
 * @description
 * Control channel available.
 */
#define VIRTIO_NET_F_CTRL_VQ	17

/**
 * @macroname `VIRTIO_NET_F_CTRL_RX`
 *
 * @description
 * Control channel RX mode support.
 */
#define VIRTIO_NET_F_CTRL_RX	18

/**
 * @macroname `VIRTIO_NET_F_CTRL_VLAN`
 *
 * @description
 * Control channel VLAN filtering.
 */
#define VIRTIO_NET_F_CTRL_VLAN	19

/**
 * @macroname `VIRTIO_NET_F_CTRL_RX_EXTRA`
 *
 * @description
 * Extra RX mode control support.
 */
#define VIRTIO_NET_F_CTRL_RX_EXTRA 20

/**
 * @macroname `VIRTIO_NET_F_GUEST_ANNOUNCE`
 *
 * @description
 * Guest can announce device on the network.
 */
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21

/**
 * @macroname `VIRTIO_NET_F_MQ`
 *
 * @description
 * Device supports Receive Flow Steering.
 */
#define VIRTIO_NET_F_MQ	22

/**
 * @macroname `VIRTIO_NET_F_CTRL_MAC_ADDR`
 *
 * @description
 * Set MAC address.
 */
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23

/** @} */


/**
 * @name Virtio network status bits
 * The bits of the status of the device (virtio_net_config_t, with
 * VIRTIO_NET_F_STATUS).
 * @{
 */

/**
 * @macroname `VIRTIO_NET_S_LINK_UP`
 *
 * @description
 * Link is up.
 */
#define VIRTIO_NET_S_LINK_UP	1

/**
 * @macroname `VIRTIO_NET_S_ANNOUNCE`
 *
 * @description
 * Announcement is needed.
 */
#define VIRTIO_NET_S_ANNOUNCE	2

/** @} */


/**
 * @typename `virtio_net_config_t`
 *
 * @description
 * The configuration space of a virtio network device.
 */
typedef struct {
	/**
	 * The config defining mac address (if VIRTIO_NET_F_MAC).
	 */
	uint8_t mac[ETH_ALEN];
	/**
	 * See VIRTIO_NET_F_STATUS and VIRTIO_NET_S_* above.
	 */
	uint16_t status;
	/**
	 * Maximum number of each of transmit and receive queues; see
	 * VIRTIO_NET_F_MQ and VIRTIO_NET_CTRL_MQ. Legal values are between 1
	 * and 0x8000.
	 */
	uint16_t max_virtqueue_pairs;
} __attribute__((packed)) virtio_net_config_t;

/**
 * @name Virtio network header flags
 * The flags of a virtio_net_hdr_t.
 * @{
 */

/**
 * @macroname `VIRTIO_NET_HDR_F_NEEDS_CSUM`
 *
 * @description
 * Use csum_start, csum_offset.
 */
#define VIRTIO_NET_HDR_F_NEEDS_CSUM	1

/**
 * @macroname `VIRTIO_NET_HDR_F_DATA_VALID`
 *
 * @description
 * Csum is valid.
 */
#define VIRTIO_NET_HDR_F_DATA_VALID	2

/** @} */

/**
 * @name Virtio network GSO types
 * The gso_type of a virtio_net_hdr_t.
 * @{
 */

/**
 * @macroname `VIRTIO_NET_HDR_GSO_NONE`
 *
 * @description
 * Not a GSO frame.
 */
#define VIRTIO_NET_HDR_GSO_NONE		0

/**
 * @macroname `VIRTIO_NET_HDR_GSO_TCPV4`
 *
 * @description
 * GSO frame, IPv4 TCP (TSO).
 */
#define VIRTIO_NET_HDR_GSO_TCPV4	1

/**
 * @macroname `VIRTIO_NET_HDR_GSO_UDP`
 *
 * @description
 * GSO frame, IPv4 UDP (UFO).
 */
#define VIRTIO_NET_HDR_GSO_UDP		3

/**
 * @macroname `VIRTIO_NET_HDR_GSO_TCPV6`
 *
 * @description
 * GSO frame, IPv6 TCP.
 */
#define VIRTIO_NET_HDR_GSO_TCPV6	4

/**
 * @macroname `VIRTIO_NET_HDR_GSO_ECN`
 *
 * @description
 * TCP has ECN set.
 */
#define VIRTIO_NET_HDR_GSO_ECN		0x80

/** @} */

/**
 * @typename `virtio_net_hdr_t`
 *
 * @description
 * The header of a frame on the virtqueues. It comes first in the scatter-gather
 * list: if VIRTIO_F_ANY_LAYOUT is not negotiated, it must be the first element
 * of the list. Without the GSO or CSUM features, it can simply be ignored.
 */
typedef struct {
	uint8_t flags; /**< The flags (VIRTIO_NET_HDR_F_*). */
	uint8_t gso_type; /**< The GSO type (VIRTIO_NET_HDR_GSO_*). */
	uint16_t hdr_len; /**< Ethernet + IP + tcp/udp hdrs. */
	uint16_t gso_size; /**< Bytes to append to hdr_len per frame. */
	uint16_t csum_start; /**< Position to start checksumming from. */
	uint16_t csum_offset; /**< Offset after that to place checksum. */
} virtio_net_hdr_t;

/**
 * @typename `virtio_net_hdr_mrg_rxbuf_t`
 *
 * @description
 * This is the version of the header to use when the MRG_RXBUF feature has been
 * negotiated.
 */
typedef struct {
	virtio_net_hdr_t hdr; /**< The header. */
	uint16_t num_buffers; /**< Number of merged rx buffers. */
} virtio_net_hdr_mrg_rxbuf_t;

/**
 * @typename `virtio_net_ctrl_hdr_t`
 *
 * @description
 * Control virtqueue data structures
 *
 * The control virtqueue expects a header in the first sg entry and an
 * ack/status response in the last entry. Data for the command goes in between.
 */
typedef struct {
	uint8_t class; /**< The class of the command (VIRTIO_NET_CTRL_*). */
	uint8_t cmd; /**< The command in its class. */
} __attribute__((packed)) virtio_net_ctrl_hdr_t;

/**
 * @typename `virtio_net_ctrl_ack_t`
 *
 * @description
 * The status a command of the control virtqueue returns (VIRTIO_NET_OK or
 * VIRTIO_NET_ERR).
 */
typedef uint8_t virtio_net_ctrl_ack_t;

/**
 * @name Virtio network control acknowledgements
 * The status a command of the control virtqueue returns.
 * @{
 */

/**
 * @macroname `VIRTIO_NET_OK`
 *
 * @description
 * The command succeeded (virtio_net_ctrl_ack_t).
 */
#define VIRTIO_NET_OK     0

/**
 * @macroname `VIRTIO_NET_ERR`
 *
 * @description
 * The command failed (virtio_net_ctrl_ack_t).
 */
#define VIRTIO_NET_ERR    1

/** @} */


/**
 * @name Virtio network RX mode commands
 * Control the RX mode, ie. promisucous, allmulti, etc... All commands require
 * an "out" sg entry containing a 1 byte state value, zero = disable, non-zero =
 * enable. Commands 0 and 1 are supported with the VIRTIO_NET_F_CTRL_RX feature.
 * Commands 2-5 are added with VIRTIO_NET_F_CTRL_RX_EXTRA.
 * @{
 */

/**
 * @macroname `VIRTIO_NET_CTRL_RX`
 *
 * @description
 * The class of the RX mode commands.
 */
#define VIRTIO_NET_CTRL_RX    0

/**
 * @macroname `VIRTIO_NET_CTRL_RX_PROMISC`
 *
 * @description
 * Promiscuous mode.
 */
 #define VIRTIO_NET_CTRL_RX_PROMISC      0

/**
 * @macroname `VIRTIO_NET_CTRL_RX_ALLMULTI`
 *
 * @description
 * All multicast frames.
 */
 #define VIRTIO_NET_CTRL_RX_ALLMULTI     1

/**
 * @macroname `VIRTIO_NET_CTRL_RX_ALLUNI`
 *
 * @description
 * All unicast frames.
 */
 #define VIRTIO_NET_CTRL_RX_ALLUNI       2

/**
 * @macroname `VIRTIO_NET_CTRL_RX_NOMULTI`
 *
 * @description
 * No multicast frames.
 */
 #define VIRTIO_NET_CTRL_RX_NOMULTI      3

/**
 * @macroname `VIRTIO_NET_CTRL_RX_NOUNI`
 *
 * @description
 * No unicast frames.
 */
 #define VIRTIO_NET_CTRL_RX_NOUNI        4

/**
 * @macroname `VIRTIO_NET_CTRL_RX_NOBCAST`
 *
 * @description
 * No broadcast frames.
 */
 #define VIRTIO_NET_CTRL_RX_NOBCAST      5

/** @} */


/**
 * @typename `virtio_net_ctrl_mac_t`
 *
 * @description
 * Control the MAC
 *
 * The MAC filter table is managed by the hypervisor, the guest should assume
 * the size is infinite. Filtering should be considered non-perfect, ie. based
 * on hypervisor resources, the guest may received packets from sources not
 * specified in the filter list.
 *
 * In addition to the class/cmd header, the TABLE_SET command requires two out
 * scatterlists. Each contains a 4 byte count of entries followed by a
 * concatenated byte stream of the ETH_ALEN MAC addresses. The first sg list
 * contains unicast addresses, the second is for multicast. This functionality
 * is present if the VIRTIO_NET_F_CTRL_RX feature is available.
 *
 * The ADDR_SET command requests one out scatterlist, it contains a 6 bytes MAC
 * address. This functionality is present if the VIRTIO_NET_F_CTRL_MAC_ADDR
 * feature is available.
 */
typedef struct {
	uint32_t entries; /**< The number of MAC addresses. */
	/* MISRA C:2025 Rule 18.7 deviation: "entries" MAC addresses follow the
	 * count. */
	uint8_t macs[][ETH_ALEN]; /**< The MAC addresses. */
} __attribute__((packed)) virtio_net_ctrl_mac_t;

/**
 * @name Virtio network MAC commands
 * The commands of the MAC filter (see the comment above virtio_net_ctrl_mac_t).
 * @{
 */

/**
 * @macroname `VIRTIO_NET_CTRL_MAC`
 *
 * @description
 * The class of the MAC filter commands.
 */
#define VIRTIO_NET_CTRL_MAC    1

/**
 * @macroname `VIRTIO_NET_CTRL_MAC_TABLE_SET`
 *
 * @description
 * Sets the MAC filter table.
 */
 #define VIRTIO_NET_CTRL_MAC_TABLE_SET        0

/**
 * @macroname `VIRTIO_NET_CTRL_MAC_ADDR_SET`
 *
 * @description
 * Sets the MAC address.
 */
 #define VIRTIO_NET_CTRL_MAC_ADDR_SET         1

/** @} */


/**
 * @name Virtio network VLAN commands
 * Control VLAN filtering
 *
 * The VLAN filter table is controlled via a simple ADD/DEL interface. VLAN IDs
 * not added may be filterd by the hypervisor. Del is the opposite of add. Both
 * commands expect an out entry containing a 2 byte VLAN ID. VLAN filterting is
 * available with the VIRTIO_NET_F_CTRL_VLAN feature bit.
 * @{
 */

/**
 * @macroname `VIRTIO_NET_CTRL_VLAN`
 *
 * @description
 * The class of the VLAN filter commands.
 */
#define VIRTIO_NET_CTRL_VLAN       2

/**
 * @macroname `VIRTIO_NET_CTRL_VLAN_ADD`
 *
 * @description
 * Adds a VLAN ID to the filter.
 */
 #define VIRTIO_NET_CTRL_VLAN_ADD             0

/**
 * @macroname `VIRTIO_NET_CTRL_VLAN_DEL`
 *
 * @description
 * Deletes a VLAN ID from the filter.
 */
 #define VIRTIO_NET_CTRL_VLAN_DEL             1

/** @} */


/**
 * @name Virtio network announce commands
 * Control link announce acknowledgement
 *
 * The command VIRTIO_NET_CTRL_ANNOUNCE_ACK is used to indicate that driver has
 * recevied the notification; device would clear the VIRTIO_NET_S_ANNOUNCE bit
 * in the status field after it receives this command.
 * @{
 */

/**
 * @macroname `VIRTIO_NET_CTRL_ANNOUNCE`
 *
 * @description
 * The class of the announce commands.
 */
#define VIRTIO_NET_CTRL_ANNOUNCE       3

/**
 * @macroname `VIRTIO_NET_CTRL_ANNOUNCE_ACK`
 *
 * @description
 * Acknowledges the announcement.
 */
 #define VIRTIO_NET_CTRL_ANNOUNCE_ACK         0

/** @} */


/**
 * @typename `virtio_net_ctrl_mq_t`
 *
 * @description
 * Control Receive Flow Steering
 *
 * The command VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET enables Receive Flow Steering,
 * specifying the number of the transmit and receive queues that will be used.
 * After the command is consumed and acked by the device, the device will not
 * steer new packets on receive virtqueues other than specified nor read from
 * transmit virtqueues other than specified. Accordingly, driver should not
 * transmit new packets on virtqueues other than specified.
 */
typedef struct {
	uint16_t virtqueue_pairs; /**< The number of queue pairs. */
} virtio_net_ctrl_mq_t;

/**
 * @name Virtio network flow steering commands
 * The commands of Receive Flow Steering (see the comment above
 * virtio_net_ctrl_mq_t).
 * @{
 */

/**
 * @macroname `VIRTIO_NET_CTRL_MQ`
 *
 * @description
 * The class of the Receive Flow Steering commands.
 */
#define VIRTIO_NET_CTRL_MQ   4

/**
 * @macroname `VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET`
 *
 * @description
 * Sets the number of queue pairs.
 */
 #define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET        0

/**
 * @macroname `VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN`
 *
 * @description
 * The minimum number of queue pairs.
 */
 #define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN        1

/**
 * @macroname `VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX`
 *
 * @description
 * The maximum number of queue pairs.
 */
 #define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX        0x8000

/** @} */


#endif /* POK_VIRTIO_NET_H */
