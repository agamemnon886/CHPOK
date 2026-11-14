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
 * ARINC 653 semaphores (intra-partition communication): CREATE_SEMAPHORE,
 * WAIT_SEMAPHORE, SIGNAL_SEMAPHORE, GET_SEMAPHORE_ID and
 * GET_SEMAPHORE_STATUS.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_SEMAPHORE

#include "semaphore.h"

#include <arinc653/types.h>
#include <arinc653/semaphore.h>
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

static size_t nsemaphores_used = 0;

/**
 * @funcname `find_semaphore`
 *
 * @description
 * Finds a semaphore by name (case insensitive).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char *` | [in] The name. |
 *
 * @return The semaphore, or NULL if there is none.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_semaphores` | `arinc_semaphore_t *` | The semaphores of the partition |
 * | `nsemaphores_used` | `size_t` | The number of semaphores created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of a semaphore |
 * | `semaphore` | `arinc_semaphore_t *` | The semaphore |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `nsemaphores_used`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `semaphore` to the address of `arinc_semaphores[i]`.
 *    2. If `strncasecmp(semaphore->semaphore_name, name, MAX_NAME_LENGTH)` is
 *       equal to `0`:
 *       1. Return `semaphore`.
 * 2. Return `NULL`.
 *
 * @calls
 * - `strncasecmp()`
 */
static arinc_semaphore_t* find_semaphore(const char* name)
{
   for(size_t i = 0U; i < nsemaphores_used; i++)
   {
      arinc_semaphore_t* semaphore = &arinc_semaphores[i];
      if(strncasecmp(semaphore->semaphore_name, name, MAX_NAME_LENGTH) == 0) {
         return semaphore;
      }
   }

   return NULL;
}

/**
 * @funcname `CREATE_SEMAPHORE`
 *
 * @description
 * Creates a semaphore (ARINC 653 service), not in NORMAL mode.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SEMAPHORE_NAME` | `const SEMAPHORE_NAME_TYPE` | [in] The name. |
 * | `CURRENT_VALUE` | `SEMAPHORE_VALUE_TYPE` | [in] The initial value. |
 * | `MAXIMUM_VALUE` | `SEMAPHORE_VALUE_TYPE` | [in] The maximum value. |
 * | `QUEUING_DISCIPLINE` | `QUEUING_DISCIPLINE_TYPE` | [in] The discipline of the waiting processes (FIFO or PRIORITY). |
 * | `SEMAPHORE_ID` | `SEMAPHORE_ID_TYPE *` | [out] Where its ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_MODE, NO_ACTION (the name exists), INVALID_CONFIG (too many) or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_config_nsemaphores` | `size_t` | The maximum number of semaphores (configuration) |
 * | `arinc_semaphores` | `arinc_semaphore_t *` | The semaphores of the partition |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nsemaphores_used` | `size_t` | The number of semaphores created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `semaphore` | `arinc_semaphore_t *` | The new semaphore |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `kshd.partition_mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 *    2. Return.
 * 2. If `find_semaphore(SEMAPHORE_NAME)` is not NULL:
 *    1. Set `*RETURN_CODE` to `NO_ACTION`.
 *    2. Return.
 * 3. If `nsemaphores_used` is equal to `arinc_config_nsemaphores`:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 4. If `MAXIMUM_VALUE` is less than `0` or `MAXIMUM_VALUE` is greater than
 *    `MAX_SEMAPHORE_VALUE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 5. If `CURRENT_VALUE` is less than `0` or `CURRENT_VALUE` is greater than
 *    `MAXIMUM_VALUE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 6. If `QUEUING_DISCIPLINE` is not equal to `FIFO` and `QUEUING_DISCIPLINE` is
 *    not equal to `PRIORITY`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 7. Set `semaphore` to the address of `arinc_semaphores[nsemaphores_used]`.
 * 8. Call `strncpy()` with the arguments `semaphore->semaphore_name`,
 *    `SEMAPHORE_NAME` and `MAX_NAME_LENGTH`.
 * 9. Set `semaphore->maximum_value` to `MAXIMUM_VALUE`.
 * 10. Set `semaphore->current_value` to `CURRENT_VALUE`.
 * 11. Set `semaphore->discipline` to `QUEUING_DISCIPLINE`.
 * 12. Call `msection_init()` with the address of `semaphore->section`.
 * 13. Call `msection_wq_init()` with the address of `semaphore->process_queue`.
 * 14. Set `*SEMAPHORE_ID` to `nsemaphores_used + 1U`.
 * 15. Increment `nsemaphores_used`.
 * 16. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_semaphore()`
 * - `strncpy()`
 * - `msection_init()`
 * - `msection_wq_init()`
 */
