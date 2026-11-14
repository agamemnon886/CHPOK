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
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the types are typedefs without a
 * tag, used by their typedef names, and are documented with their macros.
 */

#ifndef STATE_STRUCTS_H
#define STATE_STRUCTS_H

/**
 * @typename `udp_ip_pair_t`
 *
 * @description
 * A destination of the ROUTER component: an IPv4 address and a UDP port.
 */
typedef struct {
    uint32_t ip; /**< The IPv4 address. */
    uint16_t port; /**< The UDP port. */
} udp_ip_pair_t;

#endif
