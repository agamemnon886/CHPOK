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
 * guideline can't be followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: name
 * parameters and the message parameter of the send or write service are
 * const (they are only read), so callers can pass string literals and
 * const data.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * ARINC 653 blackboards (intra-partition communication): CREATE_BLACKBOARD,
 * DISPLAY_BLACKBOARD, READ_BLACKBOARD, CLEAR_BLACKBOARD, GET_BLACKBOARD_ID
 * and GET_BLACKBOARD_STATUS.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_BLACKBOARD

#include "blackboard.h"

#include <arinc653/types.h>
#include <arinc653/blackboard.h>
#include <utils.h>

#include <kernel_shared_data.h>
#include <core/assert_os.h>

#include <string.h>
#include "arinc_alloc.h"
#include <arinc_config.h>
#include "arinc_process_queue.h"

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the allocator returns its
 * memory as a pointer to void, which is converted to the type of the object
 * allocated; the kernel passes message addresses to waiting processes as
 * pointers to void.
 * MISRA C:2025 Rule 18.4 deviation (this file): thread and object IDs are
 * the index of an element, computed as the difference between its address
 * and the start of its array.
 */

static size_t nblackboards_used = 0;

/**
 * @funcname `find_blackboard`
 *
 * @description
 * Finds a blackboard by name (case insensitive).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char *` | [in] The name. |
 *
 * @return The blackboard, or NULL if there is none.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_blackboards` | `arinc_blackboard_t *` | The blackboards of the partition |
 * | `nblackboards_used` | `size_t` | The number of blackboards created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `blackboard` | `arinc_blackboard_t *` | The blackboard |
 * | `i` | `size_t` | The index of a blackboard |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `nblackboards_used`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `blackboard` to the address of `arinc_blackboards[i]`.
 *    2. If `strncasecmp(blackboard->blackboard_name, name, MAX_NAME_LENGTH)` is
 *       equal to `0`:
 *       1. Return `blackboard`.
 * 2. Return `NULL`.
 *
 * @calls
 * - `strncasecmp()`
 */
static arinc_blackboard_t* find_blackboard(const char* name)
{
   for(size_t i = 0U; i < nblackboards_used; i++)
   {
      arinc_blackboard_t* blackboard = &arinc_blackboards[i];
      if(strncasecmp(blackboard->blackboard_name, name, MAX_NAME_LENGTH) == 0) {
         return blackboard;
      }
   }

   return NULL;
}


/**
 * @funcname `CREATE_BLACKBOARD`
 *
 * @description
 * Creates a blackboard (ARINC 653 service), not in NORMAL mode: its message
 * memory comes from the message allocator.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BLACKBOARD_NAME` | `const BLACKBOARD_NAME_TYPE` | [in] The name. |
 * | `MAX_MESSAGE_SIZE` | `MESSAGE_SIZE_TYPE` | [in] The maximum size of the message. |
 * | `BLACKBOARD_ID` | `BLACKBOARD_ID_TYPE *` | [out] Where its ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_MODE, NO_ACTION (the name exists), INVALID_CONFIG (too many or no memory) or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_blackboards` | `arinc_blackboard_t *` | The blackboards of the partition |
 * | `arinc_config_nblackboards` | `size_t` | The maximum number of blackboards (configuration) |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nblackboards_used` | `size_t` | The number of blackboards created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `blackboard` | `arinc_blackboard_t *` | The new blackboard |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `kshd.partition_mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 *    2. Return.
 * 2. If `find_blackboard(BLACKBOARD_NAME)` is not NULL:
 *    1. Set `*RETURN_CODE` to `NO_ACTION`.
 *    2. Return.
 * 3. If `nblackboards_used` is equal to `arinc_config_nblackboards`:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 4. If `MAX_MESSAGE_SIZE` is less than or equal to `0` or `MAX_MESSAGE_SIZE`
 *    is greater than `SYSTEM_LIMIT_MESSAGE_SIZE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 5. Set `blackboard` to the address of `arinc_blackboards[nblackboards_used]`.
 * 6. Set `blackboard->max_message_size` to `MAX_MESSAGE_SIZE`.
 * 7. Call `arinc_alloc()` with the arguments `blackboard->max_message_size` and
 *    `__alignof__(int32_t)`, and store the result in `blackboard->message`.
 * 8. If `blackboard->message` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 9. Call `strncpy()` with the arguments `blackboard->blackboard_name`,
 *    `BLACKBOARD_NAME` and `MAX_NAME_LENGTH`.
 * 10. Set `blackboard->message_size` to `0`.
 * 11. Call `msection_init()` with the address of `blackboard->section`.
 * 12. Call `msection_wq_init()` with the address of
 *     `blackboard->process_queue`.
 * 13. Set `*BLACKBOARD_ID` to `nblackboards_used + 1U`.
 * 14. Increment `nblackboards_used`.
 * 15. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_blackboard()`
 * - `arinc_alloc()`
 * - `strncpy()`
 * - `msection_init()`
 * - `msection_wq_init()`
 */
