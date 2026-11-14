/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (P2/components-glue/config.yaml).
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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions (the format of the function documentation, see
 * README.md).
 */

/**
 * @file
 * The glue code of the components of a partition: the component
 * instances, their initialization and links, and the loop of their
 * activities (generated from misc/templates/glue.c.tpl).
 */

struct port_ops{
    void *ops;
    void *owner;
};

    #include <ARINC_SENDER_gen.h>
        void __ARINC_SENDER_init__(ARINC_SENDER*);
        void __ARINC_SENDER_activity__(ARINC_SENDER*);
        ARINC_SENDER arinc_sender_1 = {
            .state = {
                .port_name = "UOUT",
                .port_direction = DESTINATION,
                .overhead = 42,
                .port_max_message_size = 64,
                .is_queuing_port = 1,
                .q_port_max_nb_messages = 10,
            },

        };

    #include <UDP_IP_SENDER_gen.h>
        void __UDP_IP_SENDER_init__(UDP_IP_SENDER*);
        void __UDP_IP_SENDER_activity__(UDP_IP_SENDER*);
        UDP_IP_SENDER udp_ip_sender_1 = {
            .state = {
                .src_ip = IP_ADDR(192, 168, 56, 101),
                .src_port = 10002,
                .dst_ip = IP_ADDR(192, 168, 56, 1),
                .dst_port = 10003,
                .dst_mac = {0x08, 0x00, 0x27, 0x00, 0x88, 0xAD},
            },

        };

    #include <MAC_SENDER_gen.h>
        void __MAC_SENDER_init__(MAC_SENDER*);
        void __MAC_SENDER_activity__(MAC_SENDER*);
        MAC_SENDER mac_sender_1 = {
            .state = {
                .src_mac = {0x52, 0x54, 0x00, 0x01, 0x02, 0x03},
            },

        };

    #include <VIRTIO_NET_DEV_gen.h>
        void __VIRTIO_NET_DEV_init__(VIRTIO_NET_DEV*);
        void __VIRTIO_NET_DEV_activity__(VIRTIO_NET_DEV*);
        VIRTIO_NET_DEV virtio_net_dev_1 = {
            .state = {
                .pci_bus = 0,
                .pci_dev = 2,
                .pci_fn = 0,
            },

        };

    #include <ARP_ANSWERER_gen.h>
        void __ARP_ANSWERER_init__(ARP_ANSWERER*);
        void __ARP_ANSWERER_activity__(ARP_ANSWERER*);
        ARP_ANSWERER arp_answerer_1 = {
            .state = {
                .good_ips = {IP_ADDR(192, 168, 56, 101),IP_ADDR(192, 168, 56, 102)},
                .good_ips_len = 2,
                .src_mac = {0x52, 0x54, 0x00, 0x01, 0x02, 0x03},
            },

        };

    #include <MAC_RECEIVER_gen.h>
        void __MAC_RECEIVER_init__(MAC_RECEIVER*);
        void __MAC_RECEIVER_activity__(MAC_RECEIVER*);
        MAC_RECEIVER mac_receiver_1 = {
            .state = {
                .my_mac = {0x52, 0x54, 0x00, 0x01, 0x02, 0x03},
            },

        };

    #include <MAC_SENDER_gen.h>
        void __MAC_SENDER_init__(MAC_SENDER*);
        void __MAC_SENDER_activity__(MAC_SENDER*);
        MAC_SENDER mac_sender_2 = {
            .state = {
                .src_mac = {0x52, 0x54, 0x01, 0x01, 0x02, 0x03},
            },

        };

    #include <VIRTIO_NET_DEV_gen.h>
        void __VIRTIO_NET_DEV_init__(VIRTIO_NET_DEV*);
        void __VIRTIO_NET_DEV_activity__(VIRTIO_NET_DEV*);
        VIRTIO_NET_DEV virtio_net_dev_2 = {
            .state = {
                .pci_bus = 0,
                .pci_dev = 3,
                .pci_fn = 0,
            },

        };

    #include <ARP_ANSWERER_gen.h>
        void __ARP_ANSWERER_init__(ARP_ANSWERER*);
        void __ARP_ANSWERER_activity__(ARP_ANSWERER*);
        ARP_ANSWERER arp_answerer_2 = {
            .state = {
                .good_ips = {IP_ADDR(192, 168, 0, 101),IP_ADDR(192, 168, 0, 102)},
                .good_ips_len = 2,
                .src_mac = {0x52, 0x54, 0x01, 0x01, 0x02, 0x03},
            },

        };

    #include <MAC_RECEIVER_gen.h>
        void __MAC_RECEIVER_init__(MAC_RECEIVER*);
        void __MAC_RECEIVER_activity__(MAC_RECEIVER*);
        MAC_RECEIVER mac_receiver_2 = {
            .state = {
                .my_mac = {0x52, 0x54, 0x01, 0x01, 0x02, 0x03},
            },

        };

    #include <UDP_RECEIVER_gen.h>
        void __UDP_RECEIVER_init__(UDP_RECEIVER*);
        void __UDP_RECEIVER_activity__(UDP_RECEIVER*);
        UDP_RECEIVER udp_receiver = {

        };

    #include <ROUTER_gen.h>
        void __ROUTER_init__(ROUTER*);
        void __ROUTER_activity__(ROUTER*);
            struct port_ops router_array_for_portArray[2];
        ROUTER router = {
            .state = {
                .map_ip_port_to_idx = {{IP_ADDR(192, 168, 56, 101), 10001},{IP_ADDR(192, 168, 56, 102), 10005}},
                .map_ip_port_to_idx_len = 2,
            },

            .out = {
                .portArray = (void *)router_array_for_portArray,
            }
        };

    #include <ARINC_RECEIVER_gen.h>
        void __ARINC_RECEIVER_init__(ARINC_RECEIVER*);
        void __ARINC_RECEIVER_activity__(ARINC_RECEIVER*);
        ARINC_RECEIVER arinc_receiver_1 = {
            .state = {
                .port_name = "UIN",
                .port_direction = SOURCE,
                .port_max_message_size = 64,
                .is_queuing_port = 1,
                .q_port_max_nb_messages = 10,
            },

        };

    #include <ARINC_RECEIVER_gen.h>
        void __ARINC_RECEIVER_init__(ARINC_RECEIVER*);
        void __ARINC_RECEIVER_activity__(ARINC_RECEIVER*);
        ARINC_RECEIVER arinc_receiver_2 = {
            .state = {
                .port_name = "__test__",
            },

        };



