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

#ifndef LIBJET_ARINC_EVENT_H
#define LIBJET_ARINC_EVENT_H

#include <config.h>

#ifdef POK_NEEDS_ARINC653_EVENT
#include <arinc653/types.h>
#include <arinc653/event.h>
#include <msection.h>
#include <types.h>

struct arinc_event
{
    EVENT_NAME_TYPE event_name;
    
    EVENT_STATE_TYPE event_state;
    
    struct msection section;
    struct msection_wq process_queue;
};

/* Preallocated array of buffers. */
extern struct arinc_event* arinc_events;

#endif /* POK_NEEDS_ARINC653_EVENT */

#endif /* LIBJET_ARINC_EVENT_H */
