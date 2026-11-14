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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; operands of the
 * same essential type; the name is copied with strncpy(), so a shorter
 * name isn't read past its end.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized; deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: name
 * parameters are const (they are only read), so callers can pass string
 * literals.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * ARINC 653 events (intra-partition communication): CREATE_EVENT,
 * SET_EVENT, RESET_EVENT, WAIT_EVENT, GET_EVENT_ID and GET_EVENT_STATUS.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_EVENT

#include "event.h"

#include <arinc653/types.h>
#include <arinc653/event.h>
#include <types.h>
#include <utils.h>

#include <kernel_shared_data.h>
#include <core/assert_os.h>

#include <string.h>
#include <arinc_config.h>
#include "arinc_process_queue.h"

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): thread and object IDs are
 * the index of an element, computed as the difference between its address
 * and the start of its array.
 */

static size_t nevents_used = 0;

/**
 * @funcname `find_event`
 *
 * @description
 * Finds an event by name (case insensitive).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char *` | [in] The name. |
 *
 * @return The event, or NULL if there is none.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_events` | `arinc_event_t *` | The events of the partition |
 * | `nevents_used` | `size_t` | The number of events created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `arinc_event_t *` | The event |
 * | `i` | `size_t` | The index of an event |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `nevents_used`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `event` to the address of `arinc_events[i]`.
 *    2. If `strncasecmp(event->event_name, name, MAX_NAME_LENGTH)` is equal to
 *       `0`:
 *       1. Return `event`.
 * 2. Return `NULL`.
 *
 * @calls
 * - `strncasecmp()`
 */
static arinc_event_t* find_event(const char* name)
{
   for(size_t i = 0U; i < nevents_used; i++)
   {
      arinc_event_t* event = &arinc_events[i];
      if(strncasecmp(event->event_name, name, MAX_NAME_LENGTH) == 0) {
         return event;
      }
   }

   return NULL;
}


/**
 * @funcname `CREATE_EVENT`
 *
 * @description
 * Creates an event, down (ARINC 653 service), not in NORMAL mode.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `EVENT_NAME` | `const EVENT_NAME_TYPE` | [in] The name. |
 * | `EVENT_ID` | `EVENT_ID_TYPE *` | [out] Where its ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_MODE, NO_ACTION (the name exists) or INVALID_CONFIG (too many). |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_config_nevents` | `size_t` | The maximum number of events (configuration) |
 * | `arinc_events` | `arinc_event_t *` | The events of the partition |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nevents_used` | `size_t` | The number of events created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `arinc_event_t *` | The new event |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `kshd.partition_mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 *    2. Return.
 * 2. If `find_event(EVENT_NAME)` is not NULL:
 *    1. Set `*RETURN_CODE` to `NO_ACTION`.
 *    2. Return.
 * 3. If `nevents_used` is equal to `arinc_config_nevents`:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 4. Set `event` to the address of `arinc_events[nevents_used]`.
 * 5. Call `strncpy()` with the arguments `event->event_name`, `EVENT_NAME` and
 *    `MAX_NAME_LENGTH`.
 * 6. Call `msection_init()` with the address of `event->section`.
 * 7. Call `msection_wq_init()` with the address of `event->process_queue`.
 * 8. Set `event->event_state` to `DOWN`.
 * 9. Set `*EVENT_ID` to `nevents_used + 1U`.
 * 10. Increment `nevents_used`.
 * 11. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_event()`
 * - `strncpy()`
 * - `msection_init()`
 * - `msection_wq_init()`
 */