/**
 * @funcname `__components_init__`
 *
 * @description
 * Initializes the components of the partition: each one, then the links
 * from their out ports to the in ports of other components.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_sender_1` | `ARINC_SENDER` | The component instance arinc_sender_1 |
 * | `udp_ip_sender_1` | `UDP_IP_SENDER` | The component instance udp_ip_sender_1 |
 * | `mac_sender_1` | `MAC_SENDER` | The component instance mac_sender_1 |
 * | `virtio_net_dev_1` | `VIRTIO_NET_DEV` | The component instance virtio_net_dev_1 |
 * | `arp_answerer_1` | `ARP_ANSWERER` | The component instance arp_answerer_1 |
 * | `mac_receiver_1` | `MAC_RECEIVER` | The component instance mac_receiver_1 |
 * | `mac_sender_2` | `MAC_SENDER` | The component instance mac_sender_2 |
 * | `virtio_net_dev_2` | `VIRTIO_NET_DEV` | The component instance virtio_net_dev_2 |
 * | `arp_answerer_2` | `ARP_ANSWERER` | The component instance arp_answerer_2 |
 * | `mac_receiver_2` | `MAC_RECEIVER` | The component instance mac_receiver_2 |
 * | `udp_receiver` | `UDP_RECEIVER` | The component instance udp_receiver |
 * | `router` | `ROUTER` | The component instance router |
 * | `arinc_receiver_1` | `ARINC_RECEIVER` | The component instance arinc_receiver_1 |
 * | `arinc_receiver_2` | `ARINC_RECEIVER` | The component instance arinc_receiver_2 |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `__ARINC_SENDER_init__()` with the argument `&arinc_sender_1`.
 * 2. Call `__UDP_IP_SENDER_init__()` with the argument `&udp_ip_sender_1`.
 * 3. Call `__MAC_SENDER_init__()` with the argument `&mac_sender_1`.
 * 4. Call `__VIRTIO_NET_DEV_init__()` with the argument `&virtio_net_dev_1`.
 * 5. Call `__ARP_ANSWERER_init__()` with the argument `&arp_answerer_1`.
 * 6. Call `__MAC_RECEIVER_init__()` with the argument `&mac_receiver_1`.
 * 7. Call `__MAC_SENDER_init__()` with the argument `&mac_sender_2`.
 * 8. Call `__VIRTIO_NET_DEV_init__()` with the argument `&virtio_net_dev_2`.
 * 9. Call `__ARP_ANSWERER_init__()` with the argument `&arp_answerer_2`.
 * 10. Call `__MAC_RECEIVER_init__()` with the argument `&mac_receiver_2`.
 * 11. Call `__UDP_RECEIVER_init__()` with the argument `&udp_receiver`.
 * 12. Call `__ROUTER_init__()` with the argument `&router`.
 * 13. Call `__ARINC_RECEIVER_init__()` with the argument `&arinc_receiver_1`.
 * 14. Call `__ARINC_RECEIVER_init__()` with the argument `&arinc_receiver_2`.
 * 15. Set `arinc_sender_1.out.portA.ops` to `&udp_ip_sender_1.in.portA.ops`.
 * 16. Set `arinc_sender_1.out.portA.owner` to `&udp_ip_sender_1`.
 * 17. Set `udp_ip_sender_1.out.portB.ops` to `&mac_sender_1.in.portA.ops`.
 * 18. Set `udp_ip_sender_1.out.portB.owner` to `&mac_sender_1`.
 * 19. Set `mac_sender_1.out.portB.ops` to `&virtio_net_dev_1.in.portA.ops`.
 * 20. Set `mac_sender_1.out.portB.owner` to `&virtio_net_dev_1`.
 * 21. Set `virtio_net_dev_1.out.portB.ops` to `&mac_receiver_1.in.portA.ops`.
 * 22. Set `virtio_net_dev_1.out.portB.owner` to `&mac_receiver_1`.
 * 23. Set `mac_receiver_1.out.port_ARP.ops` to `&arp_answerer_1.in.portA.ops`.
 * 24. Set `mac_receiver_1.out.port_ARP.owner` to `&arp_answerer_1`.
 * 25. Set `arp_answerer_1.out.portB.ops` to `&mac_sender_1.in.portA.ops`.
 * 26. Set `arp_answerer_1.out.portB.owner` to `&mac_sender_1`.
 * 27. Set `mac_receiver_1.out.port_UDP.ops` to `&udp_receiver.in.portA.ops`.
 * 28. Set `mac_receiver_1.out.port_UDP.owner` to `&udp_receiver`.
 * 29. Set `udp_receiver.out.portB.ops` to `&router.in.portA.ops`.
 * 30. Set `udp_receiver.out.portB.owner` to `&router`.
 * 31. Set `mac_sender_2.out.portB.ops` to `&virtio_net_dev_2.in.portA.ops`.
 * 32. Set `mac_sender_2.out.portB.owner` to `&virtio_net_dev_2`.
 * 33. Set `virtio_net_dev_2.out.portB.ops` to `&mac_receiver_2.in.portA.ops`.
 * 34. Set `virtio_net_dev_2.out.portB.owner` to `&mac_receiver_2`.
 * 35. Set `mac_receiver_2.out.port_ARP.ops` to `&arp_answerer_2.in.portA.ops`.
 * 36. Set `mac_receiver_2.out.port_ARP.owner` to `&arp_answerer_2`.
 * 37. Set `arp_answerer_2.out.portB.ops` to `&mac_sender_2.in.portA.ops`.
 * 38. Set `arp_answerer_2.out.portB.owner` to `&mac_sender_2`.
 * 39. Set `mac_receiver_2.out.port_UDP.ops` to `&udp_receiver.in.portA.ops`.
 * 40. Set `mac_receiver_2.out.port_UDP.owner` to `&udp_receiver`.
 * 41. Set `router.out.portArray[0].ops` to `&arinc_receiver_1.in.portA.ops`.
 * 42. Set `router.out.portArray[0].owner` to `&arinc_receiver_1`.
 * 43. Set `router.out.portArray[1].ops` to `&arinc_receiver_2.in.portA.ops`.
 * 44. Set `router.out.portArray[1].owner` to `&arinc_receiver_2`.
 *
 * @calls
 * - `__ARINC_SENDER_init__()`
 * - `__UDP_IP_SENDER_init__()`
 * - `__MAC_SENDER_init__()`
 * - `__VIRTIO_NET_DEV_init__()`
 * - `__ARP_ANSWERER_init__()`
 * - `__MAC_RECEIVER_init__()`
 * - `__MAC_SENDER_init__()`
 * - `__VIRTIO_NET_DEV_init__()`
 * - `__ARP_ANSWERER_init__()`
 * - `__MAC_RECEIVER_init__()`
 * - `__UDP_RECEIVER_init__()`
 * - `__ROUTER_init__()`
 * - `__ARINC_RECEIVER_init__()`
 * - `__ARINC_RECEIVER_init__()`
 */
