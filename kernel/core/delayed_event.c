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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the insertion
 * loop has a single loop counter; increments and assignments are separate
 * statements; the unused queue parameter of delayed_event_remove() is
 * cast to void.
 */

#include <core/delayed_event.h>
#include <libc.h>

void delayed_event_queue_init(struct delayed_event_queue* q)
{
    q->first_event = NULL;
}

void delayed_event_queue_check(struct delayed_event_queue* q,
    pok_time_t time)
{
    struct delayed_event* event = q->first_event;
    while(event != NULL)
    {
        if(event->timepoint > time) {
            break;
        }

        struct delayed_event* next_event = event->next_event;

        q->first_event = next_event;
        if(next_event != NULL) {
            next_event->pprev_event = &q->first_event;
        }

        event->pprev_event = NULL;

        event->process_event(event->handler_id);

        event = q->first_event;
    }
}

void delayed_event_init(struct delayed_event* event)
{
    event->pprev_event = NULL;
}

void delayed_event_add(struct delayed_event_queue* q,
    struct delayed_event* event, pok_time_t timepoint,
    uint16_t handler_id, process_event_t process_event)
{
    struct delayed_event* next_event;

    /*
     * Remove event from queue, if it was.
     */
    if(event->pprev_event != NULL)
    {
        next_event = event->next_event;

        *(event->pprev_event) = next_event;
        if(next_event != NULL) {
            next_event->pprev_event = event->pprev_event;
        }

        event->pprev_event = NULL;
    }

    event->timepoint = timepoint;
    event->handler_id = handler_id;
    event->process_event = process_event;


    // Insert before the first event which is later than this one.
    struct delayed_event** insertion_point = &q->first_event;
    next_event = *insertion_point;
    while((next_event != NULL) && (next_event->timepoint <= timepoint))
    {
        insertion_point = &next_event->next_event;
        next_event = *insertion_point;
    }

    event->next_event = next_event;
    event->pprev_event = insertion_point;

    *insertion_point = event;
    if(next_event != NULL) {
        next_event->pprev_event = &event->next_event;
    }

}

void delayed_event_remove(struct delayed_event_queue* q,
    struct delayed_event* event)
{
    (void) q; // The event knows its place in the queue.

    if(event->pprev_event == NULL) {
        return; // Event is not in the queue.
    }

    struct delayed_event* next_event = event->next_event;

    *(event->pprev_event) = next_event;
    if(next_event != NULL) {
        next_event->pprev_event = event->pprev_event;
    }

    event->pprev_event = NULL;
}

pok_time_t delayed_event_queue_get_check_time(struct delayed_event_queue* q)
{
    if(q->first_event != NULL) {
        return q->first_event->timepoint;
    }
    else {
        return 0;
    }
}
