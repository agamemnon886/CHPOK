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
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: msection_t, msection_wq_t.
 */

#ifndef LIBJET_ARINC_BLACKBOARD_H
#define LIBJET_ARINC_BLACKBOARD_H

#include <config.h>

#ifdef POK_NEEDS_ARINC653_BLACKBOARD
#include <arinc653/types.h>
#include <arinc653/blackboard.h>
#include <msection.h>
#include <types.h>

/**
 * @typename `arinc_blackboard_t`
 *
 * @description
 * A blackboard of the partition (CREATE_BLACKBOARD()), in the preallocated
 * array arinc_blackboards.
 */
typedef struct {
    BLACKBOARD_NAME_TYPE blackboard_name; /**< The name of the blackboard. */
    
    MESSAGE_SIZE_TYPE max_message_size; /**< The maximum size of a message. */
    
    char* message; /**< The message displayed (max_message_size bytes). */
    MESSAGE_SIZE_TYPE message_size; /**< 0 means absent of the message. */
    
    /**
     * Protects the blackboard (a critical section of the partition's
     * processes).
     */
    msection_t section;
    msection_wq_t process_queue; /**< The processes waiting for a message. */
} arinc_blackboard_t;

/* Preallocated array of blackboards. */
extern arinc_blackboard_t* arinc_blackboards;

#endif /* POK_NEEDS_ARINC653_BLACKBOARD */

#endif /* LIBJET_ARINC_BLACKBOARD_H */
