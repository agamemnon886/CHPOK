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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants;
 * port directions are unsigned constants of pok_port_direction_t instead
 * of an enum.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_queuing_discipline_e (was pok_queuing_discipline_t); the Doxygen comment
 * of the file is removed (header files are not documented).
 */

#ifndef JET_UAPI_PORT_TYPES_H
#define JET_UAPI_PORT_TYPES_H

#include <uapi/types.h>

/**
 * @name Port directions
 * Port directions: values of pok_port_direction_t.
 * @{
 */

/**
 * @macroname `POK_PORT_DIRECTION_IN`
 *
 * @description
 * The partition receives messages from the port (ARINC 653 DESTINATION).
 */
#define POK_PORT_DIRECTION_IN   1U

/**
 * @macroname `POK_PORT_DIRECTION_OUT`
 *
 * @description
 * The partition sends messages to the port (ARINC 653 SOURCE).
 */
#define POK_PORT_DIRECTION_OUT  2U

/** @} */
/**
 * @typename `pok_port_directions_t`
 *
 * @description
 * The direction of a port: another name of pok_port_direction_t.
 */
typedef pok_port_direction_t pok_port_directions_t;

/**
 * @typename `pok_port_queuing_status_t`
 *
 * @description
 * Status for queuing port, for return into user space.
 */
typedef struct
{
   pok_port_size_t      nb_message; /**< The number of messages in the port. */
   pok_port_size_t      max_nb_message; /**< The maximum number of messages. */
   pok_port_size_t      max_message_size; /**< The maximum size of a message. */
   pok_port_direction_t direction; /**< The direction. */
   /**
    * The number of processes that wait on the port.
    */
   uint8_t              waiting_processes;
} pok_port_queuing_status_t;

/**
 * @typename `pok_port_queuing_create_arg_t`
 *
 * @description
 * Packaging for arguments passed to syscall
 * POK_SYSCALL_MIDDLEWARE_QUEUEING_CREATE.
 */
typedef struct
{
    pok_port_size_t message_size; /**< The maximum size of a message. */
    pok_port_size_t max_nb_message; /**< The maximum number of messages. */
    pok_port_direction_t direction; /**< The direction. */
    /**
     * The queuing discipline of the processes that wait on the port.
     */
    pok_queuing_discipline_e discipline;
} pok_port_queuing_create_arg_t;

/**
 * @typename `pok_port_sampling_status_t`
 *
 * @description
 * Status for sampling port, for return into user space.
 */
typedef struct
{
   pok_port_size_t      size; /**< The maximum size of a message. */
   pok_port_direction_t direction; /**< The direction. */
   uint64_t             refresh; /**< The refresh period. */
   /**
    * Whether the last message read is valid (younger than the refresh period).
    */
   pok_bool_t           validity;
}pok_port_sampling_status_t;

#endif /* JET_UAPI_PORT_TYPES_H */
