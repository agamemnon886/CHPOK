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

#ifndef POK_SYSPART_PORTS_H
#define POK_SYSPART_PORTS_H

#include <middleware/port.h>
#include <arinc653/types.h>

typedef struct
{
    NAME_TYPE                   name;
    PORT_DIRECTION_TYPE         direction;
    size_t                      overhead;
} sys_port_header_t;

typedef struct
{
    MESSAGE_SIZE_TYPE           message_size;
    /* MISRA C:2025 Rule 18.7 deviation: the message follows its size. */
    char                        data[];
} sys_port_data_t;

typedef struct
{
    sys_port_header_t           header;

    pok_port_size_t             max_message_size;
    pok_port_size_t             max_nb_messages;

    sys_port_data_t             *data;
    APEX_INTEGER                id;

} sys_queuing_port_t;

typedef struct
{
    sys_port_header_t           header;

    pok_port_size_t             max_message_size;

    sys_port_data_t             *data;
    APEX_INTEGER                id;
} sys_sampling_port_t;


#endif
