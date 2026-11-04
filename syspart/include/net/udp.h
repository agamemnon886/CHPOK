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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; deviations are documented where a
 * guideline can't be followed.
 */

#ifndef POK_NET_UDP_H
#define POK_NET_UDP_H

#include <types.h>

struct udp_hdr {
    uint16_t src_port, dst_port;
    uint16_t length;
    uint16_t checksum;

    /* MISRA C:2025 Rule 18.7 deviation: the UDP payload follows the header. */
    char payload[];
} __attribute__((packed));

#endif
