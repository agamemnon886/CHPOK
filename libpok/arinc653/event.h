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

#ifndef LIBJET_ARINC_EVENT_H
#define LIBJET_ARINC_EVENT_H

#include <config.h>

#ifdef POK_NEEDS_ARINC653_EVENT
#include <arinc653/types.h>
#include <arinc653/event.h>
#include <msection.h>
#include <types.h>

/**
 * @typename `arinc_event_t`
 *
 * @description
 * An event of the partition (CREATE_EVENT()), in the preallocated array
 * arinc_events.
 */
typedef struct {
    EVENT_NAME_TYPE event_name; /**< The name of the event. */
    
    EVENT_STATE_TYPE event_state; /**< The state of the event (UP or DOWN). */
    
    /**
     * Protects the event (a critical section of the partition's processes).
     */
    msection_t section;
    msection_wq_t process_queue; /**< The processes waiting for the event. */
} arinc_event_t;

/* Preallocated array of buffers. */
extern arinc_event_t* arinc_events;

#endif /* POK_NEEDS_ARINC653_EVENT */

#endif /* LIBJET_ARINC_EVENT_H */