void __components_init__()
{
            __ARINC_SENDER_init__(&arinc_sender_1);

            __UDP_IP_SENDER_init__(&udp_ip_sender_1);

            __MAC_SENDER_init__(&mac_sender_1);

            __VIRTIO_NET_DEV_init__(&virtio_net_dev_1);

            __ARP_ANSWERER_init__(&arp_answerer_1);

            __MAC_RECEIVER_init__(&mac_receiver_1);

            __MAC_SENDER_init__(&mac_sender_2);

            __VIRTIO_NET_DEV_init__(&virtio_net_dev_2);

            __ARP_ANSWERER_init__(&arp_answerer_2);

            __MAC_RECEIVER_init__(&mac_receiver_2);

            __UDP_RECEIVER_init__(&udp_receiver);

            __ROUTER_init__(&router);

            __ARINC_RECEIVER_init__(&arinc_receiver_1);

            __ARINC_RECEIVER_init__(&arinc_receiver_2);


        arinc_sender_1.out.portA.ops = &udp_ip_sender_1.in.portA.ops;
        arinc_sender_1.out.portA.owner = &udp_ip_sender_1;
        udp_ip_sender_1.out.portB.ops = &mac_sender_1.in.portA.ops;
        udp_ip_sender_1.out.portB.owner = &mac_sender_1;
        mac_sender_1.out.portB.ops = &virtio_net_dev_1.in.portA.ops;
        mac_sender_1.out.portB.owner = &virtio_net_dev_1;
        virtio_net_dev_1.out.portB.ops = &mac_receiver_1.in.portA.ops;
        virtio_net_dev_1.out.portB.owner = &mac_receiver_1;
        mac_receiver_1.out.port_ARP.ops = &arp_answerer_1.in.portA.ops;
        mac_receiver_1.out.port_ARP.owner = &arp_answerer_1;
        arp_answerer_1.out.portB.ops = &mac_sender_1.in.portA.ops;
        arp_answerer_1.out.portB.owner = &mac_sender_1;
        mac_receiver_1.out.port_UDP.ops = &udp_receiver.in.portA.ops;
        mac_receiver_1.out.port_UDP.owner = &udp_receiver;
        udp_receiver.out.portB.ops = &router.in.portA.ops;
        udp_receiver.out.portB.owner = &router;
        mac_sender_2.out.portB.ops = &virtio_net_dev_2.in.portA.ops;
        mac_sender_2.out.portB.owner = &virtio_net_dev_2;
        virtio_net_dev_2.out.portB.ops = &mac_receiver_2.in.portA.ops;
        virtio_net_dev_2.out.portB.owner = &mac_receiver_2;
        mac_receiver_2.out.port_ARP.ops = &arp_answerer_2.in.portA.ops;
        mac_receiver_2.out.port_ARP.owner = &arp_answerer_2;
        arp_answerer_2.out.portB.ops = &mac_sender_2.in.portA.ops;
        arp_answerer_2.out.portB.owner = &mac_sender_2;
        mac_receiver_2.out.port_UDP.ops = &udp_receiver.in.portA.ops;
        mac_receiver_2.out.port_UDP.owner = &udp_receiver;
        router.out.portArray[0].ops = &arinc_receiver_1.in.portA.ops;
        router.out.portArray[0].owner = &arinc_receiver_1;
        router.out.portArray[1].ops = &arinc_receiver_2.in.portA.ops;
        router.out.portArray[1].owner = &arinc_receiver_2;

}

