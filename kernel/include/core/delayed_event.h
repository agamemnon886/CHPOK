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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen headers for its types
 * and comments for their members; the function declarations are hidden from
 * Doxygen (\cond), which would otherwise document the functions a second
 * time, in the section of this header.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: struct delayed_event and struct
 * delayed_event_queue are typedefs, delayed_event_t and delayed_event_queue_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 */

#ifndef POK_DELAYED_EVENT_H
#define POK_DELAYED_EVENT_H

/*
 * Delayed events - events which should fire at specific time point
 */

#include <list.h>
#include <types.h>

/**
 * @typename `process_event_t`
 *
 * @description
 * The function that processes a delayed event when its time has come
 * (delayed_event_queue_check()). Its parameter, handler_id, is the
 * handler_id of the event; it returns nothing.
 */
typedef void (*process_event_t)(uint16_t handler_id);

/**
 * @typename `delayed_event_t`
 *
 * @description
 * Event which should occure at a specific time point. It is in a queue of
 * delayed events (delayed_event_queue_t) while it waits for its time.
 */
typedef struct delayed_event {
	struct delayed_event* next_event; /**< The next event in the queue. */
	/** NULL if event is not in the event queue (else the address of the
	 * pointer to this event: the previous event's next_event or the
	 * queue's first_event). */
	struct delayed_event** pprev_event;
	pok_time_t timepoint; /**< The time when the event happens. */
	uint16_t handler_id; /**< The argument of process_event. */
	/** The function called when the time has come. */
	process_event_t process_event;
} delayed_event_t;

/*
 * Tells whether an event is in a queue (waits for its time).
 */
static inline pok_bool_t delayed_event_is_active(delayed_event_t* event)
{
	return event->pprev_event != NULL;
}

/**
 * @typename `delayed_event_queue_t`
 *
 * @description
 * Queue of delayed events.
 */
typedef struct {
	/** The first event (NULL if the queue is empty). Events are ordered by
	 * timeout. */
	delayed_event_t* first_event;
} delayed_event_queue_t;

/* Initialize delayed events queue. */
/** \cond */
void delayed_event_queue_init(delayed_event_queue_t* q);

/*
 * Emit all events which have time expired.
 * 
 * For each such event @process_event is called, and event is removed
 * from the queue.
 */
void delayed_event_queue_check(delayed_event_queue_t* q, pok_time_t time);

 /*
  *  Return time when queue should be checked.
  * 
  * NOTE: This time needn't cause any delayed event to be occured.
  */
pok_time_t delayed_event_queue_get_check_time(delayed_event_queue_t* q);

/*
 *  Initialize delayed event.
 * 
 * Initially event doesn't belong to any queue.
 */
void delayed_event_init(delayed_event_t* event);

/* 
 * Add delayed event to the queue.
 * 
 * Initially event should either be not added to any queue,
 * or should be added to queue `q`.
 * 
 * In the last case event will (possibly) be reordered in the queue.
 */
void delayed_event_add(delayed_event_queue_t* q,
	delayed_event_t* event, pok_time_t timepoint,
	uint16_t handler_id, process_event_t process_event);

/* 
 * If element belongs to the queue, delete it.
 */
void delayed_event_remove(delayed_event_queue_t* q,
	delayed_event_t* event);
/** \endcond */

#endif /* ! POK_DELAYED_EVENT_H*/
