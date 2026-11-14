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
 * Modified by Kirill Brazhnikov, 2026-11-01: MISRA C:2025: pok_bool_t is
 * the C99 bool (TRUE and FALSE are true and false).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen headers for the file
 * and its types.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_queuing_discipline_e (was pok_queuing_discipline_t); the Doxygen comment
 * of the file is removed (header files are not documented).
 */


#ifndef JET_UAPI_TYPES_H
#define JET_UAPI_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @name Boolean values
 * Boolean type and values. pok_bool_t is the C99 bool, so that conditions using
 * it are essentially Boolean (MISRA C:2025 Rule 14.4).
 * @{
 */

/**
 * @macroname `FALSE`
 *
 * @description
 * False: the C99 false.
 */
#define FALSE  false

/**
 * @macroname `TRUE`
 *
 * @description
 * True: the C99 true.
 */
#define TRUE   true

/** @} */

/**
 * @typename `pok_bool_t`
 *
 * @description
 * A Boolean value (TRUE or FALSE). It is the C99 bool, so that conditions that
 * use it are essentially Boolean.
 */
typedef bool pok_bool_t;

/**
 * @typename `pok_port_size_t`
 *
 * @description
 * The size of a port: the maximum size of a message of a sampling or queuing
 * port, in bytes.
 */
typedef uint32_t pok_port_size_t;

/**
 * @typename `pok_port_direction_t`
 *
 * @description
 * The direction of a port: POK_PORT_DIRECTION_IN (the partition receives) or
 * POK_PORT_DIRECTION_OUT (it sends), see uapi/port_types.h.
 */
typedef uint8_t pok_port_direction_t;

/**
 * @typename `pok_port_kind_t`
 *
 * @description
 * The kind of a port (sampling or queuing). Not used.
 */
typedef uint8_t pok_port_kind_t;

/**
 * @typename `pok_port_id_t`
 *
 * @description
 * The identifier of a port of a partition (sampling or queuing).
 */
typedef uint8_t pok_port_id_t;

/**
 * @typename `pok_size_t`
 *
 * @description
 * A small size in bytes (at most 255), used by the encryption protocols of
 * libpok.
 */
typedef uint8_t pok_size_t;

/**
 * @typename `pok_range_t`
 *
 * @description
 * A number of messages (at most 255). Not used: the number of messages of a
 * queuing port or buffer is a pok_message_range_t.
 */
typedef uint8_t pok_range_t;

/**
 * @typename `pok_buffer_id_t`
 *
 * @description
 * The identifier of an ARINC 653 buffer of a partition.
 */
typedef uint8_t pok_buffer_id_t;

/**
 * @typename `pok_blackboard_id_t`
 *
 * @description
 * The identifier of an ARINC 653 blackboard of a partition.
 */
typedef uint8_t pok_blackboard_id_t;

/**
 * @typename `pok_sem_id_t`
 *
 * @description
 * The identifier of an ARINC 653 semaphore of a partition.
 */
typedef uint16_t pok_sem_id_t;

/**
 * @typename `pok_event_id_t`
 *
 * @description
 * The identifier of an ARINC 653 event of a partition.
 */
typedef uint16_t pok_event_id_t;

/**
 * @typename `pok_partition_id_t`
 *
 * @description
 * The identifier of a partition (its index in the configuration).
 */
typedef uint8_t pok_partition_id_t;

/**
 * @typename `pok_thread_id_t`
 *
 * @description
 * The identifier of a thread (ARINC 653 process) of a partition, or
 * JET_THREAD_ID_NONE.
 */
typedef uint8_t pok_thread_id_t;

/**
 * @typename `pok_sem_value_t`
 *
 * @description
 * The value (count) of a semaphore. Not used.
 */
typedef uint16_t pok_sem_value_t;

/**
 * @typename `pok_message_size_t`
 *
 * @description
 * The size of a message, in bytes (0 to 8192).
 */
typedef uint16_t pok_message_size_t;

/**
 * @typename `pok_message_range_t`
 *
 * @description
 * A number of messages: the capacity of a queuing port or buffer (0 to 512).
 */
typedef uint16_t pok_message_range_t;

/**
 * @typename `pok_time_t`
 *
 * @description
 * A time or a duration, in nanoseconds. A negative value (POK_TIME_INFINITY) is
 * infinite.
 */
typedef int64_t pok_time_t;

/**
 * @macroname `POK_TIME_INFINITY`
 *
 * @description
 * This constant should be used for *set* time as infinity(special).
 */
#define POK_TIME_INFINITY ((int64_t)(-1))

/**
 * @typename `pok_queuing_discipline_e`
 *
 * @description
 * The order in which the processes that wait on a port, buffer, semaphore or
 * other object are woken up.
 */
typedef enum {
    POK_QUEUING_DISCIPLINE_FIFO,     /**< In the order they started to wait. */
    POK_QUEUING_DISCIPLINE_PRIORITY, /**< By priority (FIFO among equal
                                          priorities). */
} pok_queuing_discipline_e;

/**
 * @macroname `MAX_NAME_LENGTH`
 *
 * @description
 * The maximum length of an ARINC 653 name (NAME_TYPE): a name has this many
 * characters, or fewer and a NUL.
 */
#define MAX_NAME_LENGTH 30

/**
 * @macroname `JET_THREAD_ID_NONE`
 *
 * @description
 * Special value of type pok_thread_id_t, which cannot be id of actual thread.
 */
#define JET_THREAD_ID_NONE (pok_thread_id_t)(-1)


#endif /* JET_UAPI_TYPES_H */
