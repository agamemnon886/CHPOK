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
 */

#ifndef LIBJET_ARINC_BLACKBOARD_H
#define LIBJET_ARINC_BLACKBOARD_H

#include <config.h>

#ifdef POK_NEEDS_ARINC653_BLACKBOARD
#include <arinc653/types.h>
#include <arinc653/blackboard.h>
#include <msection.h>
#include <types.h>

struct arinc_blackboard
{
    BLACKBOARD_NAME_TYPE blackboard_name;
    
    MESSAGE_SIZE_TYPE max_message_size;
    
    char* message;
    MESSAGE_SIZE_TYPE message_size; // 0 means absent of the message.
    
    struct msection section;
    struct msection_wq process_queue;
};

/* Preallocated array of blackboards. */
extern struct arinc_blackboard* arinc_blackboards;

#endif /* POK_NEEDS_ARINC653_BLACKBOARD */

#endif /* LIBJET_ARINC_BLACKBOARD_H */