/**
 * @funcname `__components_activity__`
 *
 * @description
 * The loop of the partition's components: forever, one step of the
 * activity of each component in turn.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_sender_1` | `ARINC_SENDER` | The component instance arinc_sender_1 |
 * | `udp_ip_sender_1` | `UDP_IP_SENDER` | The component instance udp_ip_sender_1 |
 * | `mac_sender_1` | `MAC_SENDER` | The component instance mac_sender_1 |
 * | `virtio_net_dev_1` | `VIRTIO_NET_DEV` | The component instance virtio_net_dev_1 |
 * | `arp_answerer_1` | `ARP_ANSWERER` | The component instance arp_answerer_1 |
 * | `mac_receiver_1` | `MAC_RECEIVER` | The component instance mac_receiver_1 |
 * | `mac_sender_2` | `MAC_SENDER` | The component instance mac_sender_2 |
 * | `virtio_net_dev_2` | `VIRTIO_NET_DEV` | The component instance virtio_net_dev_2 |
 * | `arp_answerer_2` | `ARP_ANSWERER` | The component instance arp_answerer_2 |
 * | `mac_receiver_2` | `MAC_RECEIVER` | The component instance mac_receiver_2 |
 * | `udp_receiver` | `UDP_RECEIVER` | The component instance udp_receiver |
 * | `router` | `ROUTER` | The component instance router |
 * | `arinc_receiver_1` | `ARINC_RECEIVER` | The component instance arinc_receiver_1 |
 * | `arinc_receiver_2` | `ARINC_RECEIVER` | The component instance arinc_receiver_2 |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. While `1` is true:
 *    1. Call `__ARINC_SENDER_activity__()` with the argument
 *       `&arinc_sender_1`.
 *    2. Call `__UDP_IP_SENDER_activity__()` with the argument
 *       `&udp_ip_sender_1`.
 *    3. Call `__MAC_SENDER_activity__()` with the argument
 *       `&mac_sender_1`.
 *    4. Call `__VIRTIO_NET_DEV_activity__()` with the argument
 *       `&virtio_net_dev_1`.
 *    5. Call `__ARP_ANSWERER_activity__()` with the argument
 *       `&arp_answerer_1`.
 *    6. Call `__MAC_RECEIVER_activity__()` with the argument
 *       `&mac_receiver_1`.
 *    7. Call `__MAC_SENDER_activity__()` with the argument
 *       `&mac_sender_2`.
 *    8. Call `__VIRTIO_NET_DEV_activity__()` with the argument
 *       `&virtio_net_dev_2`.
 *    9. Call `__ARP_ANSWERER_activity__()` with the argument
 *       `&arp_answerer_2`.
 *    10. Call `__MAC_RECEIVER_activity__()` with the argument
 *       `&mac_receiver_2`.
 *    11. Call `__UDP_RECEIVER_activity__()` with the argument
 *       `&udp_receiver`.
 *    12. Call `__ROUTER_activity__()` with the argument
 *       `&router`.
 *    13. Call `__ARINC_RECEIVER_activity__()` with the argument
 *       `&arinc_receiver_1`.
 *    14. Call `__ARINC_RECEIVER_activity__()` with the argument
 *       `&arinc_receiver_2`.
 *
 * @calls
 * - `__ARINC_SENDER_activity__()`
 * - `__UDP_IP_SENDER_activity__()`
 * - `__MAC_SENDER_activity__()`
 * - `__VIRTIO_NET_DEV_activity__()`
 * - `__ARP_ANSWERER_activity__()`
 * - `__MAC_RECEIVER_activity__()`
 * - `__MAC_SENDER_activity__()`
 * - `__VIRTIO_NET_DEV_activity__()`
 * - `__ARP_ANSWERER_activity__()`
 * - `__MAC_RECEIVER_activity__()`
 * - `__UDP_RECEIVER_activity__()`
 * - `__ROUTER_activity__()`
 * - `__ARINC_RECEIVER_activity__()`
 * - `__ARINC_RECEIVER_activity__()`
 */
void __components_activity__()
{
    while (1) {
                __ARINC_SENDER_activity__(&arinc_sender_1);
                __UDP_IP_SENDER_activity__(&udp_ip_sender_1);
                __MAC_SENDER_activity__(&mac_sender_1);
                __VIRTIO_NET_DEV_activity__(&virtio_net_dev_1);
                __ARP_ANSWERER_activity__(&arp_answerer_1);
                __MAC_RECEIVER_activity__(&mac_receiver_1);
                __MAC_SENDER_activity__(&mac_sender_2);
                __VIRTIO_NET_DEV_activity__(&virtio_net_dev_2);
                __ARP_ANSWERER_activity__(&arp_answerer_2);
                __MAC_RECEIVER_activity__(&mac_receiver_2);
                __UDP_RECEIVER_activity__(&udp_receiver);
                __ROUTER_activity__(&router);
                __ARINC_RECEIVER_activity__(&arinc_receiver_1);
                __ARINC_RECEIVER_activity__(&arinc_receiver_2);
    }

}
