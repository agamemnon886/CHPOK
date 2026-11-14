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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the
 * pok_port_queuing_create() macro, a GNU statement expression, is replaced
 * with an inline function.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_queuing_discipline_e (was pok_queuing_discipline_t), pok_ret_e (was
 * pok_ret_t); the Doxygen comment of the file is removed (header files are not
 * documented); the Doxygen headers of the static inline functions are plain
 * comments (they are not documented).
 */

#ifndef POK_LIBPOK_PORTS_H
#define POK_LIBPOK_PORTS_H

#include <config.h>

#include <core/dependencies.h>

#include <uapi/port_types.h>
#include <core/syscall.h>

#ifdef POK_NEEDS_PORTS_QUEUEING
/*
 * Creates a queuing port: the syscall, with the parameters packed into a
 * structure.
 */
static inline pok_ret_e pok_port_queuing_create(const char* name,
    pok_port_size_t message_size,
    pok_port_size_t max_nb_message,
    pok_port_direction_t direction,
    pok_queuing_discipline_e discipline,
    pok_port_id_t* id)
{
    pok_port_queuing_create_arg_t args;

    args.message_size = message_size;
    args.max_nb_message = max_nb_message;
    args.direction = direction;
    args.discipline = discipline;

    return pok_port_queuing_create_packed(name, &args, id);
}
#endif /* POK_NEEDS_PORTS_QUEUEING */

#endif
