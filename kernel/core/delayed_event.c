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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: struct delayed_event and struct
 * delayed_event_queue are typedefs, delayed_event_t and delayed_event_queue_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * Delayed events: a queue of events sorted by their time, each processed
 * by its callback when the time comes.
 */

#include <core/delayed_event.h>
#include <libc.h>

/**
 * @funcname `delayed_event_queue_init`
 *
 * @description
 * Initializes a queue of delayed events, empty.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `q` | `delayed_event_queue_t *` | [out] The queue. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `q->first_event` to `NULL`.
 *
 * @calls
 * None.
 */
void delayed_event_queue_init(delayed_event_queue_t* q)
{
    q->first_event = NULL;
}

/**
 * @funcname `delayed_event_queue_check`
 *
 * @description
 * Processes the events of a queue whose time has come: each one is removed from
 * the queue and its callback called.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `q` | `delayed_event_queue_t *` | [in,out] The queue. |
 * | `time` | `pok_time_t` | [in] The current time. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `delayed_event_t *` | The first event of the queue |
 * | `next_event` | `delayed_event_t *` | The event after it |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `event` to `q->first_event`.
 * 2. While `event` is not NULL:
 *    1. If `event->timepoint` is greater than `time`:
 *       1. Leave the loop.
 *    2. Set `next_event` to `event->next_event`.
 *    3. Set `q->first_event` to `next_event`.
 *    4. If `next_event` is not NULL:
 *       1. Set `next_event->pprev_event` to the address of `q->first_event`.
 *    5. Set `event->pprev_event` to `NULL`.
 *    6. Call the function `event->process_event` with the argument
 *       `event->handler_id`.
 *    7. Set `event` to `q->first_event`.
 *
 * @calls
 * - `event->process_event()` (indirect)
 */
void delayed_event_queue_check(delayed_event_queue_t* q,
    pok_time_t time)
{
    delayed_event_t* event = q->first_event;
    while(event != NULL)
    {
        if(event->timepoint > time) {
            break;
        }

        delayed_event_t* next_event = event->next_event;

        q->first_event = next_event;
        if(next_event != NULL) {
            next_event->pprev_event = &q->first_event;
        }

        event->pprev_event = NULL;

        event->process_event(event->handler_id);

        event = q->first_event;
    }
}

/**
 * @funcname `delayed_event_init`
 *
 * @description
 * Initializes a delayed event, not in any queue.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `delayed_event_t *` | [out] The event. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `event->pprev_event` to `NULL`.
 *
 * @calls
 * None.
 */
void delayed_event_init(delayed_event_t* event)
{
    event->pprev_event = NULL;
}

/**
 * @funcname `delayed_event_add`
 *
 * @description
 * Puts an event into a queue for a time (moving it if it was queued already),
 * after the events of the same or an earlier time.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `q` | `delayed_event_queue_t *` | [in,out] The queue. |
 * | `event` | `delayed_event_t *` | [in,out] The event. |
 * | `timepoint` | `pok_time_t` | [in] The time of the event. |
 * | `handler_id` | `uint16_t` | [in] The ID passed to the callback. |
 * | `process_event` | `process_event_t` | [in] The callback. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `insertion_point` | `delayed_event_t **` | The link where the event is inserted |
 * | `next_event` | `delayed_event_t *` | The event after the insertion point |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `event->pprev_event` is not NULL:
 *    1. Set `next_event` to `event->next_event`.
 *    2. Set `*(event->pprev_event)` to `next_event`.
 *    3. If `next_event` is not NULL:
 *       1. Set `next_event->pprev_event` to `event->pprev_event`.
 *    4. Set `event->pprev_event` to `NULL`.
 * 2. Set `event->timepoint` to `timepoint`.
 * 3. Set `event->handler_id` to `handler_id`.
 * 4. Set `event->process_event` to `process_event`.
 * 5. Set `insertion_point` to the address of `q->first_event`.
 * 6. Set `next_event` to the value at `insertion_point`.
 * 7. While `next_event` is not NULL and `next_event->timepoint` is less than or
 *    equal to `timepoint`:
 *    1. Set `insertion_point` to the address of `next_event->next_event`.
 *    2. Set `next_event` to the value at `insertion_point`.
 * 8. Set `event->next_event` to `next_event`.
 * 9. Set `event->pprev_event` to `insertion_point`.
 * 10. Set `*insertion_point` to `event`.
 * 11. If `next_event` is not NULL:
 *     1. Set `next_event->pprev_event` to the address of `event->next_event`.
 *
 * @calls
 * None.
 */
void delayed_event_add(delayed_event_queue_t* q,
    delayed_event_t* event, pok_time_t timepoint,
    uint16_t handler_id, process_event_t process_event)
{
    delayed_event_t* next_event;

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
    delayed_event_t** insertion_point = &q->first_event;
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

/**
 * @funcname `delayed_event_remove`
 *
 * @description
 * Removes an event from its queue, if it is queued.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `q` | `delayed_event_queue_t *` | [in] The queue (not used: the event knows its place). |
 * | `event` | `delayed_event_t *` | [in,out] The event. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `next_event` | `delayed_event_t *` | The event after it |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Mark `q` as not used.
 * 2. If `event->pprev_event` is NULL:
 *    1. Return.
 * 3. Set `next_event` to `event->next_event`.
 * 4. Set `*(event->pprev_event)` to `next_event`.
 * 5. If `next_event` is not NULL:
 *    1. Set `next_event->pprev_event` to `event->pprev_event`.
 * 6. Set `event->pprev_event` to `NULL`.
 *
 * @calls
 * None.
 */
void delayed_event_remove(delayed_event_queue_t* q,
    delayed_event_t* event)
{
    (void) q; // The event knows its place in the queue.

    if(event->pprev_event == NULL) {
        return; // Event is not in the queue.
    }

    delayed_event_t* next_event = event->next_event;

    *(event->pprev_event) = next_event;
    if(next_event != NULL) {
        next_event->pprev_event = event->pprev_event;
    }

    event->pprev_event = NULL;
}

/**
 * @funcname `delayed_event_queue_get_check_time`
 *
 * @description
 * Returns the time of the first event of a queue: when the queue must be
 * checked next.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `q` | `delayed_event_queue_t *` | [in] The queue. |
 *
 * @return The time of the first event, or 0 if the queue is empty.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `q->first_event` is not NULL:
 *    1. Return `q->first_event->timepoint`.
 * 2. Otherwise:
 *    1. Return `0`.
 *
 * @calls
 * None.
 */
pok_time_t delayed_event_queue_get_check_time(delayed_event_queue_t* q)
{
    if(q->first_event != NULL) {
        return q->first_event->timepoint;
    }
    else {
        return 0;
    }
}
