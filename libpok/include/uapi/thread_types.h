/*
 * COPIED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify original one (kernel/include/uapi/thread_types.h).
 */
/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2014, 2016 ISPRAS
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
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros.
 */

#ifndef JET_UAPI_THREAD_TYPES_H
#define JET_UAPI_THREAD_TYPES_H

#include <uapi/types.h>

/**
 * @typename `pok_state_e`
 *
 * @description
 * The state of a thread; comments describe to what states of ARINC653 these
 * correspond to.
 */
typedef enum
{
  POK_STATE_STOPPED = 0, /**< DORMANT (must be started first). */
  /**
   * READY or RUNNING. When return status to user space it means READY.
   */
  POK_STATE_RUNNABLE = 1,
  /**
   * WAITING for any event except resuming (but for timed suspend exactly this
   * state is used).
   */
  POK_STATE_WAITING = 2,
  /**
   * RUNNING. Used only for return status to user space.
   */
  POK_STATE_RUNNING = 3,
} pok_state_e;

/**
 * @typename `pok_deadline_e`
 *
 * @description
 * The deadline type of a process (ARINC 653 DEADLINE_TYPE).
 */
typedef enum {
    DEADLINE_SOFT, /**< A soft deadline. */
    DEADLINE_HARD /**< A hard deadline. */
} pok_deadline_e;

/**
 * @typename `pok_thread_attr_t`
 *
 * @description
 * The attributes of a thread (ARINC 653 PROCESS_ATTRIBUTE_TYPE, as the kernel
 * takes them).
 */
typedef struct
{
    uint8_t      	priority; /**< The base priority. */
    /**
     * The period (POK_TIME_INFINITY: aperiodic).
     */
    pok_time_t	   	period;
    pok_deadline_e	deadline; /**< The deadline type. */
    /**
     * The time capacity (POK_TIME_INFINITY: none).
     */
    pok_time_t   	time_capacity;
    uint32_t     	stack_size; /**< The size of the stack, in bytes. */
} pok_thread_attr_t;

/**
 * @typename `pok_thread_status_t`
 *
 * @description
 * The status of a thread (ARINC 653 PROCESS_STATUS_TYPE).
 */
typedef struct 
{
    pok_thread_attr_t   attributes; /**< The attributes. */
    pok_time_t          deadline_time; /**< The deadline time. */
    pok_state_e         state; /**< The state. */
    uint8_t             current_priority; /**< The current priority. */
} pok_thread_status_t;


#endif /* JET_UAPI_THREAD_TYPES_H */
