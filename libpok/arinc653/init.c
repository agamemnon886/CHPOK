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
 * Modified by Kirill Brazhnikov, 2026-11-01: allocates and initializes the
 * ARINC 653 mutexes.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * Initialization of the ARINC 653 intra-partition objects: their arrays
 * and the message memory, allocated at partition start.
 */

#include <init_arinc.h>
#include "arinc_alloc.h"
#include <arinc_config.h>

#include <smalloc.h>

#include "buffer.h"
#include "blackboard.h"
#include "event.h"
#include "semaphore.h"
#include "mutex.h"

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the allocator returns its
 * memory as a pointer to void, which is converted to the type of the object
 * allocated.
 */

#ifdef POK_NEEDS_ARINC653_BUFFER
arinc_buffer_t* arinc_buffers;
#endif /* POK_NEEDS_ARINC653_BUFFER */

#ifdef POK_NEEDS_ARINC653_BLACKBOARD
arinc_blackboard_t* arinc_blackboards;
#endif /* POK_NEEDS_ARINC653_BLACKBOARD */

#ifdef POK_NEEDS_ARINC653_EVENT
arinc_event_t* arinc_events;
#endif /* POK_NEEDS_ARINC653_EVENT */

#ifdef POK_NEEDS_ARINC653_SEMAPHORE
arinc_semaphore_t* arinc_semaphores;
#endif /* POK_NEEDS_ARINC653_SEMAPHORE */

#ifdef POK_NEEDS_ARINC653_MUTEX
arinc_mutex_t* arinc_mutexes;
#endif /* POK_NEEDS_ARINC653_MUTEX */


#if defined(POK_NEEDS_ARINC653_BUFFER) || defined(POK_NEEDS_ARINC653_BLACKBOARD)
char* arinc_intra_heap = NULL;
#endif /* defined(POK_NEEDS_ARINC653_BUFFER) || defined(POK_NEEDS_ARINC653_BLACKBOARD) */

/**
 * @funcname `libjet_arinc_init`
 *
 * @description
 * Allocates the arrays of buffers, blackboards, semaphores, events and mutexes
 * and the message memory, of the configured sizes (at partition start).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_blackboards` | `arinc_blackboard_t *` | The blackboards of the partition |
 * | `arinc_buffers` | `arinc_buffer_t *` | The buffers of the partition |
 * | `arinc_config_messages_memory_size` | `size_t` | The size of the memory for messages (configuration) |
 * | `arinc_config_nblackboards` | `size_t` | The maximum number of blackboards (configuration) |
 * | `arinc_config_nbuffers` | `size_t` | The maximum number of buffers (configuration) |
 * | `arinc_config_nevents` | `size_t` | The maximum number of events (configuration) |
 * | `arinc_config_nmutexes` | `size_t` | The maximum number of mutexes (configuration) |
 * | `arinc_config_nsemaphores` | `size_t` | The maximum number of semaphores (configuration) |
 * | `arinc_events` | `arinc_event_t *` | The events of the partition |
 * | `arinc_intra_heap` | `char *` | The memory for the messages of buffers and blackboards |
 * | `arinc_mutexes` | `arinc_mutex_t *` | The mutexes of the partition |
 * | `arinc_semaphores` | `arinc_semaphore_t *` | The semaphores of the partition |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `smalloc()` with the argument
 *    `arinc_config_nbuffers * sizeof(*arinc_buffers)`, and store the result in
 *    `arinc_buffers`.
 * 2. Call `smalloc()` with the argument
 *    `arinc_config_nblackboards * sizeof(*arinc_blackboards)`, and store the
 *    result in `arinc_blackboards`.
 * 3. Call `smalloc()` with the argument
 *    `arinc_config_nsemaphores * sizeof(*arinc_semaphores)`, and store the
 *    result in `arinc_semaphores`.
 * 4. Call `smalloc()` with the argument
 *    `arinc_config_nevents * sizeof(*arinc_events)`, and store the result in
 *    `arinc_events`.
 * 5. Call `smalloc()` with the argument
 *    `arinc_config_nmutexes * sizeof(*arinc_mutexes)`, and store the result in
 *    `arinc_mutexes`.
 * 6. Call `arinc_mutex_init()`.
 * 7. Call `smalloc()` with the argument `arinc_config_messages_memory_size`,
 *    and store the result in `arinc_intra_heap`.
 *
 * @calls
 * - `smalloc()`
 * - `arinc_mutex_init()`
 */
void libjet_arinc_init(void)
{
#ifdef POK_NEEDS_ARINC653_BUFFER
    arinc_buffers = smalloc(arinc_config_nbuffers * sizeof(*arinc_buffers));
#endif /* POK_NEEDS_ARINC653_BUFFER */

#ifdef POK_NEEDS_ARINC653_BLACKBOARD
    arinc_blackboards = smalloc(arinc_config_nblackboards * sizeof(*arinc_blackboards));
#endif /* POK_NEEDS_ARINC653_BLACKBOARD */

#ifdef POK_NEEDS_ARINC653_SEMAPHORE
    arinc_semaphores = smalloc(arinc_config_nsemaphores * sizeof(*arinc_semaphores));
#endif /* POK_NEEDS_ARINC653_SEMAPHORE */

#ifdef POK_NEEDS_ARINC653_EVENT
    arinc_events = smalloc(arinc_config_nevents * sizeof(*arinc_events));
#endif /* POK_NEEDS_ARINC653_EVENT */

#ifdef POK_NEEDS_ARINC653_MUTEX
    arinc_mutexes = smalloc(arinc_config_nmutexes * sizeof(*arinc_mutexes));
    arinc_mutex_init();
#endif /* POK_NEEDS_ARINC653_MUTEX */


#if defined(POK_NEEDS_ARINC653_BUFFER) || defined(POK_NEEDS_ARINC653_BLACKBOARD)
    arinc_intra_heap = smalloc(arinc_config_messages_memory_size);
#endif /* defined(POK_NEEDS_ARINC653_BUFFER) || defined(POK_NEEDS_ARINC653_BLACKBOARD) */
}