void CREATE_BLACKBOARD (
       /*in */ const BLACKBOARD_NAME_TYPE     BLACKBOARD_NAME,
       /*in */ MESSAGE_SIZE_TYPE        MAX_MESSAGE_SIZE,
       /*out*/ BLACKBOARD_ID_TYPE       *BLACKBOARD_ID,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{
   if(kshd.partition_mode == POK_PARTITION_MODE_NORMAL) {
      // Cannot create blackboard in NORMAL mode
      *RETURN_CODE = INVALID_MODE;
      return;
   }

   if(find_blackboard(BLACKBOARD_NAME) != NULL) {
      // Blackboard with given name already exists.
      *RETURN_CODE = NO_ACTION;
      return;
   }

   if(nblackboards_used == arinc_config_nblackboards) {
      // Too many blackboards
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   if((MAX_MESSAGE_SIZE <= 0)
      || (MAX_MESSAGE_SIZE > SYSTEM_LIMIT_MESSAGE_SIZE)) {
      // Message size is non-positive or too high.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_blackboard_t* blackboard = &arinc_blackboards[nblackboards_used];

   blackboard->max_message_size = MAX_MESSAGE_SIZE;

   // Optimize message for copiing.
   blackboard->message = arinc_alloc(blackboard->max_message_size, __alignof__(int32_t));

   if(blackboard->message == NULL)
   {
      // Failed to allocate message.
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   // The name may be shorter than MAX_NAME_LENGTH: don't read past its end.
   (void) strncpy(blackboard->blackboard_name, BLACKBOARD_NAME,
                  MAX_NAME_LENGTH);
   blackboard->message_size = 0;
   msection_init(&blackboard->section);
   msection_wq_init(&blackboard->process_queue);

   *BLACKBOARD_ID = nblackboards_used + 1U;// Avoid 0 value.

   nblackboards_used++;

   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `DISPLAY_BLACKBOARD`
 *
 * @description
 * Displays a message on a blackboard (ARINC 653 service): the processes waiting
 * on it receive the message and are released.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BLACKBOARD_ID` | `BLACKBOARD_ID_TYPE` | [in] The blackboard. |
 * | `MESSAGE_ADDR` | `const APEX_BYTE *` | [in] The message. |
 * | `LENGTH` | `MESSAGE_SIZE_TYPE` | [in] Its length. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_blackboards` | `arinc_blackboard_t *` | The blackboards of the partition |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nblackboards_used` | `size_t` | The number of blackboards created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `blackboard` | `arinc_blackboard_t *` | The blackboard |
 * | `t` | `pok_thread_id_t` | A waiting process |
 * | `w_dest` | `char *` | Its buffer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `BLACKBOARD_ID` is less than or equal to `0` or `(size_t)BLACKBOARD_ID`
 *    is greater than `nblackboards_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `blackboard` to the address of `arinc_blackboards[BLACKBOARD_ID - 1]`.
 * 3. If `LENGTH` is less than or equal to `0` or `LENGTH` is greater than
 *    `blackboard->max_message_size`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 4. Call `msection_enter()` with the address of `blackboard->section`.
 * 5. Call `memcpy()` with the arguments `blackboard->message`, `MESSAGE_ADDR`
 *    and `LENGTH`.
 * 6. Set `blackboard->message_size` to `LENGTH`.
 * 7. If `msection_wq_notify(&blackboard->section, &blackboard->process_queue,`
 *    `TRUE)` is equal to `POK_ERRNO_OK`:
 *    1. Set `t` to `blackboard->process_queue.first`.
 *    2. Repeat the following while `t` is not equal to `JET_THREAD_ID_NONE` (at
 *       least once):
 *       1. Set `w_dest` to `kshd.tshd[t].wq_buffer.dst`.
 *       2. Call `memcpy()` with the arguments `w_dest`, `blackboard->message`
 *          and `blackboard->message_size`.
 *       3. Set `kshd.tshd[t].wq_len` to `blackboard->message_size`.
 *       4. Call `msection_wq_del()` with the arguments the address of
 *          `blackboard->process_queue` and `t`.
 *       5. Set `t` to `blackboard->process_queue.first`.
 * 8. Call `msection_leave()` with the address of `blackboard->section`.
 * 9. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `msection_enter()`
 * - `memcpy()`
 * - `msection_wq_notify()`
 * - `msection_wq_del()`
 * - `msection_leave()`
 */
void DISPLAY_BLACKBOARD (
       /*in */ BLACKBOARD_ID_TYPE       BLACKBOARD_ID,
       /*in */ const APEX_BYTE *       MESSAGE_ADDR,       /* by reference */
       /*in */ MESSAGE_SIZE_TYPE        LENGTH,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{
   if ((BLACKBOARD_ID <= 0) || ((size_t)BLACKBOARD_ID > nblackboards_used)) {
      // Incorrect blackboard identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_blackboard_t* blackboard = &arinc_blackboards[BLACKBOARD_ID - 1];

   if((LENGTH <= 0) || (LENGTH > blackboard->max_message_size)) {
      // LENGTH is non-positive or too big.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   msection_enter(&blackboard->section);

   (void) memcpy(blackboard->message, MESSAGE_ADDR, LENGTH);
   blackboard->message_size = LENGTH;

   if(msection_wq_notify(&blackboard->section, &blackboard->process_queue, TRUE)
      == POK_ERRNO_OK) {
      // There are processes waiting on an empty blackboard.
      // We are already woken up them.
      pok_thread_id_t t = blackboard->process_queue.first;

      do {
         char* w_dest = kshd.tshd[t].wq_buffer.dst;
         (void) memcpy(w_dest, blackboard->message, blackboard->message_size);
         kshd.tshd[t].wq_len = blackboard->message_size;

         msection_wq_del(&blackboard->process_queue, t);

         t = blackboard->process_queue.first;
      } while(t != JET_THREAD_ID_NONE);
   }

   msection_leave(&blackboard->section);
   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `READ_BLACKBOARD`
 *
 * @description
 * Reads the message of a blackboard (ARINC 653 service); if it is empty, the
 * process waits up to a time-out (released in priority, then FIFO order).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BLACKBOARD_ID` | `BLACKBOARD_ID_TYPE` | [in] The blackboard. |
 * | `TIME_OUT` | `SYSTEM_TIME_TYPE` | [in] The time-out (0: no wait). |
 * | `MESSAGE_ADDR` | `MESSAGE_ADDR_TYPE` | [out] Where the message is stored. |
 * | `LENGTH` | `MESSAGE_SIZE_TYPE *` | [out] Where its length is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, NOT_AVAILABLE, INVALID_MODE (waiting isn't allowed) or TIMED_OUT. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_blackboards` | `arinc_blackboard_t *` | The blackboards of the partition |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nblackboards_used` | `size_t` | The number of blackboards created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `blackboard` | `arinc_blackboard_t *` | The blackboard |
 * | `t` | `pok_thread_id_t` | The current process |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `BLACKBOARD_ID` is less than or equal to `0` or `(size_t)BLACKBOARD_ID`
 *    is greater than `nblackboards_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `blackboard` to the address of `arinc_blackboards[BLACKBOARD_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `blackboard->section`.
 * 4. If `blackboard->message_size` is greater than `0`:
 *    1. Call `memcpy()` with the arguments `MESSAGE_ADDR`,
 *       `blackboard->message` and `blackboard->message_size`.
 *    2. Set `*LENGTH` to `blackboard->message_size`.
 *    3. Set `*RETURN_CODE` to `NO_ERROR`.
 * 5. Otherwise, if `TIME_OUT` is equal to `0`:
 *    1. Set `*LENGTH` to `0`.
 *    2. Set `*RETURN_CODE` to `NOT_AVAILABLE`.
 * 6. Otherwise:
 *    1. Set `t` to `kshd.current_thread_id`.
 *    2. Set `kshd.tshd[t].wq_buffer.dst` to `MESSAGE_ADDR`.
 *    3. Call `arinc_process_queue_add_common()` with the arguments the address
 *       of `blackboard->process_queue` and `PRIORITY`.
 *    4. Depending on `msection_wait(&blackboard->section, TIME_OUT)`:
 *       1. Case `POK_ERRNO_OK`:
 *          1. Set `*LENGTH` to `kshd.tshd[t].wq_len`.
 *          2. Set `*RETURN_CODE` to `NO_ERROR`.
 *          3. Leave the switch.
 *       2. Case `POK_ERRNO_MODE` or `POK_ERRNO_CANCELLED`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `blackboard->process_queue` and `t`.
 *          2. Set `*LENGTH` to `0`.
 *          3. Set `*RETURN_CODE` to `INVALID_MODE`.
 *          4. Leave the switch.
 *       3. Case `POK_ERRNO_TIMEOUT`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `blackboard->process_queue` and `t`.
 *          2. Set `*LENGTH` to `0`.
 *          3. Set `*RETURN_CODE` to `TIMED_OUT`.
 *          4. Leave the switch.
 *       4. Otherwise (default):
 *          1. Check that `FALSE` holds (assert_os: stop otherwise).
 *          2. Leave the switch.
 * 7. Call `msection_leave()` with the address of `blackboard->section`.
 *
 * @calls
 * - `msection_enter()`
 * - `memcpy()`
 * - `arinc_process_queue_add_common()`
 * - `msection_wait()`
 * - `msection_wq_del()`
 * - `assertion_os_fail()`
 * - `msection_leave()`
 */
void READ_BLACKBOARD (
       /*in */ BLACKBOARD_ID_TYPE       BLACKBOARD_ID,
       /*in */ SYSTEM_TIME_TYPE         TIME_OUT,
       /*out*/ MESSAGE_ADDR_TYPE        MESSAGE_ADDR,
       /*out*/ MESSAGE_SIZE_TYPE        *LENGTH,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{
   if ((BLACKBOARD_ID <= 0) || ((size_t)BLACKBOARD_ID > nblackboards_used)) {
      // Incorrect blackboard identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_blackboard_t* blackboard = &arinc_blackboards[BLACKBOARD_ID - 1];

   msection_enter(&blackboard->section);

   if(blackboard->message_size > 0) {
      // There is a message in the blackboard.
      (void) memcpy(MESSAGE_ADDR, blackboard->message,
                    blackboard->message_size);
      *LENGTH = blackboard->message_size;
      *RETURN_CODE = NO_ERROR;
   }
   else if(TIME_OUT == 0)
   {
      // There is no message in blackboard but waiting is not requested.
      *LENGTH = 0;
      *RETURN_CODE = NOT_AVAILABLE;
   }
   else {
      // Blackboard is empty and waiting is *requested* by the caller.
      // (whether waiting is *allowed* will be checked by the kernel.)
      pok_thread_id_t t = kshd.current_thread_id;

      kshd.tshd[t].wq_buffer.dst = MESSAGE_ADDR;

      /*
       * ARINC explicitely says, that:
       * 
       * If processes were waiting on the empty blackboard, the
       * processes will be released on a priority followed by FIFO (when
       * priorities are equal) basis.
       */
      arinc_process_queue_add_common(&blackboard->process_queue, PRIORITY);

      switch(msection_wait(&blackboard->section, TIME_OUT))
      {
      case POK_ERRNO_OK:
         // Message is already copied from the blackboard by the notifier.
         *LENGTH = kshd.tshd[t].wq_len;
         *RETURN_CODE = NO_ERROR;
         break;
      case POK_ERRNO_MODE: // Waiting is not allowed
      case POK_ERRNO_CANCELLED: // Thread has been STOP()-ed or [IPPC] server thread has been cancelled.
         msection_wq_del(&blackboard->process_queue, t);
         *LENGTH = 0;
         *RETURN_CODE = INVALID_MODE;
         break;
      case POK_ERRNO_TIMEOUT:
         // Timeout
         msection_wq_del(&blackboard->process_queue, t);
         *LENGTH = 0;
         *RETURN_CODE = TIMED_OUT;
         break;
      default:
         assert_os(FALSE);
         break;
      }
   }

   msection_leave(&blackboard->section);
}

/**
 * @funcname `CLEAR_BLACKBOARD`
 *
 * @description
 * Clears the message of a blackboard (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BLACKBOARD_ID` | `BLACKBOARD_ID_TYPE` | [in] The blackboard. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_blackboards` | `arinc_blackboard_t *` | The blackboards of the partition |
 * | `nblackboards_used` | `size_t` | The number of blackboards created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `blackboard` | `arinc_blackboard_t *` | The blackboard |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `BLACKBOARD_ID` is less than or equal to `0` or `(size_t)BLACKBOARD_ID`
 *    is greater than `nblackboards_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `blackboard` to the address of `arinc_blackboards[BLACKBOARD_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `blackboard->section`.
 * 4. Set `blackboard->message_size` to `0`.
 * 5. Call `msection_leave()` with the address of `blackboard->section`.
 * 6. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `msection_enter()`
 * - `msection_leave()`
 */
void CLEAR_BLACKBOARD (
       /*in */ BLACKBOARD_ID_TYPE       BLACKBOARD_ID,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{
   if ((BLACKBOARD_ID <= 0) || ((size_t)BLACKBOARD_ID > nblackboards_used)) {
      // Incorrect blackboard identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_blackboard_t* blackboard = &arinc_blackboards[BLACKBOARD_ID - 1];

   msection_enter(&blackboard->section);
   blackboard->message_size = 0;
   msection_leave(&blackboard->section);

   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `GET_BLACKBOARD_ID`
 *
 * @description
 * Returns the ID of a blackboard by name (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BLACKBOARD_NAME` | `const BLACKBOARD_NAME_TYPE` | [in] The name. |
 * | `BLACKBOARD_ID` | `BLACKBOARD_ID_TYPE *` | [out] Where the ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_CONFIG. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_blackboards` | `arinc_blackboard_t *` | The blackboards of the partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `blackboard` | `arinc_blackboard_t *` | The blackboard |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `find_blackboard()` with the argument `BLACKBOARD_NAME`, and store
 *    the result in `blackboard`.
 * 2. If `blackboard` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 3. Set `*BLACKBOARD_ID` to `(blackboard - arinc_blackboards) + 1`.
 * 4. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_blackboard()`
 */
void GET_BLACKBOARD_ID (
       /*in */ const BLACKBOARD_NAME_TYPE     BLACKBOARD_NAME,
       /*out*/ BLACKBOARD_ID_TYPE       *BLACKBOARD_ID,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{

   arinc_blackboard_t* blackboard = find_blackboard(BLACKBOARD_NAME);
   if(blackboard == NULL) {
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   *BLACKBOARD_ID = (blackboard - arinc_blackboards) + 1;
   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `GET_BLACKBOARD_STATUS`
 *
 * @description
 * Returns the status of a blackboard (ARINC 653 service): its maximum message
 * size, whether it is empty and the number of waiting processes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BLACKBOARD_ID` | `BLACKBOARD_ID_TYPE` | [in] The blackboard. |
 * | `BLACKBOARD_STATUS` | `BLACKBOARD_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_blackboards` | `arinc_blackboard_t *` | The blackboards of the partition |
 * | `nblackboards_used` | `size_t` | The number of blackboards created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `blackboard` | `arinc_blackboard_t *` | The blackboard |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `BLACKBOARD_ID` is less than or equal to `0` or `(size_t)BLACKBOARD_ID`
 *    is greater than `nblackboards_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `blackboard` to the address of `arinc_blackboards[BLACKBOARD_ID - 1]`.
 * 3. Set `BLACKBOARD_STATUS->MAX_MESSAGE_SIZE` to
 *    `blackboard->max_message_size`.
 * 4. Call `msection_enter()` with the address of `blackboard->section`.
 * 5. Set `BLACKBOARD_STATUS->EMPTY_INDICATOR` to `EMPTY` if
 *    `blackboard->message_size` is equal to `0`, otherwise `OCCUPIED`.
 * 6. Call `msection_wq_size()` with the arguments the address of
 *    `blackboard->section` and the address of `blackboard->process_queue`, and
 *    store the result in `BLACKBOARD_STATUS->WAITING_PROCESSES`.
 * 7. Call `msection_leave()` with the address of `blackboard->section`.
 * 8. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `msection_enter()`
 * - `msection_wq_size()`
 * - `msection_leave()`
 */
void GET_BLACKBOARD_STATUS (
       /*in */ BLACKBOARD_ID_TYPE       BLACKBOARD_ID,
       /*out*/ BLACKBOARD_STATUS_TYPE   *BLACKBOARD_STATUS,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{
   if ((BLACKBOARD_ID <= 0) || ((size_t)BLACKBOARD_ID > nblackboards_used)) {
      // Incorrect blackboard identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_blackboard_t* blackboard = &arinc_blackboards[BLACKBOARD_ID - 1];

   BLACKBOARD_STATUS->MAX_MESSAGE_SIZE = blackboard->max_message_size;

   msection_enter(&blackboard->section);
   BLACKBOARD_STATUS->EMPTY_INDICATOR = (blackboard->message_size == 0)
      ? EMPTY: OCCUPIED;
   BLACKBOARD_STATUS->WAITING_PROCESSES =
      msection_wq_size(&blackboard->section, &blackboard->process_queue);
   msection_leave(&blackboard->section);

   *RETURN_CODE = NO_ERROR;
}
#endif