void CREATE_SEMAPHORE (const SEMAPHORE_NAME_TYPE SEMAPHORE_NAME,
                       SEMAPHORE_VALUE_TYPE CURRENT_VALUE,
                       SEMAPHORE_VALUE_TYPE MAXIMUM_VALUE,
                       QUEUING_DISCIPLINE_TYPE QUEUING_DISCIPLINE,
                       SEMAPHORE_ID_TYPE *SEMAPHORE_ID,
                       RETURN_CODE_TYPE *RETURN_CODE )
{
   if(kshd.partition_mode == POK_PARTITION_MODE_NORMAL) {
      // Cannot create semaphore in NORMAL mode
      *RETURN_CODE = INVALID_MODE;
      return;
   }

   if(find_semaphore(SEMAPHORE_NAME) != NULL) {
      // Semaphore with given name already exists.
      *RETURN_CODE = NO_ACTION;
      return;
   }

   if(nsemaphores_used == arinc_config_nsemaphores) {
      // Too many semaphores
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   if((MAXIMUM_VALUE < 0) || (MAXIMUM_VALUE > MAX_SEMAPHORE_VALUE)) {
      // Maximum value is negative or too high.
      // DEV: Current implementation SIGNAL_SEMAPHORE allows to signal semaphore with maximum value, if there are waiters.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if((CURRENT_VALUE < 0) || (CURRENT_VALUE > MAXIMUM_VALUE)) {
      // Current value is negative or too high.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if((QUEUING_DISCIPLINE != FIFO) && (QUEUING_DISCIPLINE != PRIORITY)) {
      // Incorrect discipline value.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_semaphore_t* semaphore = &arinc_semaphores[nsemaphores_used];

   // The name may be shorter than MAX_NAME_LENGTH: don't read past its end.
   (void) strncpy(semaphore->semaphore_name, SEMAPHORE_NAME, MAX_NAME_LENGTH);
   semaphore->maximum_value = MAXIMUM_VALUE;
   semaphore->current_value = CURRENT_VALUE;
   semaphore->discipline = QUEUING_DISCIPLINE;

   msection_init(&semaphore->section);
   msection_wq_init(&semaphore->process_queue);

   *SEMAPHORE_ID = nsemaphores_used + 1U;// Avoid 0 value.

   nsemaphores_used++;

   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `WAIT_SEMAPHORE`
 *
 * @description
 * Takes a semaphore (ARINC 653 service); if its value is 0, the process waits
 * up to a time-out.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SEMAPHORE_ID` | `SEMAPHORE_ID_TYPE` | [in] The semaphore. |
 * | `TIME_OUT` | `SYSTEM_TIME_TYPE` | [in] The time-out (0: no wait). |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, NOT_AVAILABLE, INVALID_MODE (waiting isn't allowed) or TIMED_OUT. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_semaphores` | `arinc_semaphore_t *` | The semaphores of the partition |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nsemaphores_used` | `size_t` | The number of semaphores created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `semaphore` | `arinc_semaphore_t *` | The semaphore |
 * | `t` | `pok_thread_id_t` | The current process |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `SEMAPHORE_ID` is less than or equal to `0` or `(size_t)SEMAPHORE_ID`
 *    is greater than `nsemaphores_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `semaphore` to the address of `arinc_semaphores[SEMAPHORE_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `semaphore->section`.
 * 4. If `semaphore->current_value` is greater than `0`:
 *    1. Decrement `semaphore->current_value`.
 *    2. Set `*RETURN_CODE` to `NO_ERROR`.
 * 5. Otherwise, if `TIME_OUT` is equal to `0`:
 *    1. Set `*RETURN_CODE` to `NOT_AVAILABLE`.
 * 6. Otherwise:
 *    1. Set `t` to `kshd.current_thread_id`.
 *    2. Call `arinc_process_queue_add_common()` with the arguments the address
 *       of `semaphore->process_queue` and `semaphore->discipline`.
 *    3. Depending on `msection_wait(&semaphore->section, TIME_OUT)`:
 *       1. Case `POK_ERRNO_OK`:
 *          1. Set `*RETURN_CODE` to `NO_ERROR`.
 *          2. Leave the switch.
 *       2. Case `POK_ERRNO_MODE` or `POK_ERRNO_CANCELLED`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `semaphore->process_queue` and `t`.
 *          2. Set `*RETURN_CODE` to `INVALID_MODE`.
 *          3. Leave the switch.
 *       3. Case `POK_ERRNO_TIMEOUT`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `semaphore->process_queue` and `t`.
 *          2. Set `*RETURN_CODE` to `TIMED_OUT`.
 *          3. Leave the switch.
 *       4. Otherwise (default):
 *          1. Check that `FALSE` holds (assert_os: stop otherwise).
 *          2. Leave the switch.
 * 7. Call `msection_leave()` with the address of `semaphore->section`.
 *
 * @calls
 * - `msection_enter()`
 * - `arinc_process_queue_add_common()`
 * - `msection_wait()`
 * - `msection_wq_del()`
 * - `assertion_os_fail()`
 * - `msection_leave()`
 */
void WAIT_SEMAPHORE (SEMAPHORE_ID_TYPE SEMAPHORE_ID,
                     SYSTEM_TIME_TYPE TIME_OUT,
                     RETURN_CODE_TYPE *RETURN_CODE )
{
   if ((SEMAPHORE_ID <= 0) || ((size_t)SEMAPHORE_ID > nsemaphores_used)) {
      // Incorrect semaphore identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_semaphore_t* semaphore = &arinc_semaphores[SEMAPHORE_ID - 1];

   msection_enter(&semaphore->section);

   if(semaphore->current_value > 0) {
      // Current value is positive.
      semaphore->current_value--;
      *RETURN_CODE = NO_ERROR;
   }

   else if(TIME_OUT == 0) {
      // Current value is 0 but waiting is not requested.
      *RETURN_CODE = NOT_AVAILABLE;
   }

   else {
      // Current value is 0 and waiting is *requested* by the caller.
      // (whether waiting is *allowed* will be checked by the kernel.)
      pok_thread_id_t t = kshd.current_thread_id;

      arinc_process_queue_add_common(&semaphore->process_queue, semaphore->discipline);

      switch(msection_wait(&semaphore->section, TIME_OUT))
      {
      case POK_ERRNO_OK:
         // Current value is already decremented for us by the notifier.
         *RETURN_CODE = NO_ERROR;
         break;
      case POK_ERRNO_MODE: // Waiting is not allowed
      case POK_ERRNO_CANCELLED: // Thread has been STOP()-ed or [IPPC] server thread has been cancelled.
         msection_wq_del(&semaphore->process_queue, t);
         *RETURN_CODE = INVALID_MODE;
         break;
      case POK_ERRNO_TIMEOUT:
         // Timeout
         msection_wq_del(&semaphore->process_queue, t);
         *RETURN_CODE = TIMED_OUT;
         break;
      default:
         assert_os(FALSE);
         break;
      }
   }

   msection_leave(&semaphore->section);
}

/**
 * @funcname `SIGNAL_SEMAPHORE`
 *
 * @description
 * Signals a semaphore (ARINC 653 service): the first waiting process is
 * released, or the value is increased. Unlike ARINC 653, a semaphore at its
 * maximum value may be signaled if a process waits on it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SEMAPHORE_ID` | `SEMAPHORE_ID_TYPE` | [in] The semaphore. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM or NO_ACTION (at the maximum value). |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_semaphores` | `arinc_semaphore_t *` | The semaphores of the partition |
 * | `nsemaphores_used` | `size_t` | The number of semaphores created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `semaphore` | `arinc_semaphore_t *` | The semaphore |
 * | `t_awoken` | `pok_thread_id_t` | The process released |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `SEMAPHORE_ID` is less than or equal to `0` or `(size_t)SEMAPHORE_ID`
 *    is greater than `nsemaphores_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `semaphore` to the address of `arinc_semaphores[SEMAPHORE_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `semaphore->section`.
 * 4. If `msection_wq_notify(&semaphore->section, &semaphore->process_queue,`
 *    `FALSE)` is equal to `POK_ERRNO_OK`:
 *    1. Set `t_awoken` to `semaphore->process_queue.first`.
 *    2. Call `msection_wq_del()` with the arguments the address of
 *       `semaphore->process_queue` and `t_awoken`.
 *    3. Set `*RETURN_CODE` to `NO_ERROR`.
 * 5. Otherwise, if `semaphore->current_value` is equal to
 *    `semaphore->maximum_value`:
 *    1. Set `*RETURN_CODE` to `NO_ACTION`.
 * 6. Otherwise:
 *    1. Increment `semaphore->current_value`.
 *    2. Set `*RETURN_CODE` to `NO_ERROR`.
 * 7. Call `msection_leave()` with the address of `semaphore->section`.
 *
 * @calls
 * - `msection_enter()`
 * - `msection_wq_notify()`
 * - `msection_wq_del()`
 * - `msection_leave()`
 */
void SIGNAL_SEMAPHORE (SEMAPHORE_ID_TYPE SEMAPHORE_ID,
                       RETURN_CODE_TYPE *RETURN_CODE )
{
   if ((SEMAPHORE_ID <= 0) || ((size_t)SEMAPHORE_ID > nsemaphores_used)) {
      // Incorrect semaphore identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_semaphore_t* semaphore = &arinc_semaphores[SEMAPHORE_ID - 1];

   msection_enter(&semaphore->section);

   if(msection_wq_notify(&semaphore->section,
      &semaphore->process_queue, FALSE) == POK_ERRNO_OK) {
      // There are waiters on semaphore. We have already awoken the first of them.
      /*
       * DEV: ARINC says to emit error if current_value == maximum_value.
       * 
       * But we allow to signal such semaphore if someone waits on it.
       */
      pok_thread_id_t t_awoken = semaphore->process_queue.first;
      msection_wq_del(&semaphore->process_queue, t_awoken);
      *RETURN_CODE = NO_ERROR;
   }

   else if(semaphore->current_value == semaphore->maximum_value) {
      // Current value achive the maximum.
      *RETURN_CODE = NO_ACTION;
   }

   else {
      // No one waits on semaphore. Just increment its current value.
      semaphore->current_value ++;
      *RETURN_CODE = NO_ERROR;
   }

   msection_leave(&semaphore->section);
}

/**
 * @funcname `GET_SEMAPHORE_ID`
 *
 * @description
 * Returns the ID of a semaphore by name (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SEMAPHORE_NAME` | `const SEMAPHORE_NAME_TYPE` | [in] The name. |
 * | `SEMAPHORE_ID` | `SEMAPHORE_ID_TYPE *` | [out] Where the ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_CONFIG. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_semaphores` | `arinc_semaphore_t *` | The semaphores of the partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `semaphore` | `arinc_semaphore_t *` | The semaphore |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `find_semaphore()` with the argument `SEMAPHORE_NAME`, and store the
 *    result in `semaphore`.
 * 2. If `semaphore` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 3. Set `*SEMAPHORE_ID` to `(semaphore - arinc_semaphores) + 1`.
 * 4. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_semaphore()`
 */
void GET_SEMAPHORE_ID (const SEMAPHORE_NAME_TYPE SEMAPHORE_NAME,
                       SEMAPHORE_ID_TYPE *SEMAPHORE_ID,
                       RETURN_CODE_TYPE *RETURN_CODE )
{

   arinc_semaphore_t* semaphore = find_semaphore(SEMAPHORE_NAME);
   if(semaphore == NULL) {
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   *SEMAPHORE_ID = (semaphore - arinc_semaphores) + 1;
   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `GET_SEMAPHORE_STATUS`
 *
 * @description
 * Returns the status of a semaphore (ARINC 653 service): its current and
 * maximum values and the number of waiting processes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SEMAPHORE_ID` | `SEMAPHORE_ID_TYPE` | [in] The semaphore. |
 * | `SEMAPHORE_STATUS` | `SEMAPHORE_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_semaphores` | `arinc_semaphore_t *` | The semaphores of the partition |
 * | `nsemaphores_used` | `size_t` | The number of semaphores created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `semaphore` | `arinc_semaphore_t *` | The semaphore |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `SEMAPHORE_ID` is less than or equal to `0` or `(size_t)SEMAPHORE_ID`
 *    is greater than `nsemaphores_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `semaphore` to the address of `arinc_semaphores[SEMAPHORE_ID - 1]`.
 * 3. Set `SEMAPHORE_STATUS->MAXIMUM_VALUE` to `semaphore->maximum_value`.
 * 4. Call `msection_enter()` with the address of `semaphore->section`.
 * 5. Set `SEMAPHORE_STATUS->CURRENT_VALUE` to `semaphore->current_value`.
 * 6. Call `msection_wq_size()` with the arguments the address of
 *    `semaphore->section` and the address of `semaphore->process_queue`, and
 *    store the result in `SEMAPHORE_STATUS->WAITING_PROCESSES`.
 * 7. Call `msection_leave()` with the address of `semaphore->section`.
 * 8. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `msection_enter()`
 * - `msection_wq_size()`
 * - `msection_leave()`
 */
void GET_SEMAPHORE_STATUS (SEMAPHORE_ID_TYPE SEMAPHORE_ID,
                           SEMAPHORE_STATUS_TYPE *SEMAPHORE_STATUS,
                           RETURN_CODE_TYPE *RETURN_CODE )
{
   if ((SEMAPHORE_ID <= 0) || ((size_t)SEMAPHORE_ID > nsemaphores_used)) {
      // Incorrect semaphore identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_semaphore_t* semaphore = &arinc_semaphores[SEMAPHORE_ID - 1];

   SEMAPHORE_STATUS->MAXIMUM_VALUE = semaphore->maximum_value;

   msection_enter(&semaphore->section);

   SEMAPHORE_STATUS->CURRENT_VALUE = semaphore->current_value;
   SEMAPHORE_STATUS->WAITING_PROCESSES = msection_wq_size(&semaphore->section,
      &semaphore->process_queue);

   msection_leave(&semaphore->section);

   *RETURN_CODE = NO_ERROR;
}

#endif