void CREATE_EVENT (const EVENT_NAME_TYPE EVENT_NAME,
                   EVENT_ID_TYPE *EVENT_ID,
                   RETURN_CODE_TYPE *RETURN_CODE)
{
   if(kshd.partition_mode == POK_PARTITION_MODE_NORMAL) {
      // Cannot create buffer in NORMAL mode
      *RETURN_CODE = INVALID_MODE;
      return;
   }

   if(find_event(EVENT_NAME) != NULL) {
      // Event with given name already exists.
      *RETURN_CODE = NO_ACTION;
      return;
   }

   if(nevents_used == arinc_config_nevents) {
      // Too many events
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   arinc_event_t* event = &arinc_events[nevents_used];

   // The name may be shorter than MAX_NAME_LENGTH: don't read past its end.
   (void) strncpy(event->event_name, EVENT_NAME, MAX_NAME_LENGTH);
   msection_init(&event->section);
   msection_wq_init(&event->process_queue);
   event->event_state = DOWN;

   *EVENT_ID = nevents_used + 1U;// Avoid 0 value.

   nevents_used++;

   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `SET_EVENT`
 *
 * @description
 * Sets an event up (ARINC 653 service): every process waiting on it is
 * released.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `EVENT_ID` | `EVENT_ID_TYPE` | [in] The event. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_events` | `arinc_event_t *` | The events of the partition |
 * | `nevents_used` | `size_t` | The number of events created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `arinc_event_t *` | The event |
 * | `t` | `pok_thread_id_t` | A waiting process |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `EVENT_ID` is less than or equal to `0` or `(size_t)EVENT_ID` is
 *    greater than `nevents_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `event` to the address of `arinc_events[EVENT_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `event->section`.
 * 4. Set `event->event_state` to `UP`.
 * 5. If `msection_wq_notify(&event->section, &event->process_queue, TRUE)` is
 *    equal to `POK_ERRNO_OK`:
 *    1. Set `t` to `event->process_queue.first`.
 *    2. Repeat the following while `t` is not equal to `JET_THREAD_ID_NONE` (at
 *       least once):
 *       1. Call `msection_wq_del()` with the arguments the address of
 *          `event->process_queue` and `t`.
 *       2. Set `t` to `event->process_queue.first`.
 * 6. Call `msection_leave()` with the address of `event->section`.
 * 7. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `msection_enter()`
 * - `msection_wq_notify()`
 * - `msection_wq_del()`
 * - `msection_leave()`
 */
void SET_EVENT (EVENT_ID_TYPE EVENT_ID,
                RETURN_CODE_TYPE *RETURN_CODE)
{
   if ((EVENT_ID <= 0) || ((size_t)EVENT_ID > nevents_used)) {
      // Incorrect event identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_event_t* event = &arinc_events[EVENT_ID - 1];

   msection_enter(&event->section);

   event->event_state = UP;

   if(msection_wq_notify(&event->section, &event->process_queue, TRUE)
      == POK_ERRNO_OK) {
      // There are processes waiting for event.
      // We are already woken up them, so just cleanup the process queue.
      pok_thread_id_t t = event->process_queue.first;

      do {
         msection_wq_del(&event->process_queue, t);

         t = event->process_queue.first;
      } while(t != JET_THREAD_ID_NONE);
   }

   msection_leave(&event->section);
   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `RESET_EVENT`
 *
 * @description
 * Sets an event down (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `EVENT_ID` | `EVENT_ID_TYPE` | [in] The event. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_events` | `arinc_event_t *` | The events of the partition |
 * | `nevents_used` | `size_t` | The number of events created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `arinc_event_t *` | The event |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `EVENT_ID` is less than or equal to `0` or `(size_t)EVENT_ID` is
 *    greater than `nevents_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `event` to the address of `arinc_events[EVENT_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `event->section`.
 * 4. Set `event->event_state` to `DOWN`.
 * 5. Call `msection_leave()` with the address of `event->section`.
 * 6. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `msection_enter()`
 * - `msection_leave()`
 */
void RESET_EVENT (EVENT_ID_TYPE EVENT_ID,
                  RETURN_CODE_TYPE *RETURN_CODE)
{
   if ((EVENT_ID <= 0) || ((size_t)EVENT_ID > nevents_used)) {
      // Incorrect event identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_event_t* event = &arinc_events[EVENT_ID - 1];

   msection_enter(&event->section);
   event->event_state = DOWN;
   msection_leave(&event->section);

   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `WAIT_EVENT`
 *
 * @description
 * Waits for an event to be up (ARINC 653 service), up to a time-out; the
 * waiting processes are released in priority, then FIFO order (ARINC 653 1-4).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `EVENT_ID` | `EVENT_ID_TYPE` | [in] The event. |
 * | `TIME_OUT` | `SYSTEM_TIME_TYPE` | [in] The time-out (0: no wait). |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, NOT_AVAILABLE, INVALID_MODE (waiting isn't allowed) or TIMED_OUT. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_events` | `arinc_event_t *` | The events of the partition |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nevents_used` | `size_t` | The number of events created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `arinc_event_t *` | The event |
 * | `t` | `pok_thread_id_t` | The current process |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `EVENT_ID` is less than or equal to `0` or `(size_t)EVENT_ID` is
 *    greater than `nevents_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `event` to the address of `arinc_events[EVENT_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `event->section`.
 * 4. If `event->event_state` is equal to `UP`:
 *    1. Set `*RETURN_CODE` to `NO_ERROR`.
 * 5. Otherwise, if `TIME_OUT` is equal to `0`:
 *    1. Set `*RETURN_CODE` to `NOT_AVAILABLE`.
 * 6. Otherwise:
 *    1. Set `t` to `kshd.current_thread_id`.
 *    2. Call `arinc_process_queue_add_common()` with the arguments the address
 *       of `event->process_queue` and `PRIORITY`.
 *    3. Depending on `msection_wait(&event->section, TIME_OUT)`:
 *       1. Case `POK_ERRNO_OK`:
 *          1. Set `*RETURN_CODE` to `NO_ERROR`.
 *          2. Leave the switch.
 *       2. Case `POK_ERRNO_MODE` or `POK_ERRNO_CANCELLED`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `event->process_queue` and `t`.
 *          2. Set `*RETURN_CODE` to `INVALID_MODE`.
 *          3. Leave the switch.
 *       3. Case `POK_ERRNO_TIMEOUT`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `event->process_queue` and `t`.
 *          2. Set `*RETURN_CODE` to `TIMED_OUT`.
 *          3. Leave the switch.
 *       4. Otherwise (default):
 *          1. Check that `FALSE` holds (assert_os: stop otherwise).
 *          2. Leave the switch.
 * 7. Call `msection_leave()` with the address of `event->section`.
 *
 * @calls
 * - `msection_enter()`
 * - `arinc_process_queue_add_common()`
 * - `msection_wait()`
 * - `msection_wq_del()`
 * - `assertion_os_fail()`
 * - `msection_leave()`
 */
void WAIT_EVENT (EVENT_ID_TYPE EVENT_ID,
                 SYSTEM_TIME_TYPE TIME_OUT,
                 RETURN_CODE_TYPE *RETURN_CODE)
{
   if ((EVENT_ID <= 0) || ((size_t)EVENT_ID > nevents_used)) {
      // Incorrect event identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_event_t* event = &arinc_events[EVENT_ID - 1];

   msection_enter(&event->section);

   if(event->event_state == UP) {
      // Event is already ready.
      *RETURN_CODE = NO_ERROR;
   }
   else if(TIME_OUT == 0)
   {
      // Event is DOWN but waiting is not requested.
      *RETURN_CODE = NOT_AVAILABLE;
   }
   else {
      // Event is DOWN and waiting is *requested* by the caller.
      // (whether waiting is *allowed* will be checked by the kernel.)
      pok_thread_id_t t = kshd.current_thread_id;

      /*
       * ARINC 1-4 explicitely says, that:
       * 
       * When processes are waiting on an event and another process sets
       * the event to up, then all the processes waiting on the event
       * become ready to run and are released on a priority followed by
       * FIFO (when priorities are equal) basis.
       */
      arinc_process_queue_add_common(&event->process_queue, PRIORITY);

      switch(msection_wait(&event->section, TIME_OUT))
      {
      case POK_ERRNO_OK:
         // Event become UP.
         *RETURN_CODE = NO_ERROR;
         break;
      case POK_ERRNO_MODE: // Waiting is not allowed
      case POK_ERRNO_CANCELLED: // Thread has been STOP()-ed or [IPPC] server thread has been cancelled.
         msection_wq_del(&event->process_queue, t);
         *RETURN_CODE = INVALID_MODE;
         break;
      case POK_ERRNO_TIMEOUT:
         // Timeout
         msection_wq_del(&event->process_queue, t);
         *RETURN_CODE = TIMED_OUT;
         break;
      default:
         assert_os(FALSE);
         break;
      }
   }
   msection_leave(&event->section);
}

/**
 * @funcname `GET_EVENT_ID`
 *
 * @description
 * Returns the ID of an event by name (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `EVENT_NAME` | `const EVENT_NAME_TYPE` | [in] The name. |
 * | `EVENT_ID` | `EVENT_ID_TYPE *` | [out] Where the ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_CONFIG. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_events` | `arinc_event_t *` | The events of the partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `arinc_event_t *` | The event |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `find_event()` with the argument `EVENT_NAME`, and store the result
 *    in `event`.
 * 2. If `event` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 3. Set `*EVENT_ID` to `(event - arinc_events) + 1`.
 * 4. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_event()`
 */
void GET_EVENT_ID (const EVENT_NAME_TYPE EVENT_NAME,
                   EVENT_ID_TYPE *EVENT_ID,
                   RETURN_CODE_TYPE *RETURN_CODE)
{

   arinc_event_t* event = find_event(EVENT_NAME);
   if(event == NULL) {
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   *EVENT_ID = (event - arinc_events) + 1;
   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `GET_EVENT_STATUS`
 *
 * @description
 * Returns the status of an event (ARINC 653 service): its state and the number
 * of waiting processes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `EVENT_ID` | `EVENT_ID_TYPE` | [in] The event. |
 * | `EVENT_STATUS` | `EVENT_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_events` | `arinc_event_t *` | The events of the partition |
 * | `nevents_used` | `size_t` | The number of events created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `event` | `arinc_event_t *` | The event |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `EVENT_ID` is less than or equal to `0` or `(size_t)EVENT_ID` is
 *    greater than `nevents_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `event` to the address of `arinc_events[EVENT_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `event->section`.
 * 4. Set `EVENT_STATUS->EVENT_STATE` to `event->event_state`.
 * 5. Call `msection_wq_size()` with the arguments the address of
 *    `event->section` and the address of `event->process_queue`, and store the
 *    result in `EVENT_STATUS->WAITING_PROCESSES`.
 * 6. Call `msection_leave()` with the address of `event->section`.
 * 7. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `msection_enter()`
 * - `msection_wq_size()`
 * - `msection_leave()`
 */
void GET_EVENT_STATUS (EVENT_ID_TYPE EVENT_ID,
                       EVENT_STATUS_TYPE *EVENT_STATUS,
                       RETURN_CODE_TYPE *RETURN_CODE)
{
   if ((EVENT_ID <= 0) || ((size_t)EVENT_ID > nevents_used)) {
      // Incorrect event identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_event_t* event = &arinc_events[EVENT_ID - 1];

   msection_enter(&event->section);
   EVENT_STATUS->EVENT_STATE = event->event_state;
   EVENT_STATUS->WAITING_PROCESSES = msection_wq_size(&event->section,
      &event->process_queue);

   msection_leave(&event->section);

   *RETURN_CODE = NO_ERROR;
}

#endif
