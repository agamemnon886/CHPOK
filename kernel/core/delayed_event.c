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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
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
 * - \hlrid{HLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_INIT_01}:
 *   `delayed_event_queue_init()` shall make a queue of delayed events empty.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_005}
 *
 * @llr
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_INIT_01}:
 *   `delayed_event_queue_init()` shall set the first event of the queue to
 *   NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_INIT_01}
 *
 * @design
 * 1. Set `q->first_event` to `NULL`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_INIT_01}
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
 * - \hlrid{HLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_01}:
 *   `delayed_event_queue_check()` shall take out of a queue each event whose
 *   time is not later than the given time, in the order of the queue, and call
 *   its callback.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_008}
 *
 * @llr
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_01}: If `first_event` of
 *   `q` is NULL or its `timepoint` is greater than `time`,
 *   `delayed_event_queue_check()` shall return without calling a callback and
 *   leave the queue as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_01}
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_02}: If `timepoint` of
 *   the first event of `q` is not greater than `time`,
 *   `delayed_event_queue_check()` shall set `first_event` of `q` to
 *   `next_event` of that event, set its `pprev_event` to NULL, call its
 *   `process_event` once with its `handler_id`, and then go on with the new
 *   first event of `q`.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_01}
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_03}: If and only if
 *   `next_event` of that event is not NULL, `delayed_event_queue_check()` shall
 *   set `pprev_event` of the next event to the address of `first_event` of `q`,
 *   before the callback.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_01}
 *
 * @design
 * 1. Set `event` to `q->first_event`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_01},
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_02}
 * 2. While `event` is not NULL:
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_01},
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_02},
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_03}
 *    1. If `event->timepoint` is greater than `time`:
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_01}
 *       1. Leave the loop.
 *    2. Set `next_event` to `event->next_event`.
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_02}
 *    3. Set `q->first_event` to `next_event`.
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_02}
 *    4. If `next_event` is not NULL:
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_03}
 *       1. Set `next_event->pprev_event` to the address of `q->first_event`.
 *    5. Set `event->pprev_event` to `NULL`.
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_02}
 *    6. Call the function `event->process_event` with the argument
 *       `event->handler_id`.
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_02}
 *    7. Set `event` to `q->first_event`.
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_CHECK_02}
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
 * - \hlrid{HLR_DELAYED_EVENT_DELAYED_EVENT_INIT_01}: `delayed_event_init()`
 *   shall make a delayed event one that is in no queue.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_005}
 *
 * @llr
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_INIT_01}: `delayed_event_init()`
 *   shall set the link of the event to the previous one to NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_INIT_01}
 *
 * @design
 * 1. Set `event->pprev_event` to `NULL`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_INIT_01}
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
 * - \hlrid{HLR_DELAYED_EVENT_DELAYED_EVENT_ADD_01}: `delayed_event_add()` shall
 *   put an event into a queue for a time, with its callback and handler ID,
 *   after the events of the same or an earlier time.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_005}
 *   - \srqref{SRQ_JET_TIME_008}
 * - \hlrid{HLR_DELAYED_EVENT_DELAYED_EVENT_ADD_02}: `delayed_event_add()` shall
 *   first take an event that is already queued out of its queue.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_005}
 *
 * @llr
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_01}: If and only if
 *   `pprev_event` of `event` is not NULL, `delayed_event_add()` shall first
 *   take the event out of its queue: set the link at `pprev_event` to
 *   `next_event` of the event, and `pprev_event` to NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_ADD_02}
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_02}: If `pprev_event` of `event`
 *   and its `next_event` are not NULL, `delayed_event_add()` shall set
 *   `pprev_event` of that next event to `pprev_event` of `event` when it takes
 *   the event out.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_ADD_02}
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_03}: `delayed_event_add()` shall
 *   set `timepoint`, `handler_id` and `process_event` of `event` to the
 *   arguments of the same names.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_ADD_01}
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_04}: `delayed_event_add()` shall
 *   insert `event` into `q` before the first event whose `timepoint` is greater
 *   than `timepoint`, or at the end of `q` if there is none: set `next_event`
 *   of `event` to that event (or NULL), its `pprev_event` to the address of the
 *   link that pointed to that event (`first_event` of `q` or `next_event` of
 *   the event before), and that link to `event`.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_ADD_01}
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_05}: If and only if there is
 *   such a later event, `delayed_event_add()` shall set its `pprev_event` to
 *   the address of `next_event` of `event`.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_ADD_01}
 *
 * @design
 * 1. If `event->pprev_event` is not NULL:
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_01},
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_02}
 *    1. Set `next_event` to `event->next_event`.
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_01}
 *    2. Set `*(event->pprev_event)` to `next_event`.
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_01}
 *    3. If `next_event` is not NULL:
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_02}
 *       1. Set `next_event->pprev_event` to `event->pprev_event`.
 *    4. Set `event->pprev_event` to `NULL`.
 *       \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_01}
 * 2. Set `event->timepoint` to `timepoint`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_03}
 * 3. Set `event->handler_id` to `handler_id`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_03}
 * 4. Set `event->process_event` to `process_event`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_03}
 * 5. Set `insertion_point` to the address of `q->first_event`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_04}
 * 6. Set `next_event` to the value at `insertion_point`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_04}
 * 7. While `next_event` is not NULL and `next_event->timepoint` is less than or
 *    equal to `timepoint`: \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_04}
 *    1. Set `insertion_point` to the address of `next_event->next_event`.
 *    2. Set `next_event` to the value at `insertion_point`.
 * 8. Set `event->next_event` to `next_event`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_04}
 * 9. Set `event->pprev_event` to `insertion_point`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_04}
 * 10. Set `*insertion_point` to `event`.
 *     \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_04}
 * 11. If `next_event` is not NULL:
 *     \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_ADD_05}
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
 * - \hlrid{HLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_01}: `delayed_event_remove()`
 *   shall take an event out of its queue, and do nothing for an event that is
 *   in no queue.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_005}
 *
 * @llr
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_01}: If `pprev_event` of
 *   `event` is NULL, `delayed_event_remove()` shall return and leave the queue
 *   as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_01}
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_02}: If `pprev_event` of
 *   `event` is not NULL, `delayed_event_remove()` shall set the link at
 *   `pprev_event` to `next_event` of `event`, and then `pprev_event` of `event`
 *   to NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_01}
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_03}: If `pprev_event` of
 *   `event` and its `next_event` are not NULL, `delayed_event_remove()` shall
 *   set `pprev_event` of that next event to `pprev_event` of `event`.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_01}
 *
 * @design
 * 1. If `event->pprev_event` is NULL:
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_01}
 *    1. Return.
 * 2. Set `next_event` to `event->next_event`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_02}
 * 3. Set `*(event->pprev_event)` to `next_event`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_02}
 * 4. If `next_event` is not NULL:
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_03}
 *    1. Set `next_event->pprev_event` to `event->pprev_event`.
 * 5. Set `event->pprev_event` to `NULL`.
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_REMOVE_02}
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
 * - \hlrid{HLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_GET_CHECK_TIME_01}:
 *   `delayed_event_queue_get_check_time()` shall give the time at which a queue
 *   must next be checked: the time of its first event, or 0 if it is empty.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_008}
 *
 * @llr
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_GET_CHECK_TIME_01}: If
 *   `first_event` of `q` is not NULL, `delayed_event_queue_get_check_time()`
 *   shall return its `timepoint`.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_GET_CHECK_TIME_01}
 * - \llrid{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_GET_CHECK_TIME_02}: If
 *   `first_event` of `q` is NULL, `delayed_event_queue_get_check_time()` shall
 *   return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_GET_CHECK_TIME_01}
 *
 * @design
 * 1. If `q->first_event` is not NULL:
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_GET_CHECK_TIME_01}
 *    1. Return `q->first_event->timepoint`.
 * 2. Otherwise:
 *    \llrref{LLR_DELAYED_EVENT_DELAYED_EVENT_QUEUE_GET_CHECK_TIME_02}
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
