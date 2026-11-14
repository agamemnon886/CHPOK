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
 * ARINC 653 buffers (intra-partition communication): CREATE_BUFFER,
 * SEND_BUFFER, RECEIVE_BUFFER, GET_BUFFER_ID and GET_BUFFER_STATUS. The
 * messages are kept in a ring.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_BUFFER

#include "buffer.h"

#include <arinc653/types.h>
#include <arinc653/buffer.h>
#include <types.h>
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
 * and the start of its array; a message is found at the start of the message
 * array plus its index times the message stride.
 */

static size_t nbuffers_used = 0;

/**
 * @funcname `find_buffer`
 *
 * @description
 * Finds a buffer by name (case insensitive).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char *` | [in] The name. |
 *
 * @return The buffer, or NULL if there is none.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_buffers` | `arinc_buffer_t *` | The buffers of the partition |
 * | `nbuffers_used` | `size_t` | The number of buffers created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffer` | `arinc_buffer_t *` | The buffer |
 * | `i` | `size_t` | The index of a buffer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `nbuffers_used`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `buffer` to the address of `arinc_buffers[i]`.
 *    2. If `strncasecmp(buffer->buffer_name, name, MAX_NAME_LENGTH)` is equal
 *       to `0`:
 *       1. Return `buffer`.
 * 2. Return `NULL`.
 *
 * @calls
 * - `strncasecmp()`
 */
static arinc_buffer_t* find_buffer(const char* name)
{
   for(size_t i = 0U; i < nbuffers_used; i++)
   {
      arinc_buffer_t* buffer = &arinc_buffers[i];
      if(strncasecmp(buffer->buffer_name, name, MAX_NAME_LENGTH) == 0) {
         return buffer;
      }
   }

   return NULL;
}

/**
 * @funcname `message_index`
 *
 * @description
 * Returns the index in the ring of the message at an offset from the first one.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffer` | `arinc_buffer_t *` | [in] The buffer. |
 * | `offset` | `MESSAGE_RANGE_TYPE` | [in] The offset. |
 *
 * @return The index.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `res` | `MESSAGE_RANGE_TYPE` | The index |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `res` to `buffer->base_offset + offset`.
 * 2. If `res` is greater than or equal to `buffer->max_nb_message`:
 *    1. Subtract `buffer->max_nb_message` from `res`.
 * 3. Return `res`.
 *
 * @calls
 * None.
 */
static MESSAGE_RANGE_TYPE
message_index(arinc_buffer_t* buffer, MESSAGE_RANGE_TYPE offset)
{
   MESSAGE_RANGE_TYPE res = buffer->base_offset + offset;

   if(res >= buffer->max_nb_message) {
      res -= buffer->max_nb_message;
   }

   return res;
}

/**
 * @funcname `message_at`
 *
 * @description
 * Returns the message at an index of the ring.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffer` | `arinc_buffer_t *` | [in] The buffer. |
 * | `index` | `MESSAGE_RANGE_TYPE` | [in] The index. |
 *
 * @return The message.
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
 * 1. Return `buffer->messages + (buffer->message_stride * index)`.
 *
 * @calls
 * None.
 */
static char*
message_at(arinc_buffer_t* buffer, MESSAGE_RANGE_TYPE index)
{
   return buffer->messages + (buffer->message_stride * index);
}

/**
 * @funcname `CREATE_BUFFER`
 *
 * @description
 * Creates a buffer (ARINC 653 service), not in NORMAL mode: its messages and
 * their sizes come from the message allocator.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BUFFER_NAME` | `const BUFFER_NAME_TYPE` | [in] The name. |
 * | `MAX_MESSAGE_SIZE` | `MESSAGE_SIZE_TYPE` | [in] The maximum size of a message. |
 * | `MAX_NB_MESSAGE` | `MESSAGE_RANGE_TYPE` | [in] The maximum number of messages. |
 * | `QUEUING_DISCIPLINE` | `QUEUING_DISCIPLINE_TYPE` | [in] The discipline of the waiting processes (FIFO or PRIORITY). |
 * | `BUFFER_ID` | `BUFFER_ID_TYPE *` | [out] Where its ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_MODE, NO_ACTION (the name exists), INVALID_CONFIG (too many or no memory) or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_buffers` | `arinc_buffer_t *` | The buffers of the partition |
 * | `arinc_config_nbuffers` | `size_t` | The maximum number of buffers (configuration) |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nbuffers_used` | `size_t` | The number of buffers created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `astate` | `arinc_allocator_state` | The allocator's state before |
 * | `buffer` | `arinc_buffer_t *` | The new buffer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `kshd.partition_mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 *    2. Return.
 * 2. If `find_buffer(BUFFER_NAME)` is not NULL:
 *    1. Set `*RETURN_CODE` to `NO_ACTION`.
 *    2. Return.
 * 3. If `nbuffers_used` is equal to `arinc_config_nbuffers`:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 4. If `MAX_MESSAGE_SIZE` is less than or equal to `0` or `MAX_MESSAGE_SIZE`
 *    is greater than `SYSTEM_LIMIT_MESSAGE_SIZE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 5. If `MAX_NB_MESSAGE` is less than or equal to `0` or `MAX_NB_MESSAGE` is
 *    greater than `SYSTEM_LIMIT_NUMBER_OF_MESSAGES`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 6. If `QUEUING_DISCIPLINE` is not equal to `FIFO` and `QUEUING_DISCIPLINE` is
 *    not equal to `PRIORITY`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 7. Set `buffer` to the address of `arinc_buffers[nbuffers_used]`.
 * 8. Set `buffer->max_message_size` to `MAX_MESSAGE_SIZE`.
 * 9. Call `ALIGN()` with the arguments `buffer->max_message_size` and
 *    `__alignof__(int32_t)`, and store the result in `buffer->message_stride`.
 * 10. Set `buffer->max_nb_message` to `MAX_NB_MESSAGE`.
 * 11. Call `arinc_allocator_get_state()`, and store the result in `astate`.
 * 12. Call `arinc_alloc()` with the arguments
 *     `buffer->message_stride * buffer->max_nb_message` and
 *     `__alignof__(int32_t)`, and store the result in `buffer->messages`.
 * 13. Call `arinc_alloc()` with the arguments
 *     `(size_t)buffer->max_nb_message * sizeof(*buffer->messages_size)` and
 *     `__alignof__(*buffer->messages_size)`, and store the result in
 *     `buffer->messages_size`.
 * 14. If `buffer->messages` is NULL or `buffer->messages_size` is NULL:
 *     1. Call `arinc_allocator_reset_state()` with the argument `astate`.
 *     2. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *     3. Return.
 * 15. Call `strncpy()` with the arguments `buffer->buffer_name`, `BUFFER_NAME`
 *     and `MAX_NAME_LENGTH`.
 * 16. Call `msection_init()` with the address of `buffer->section`.
 * 17. Call `msection_wq_init()` with the address of `buffer->process_queue`.
 * 18. Set `buffer->base_offset` to `0`.
 * 19. Set `buffer->discipline` to `QUEUING_DISCIPLINE`.
 * 20. Set `*BUFFER_ID` to `nbuffers_used + 1U`.
 * 21. Increment `nbuffers_used`.
 * 22. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_buffer()`
 * - `ALIGN()`
 * - `arinc_allocator_get_state()`
 * - `arinc_alloc()`
 * - `arinc_allocator_reset_state()`
 * - `strncpy()`
 * - `msection_init()`
 * - `msection_wq_init()`
 */
void CREATE_BUFFER (
       /*in */ const BUFFER_NAME_TYPE         BUFFER_NAME,
       /*in */ MESSAGE_SIZE_TYPE        MAX_MESSAGE_SIZE,
       /*in */ MESSAGE_RANGE_TYPE       MAX_NB_MESSAGE,
       /*in */ QUEUING_DISCIPLINE_TYPE  QUEUING_DISCIPLINE,
       /*out*/ BUFFER_ID_TYPE           *BUFFER_ID,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{
   if(kshd.partition_mode == POK_PARTITION_MODE_NORMAL) {
      // Cannot create buffer in NORMAL mode
      *RETURN_CODE = INVALID_MODE;
      return;
   }


   if(find_buffer(BUFFER_NAME) != NULL) {
      // Buffer with given name already exists.
      *RETURN_CODE = NO_ACTION;
      return;
   }

   if(nbuffers_used == arinc_config_nbuffers) {
      // Too many buffers
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   if((MAX_MESSAGE_SIZE <= 0)
      || (MAX_MESSAGE_SIZE > SYSTEM_LIMIT_MESSAGE_SIZE)) {
      // Message size is non-positive or too high.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if((MAX_NB_MESSAGE <= 0)
      || (MAX_NB_MESSAGE > SYSTEM_LIMIT_NUMBER_OF_MESSAGES)) {
      // Number of messages is non-positive or too high.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if((QUEUING_DISCIPLINE != FIFO) && (QUEUING_DISCIPLINE != PRIORITY)) {
      // Incorrect discipline value.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }


   arinc_buffer_t* buffer = &arinc_buffers[nbuffers_used];

   buffer->max_message_size = MAX_MESSAGE_SIZE;
   // Optimize messages for copiing.
   buffer->message_stride = ALIGN(buffer->max_message_size, __alignof__(int32_t));
   buffer->max_nb_message = MAX_NB_MESSAGE;

   arinc_allocator_state astate = arinc_allocator_get_state();

   buffer->messages = arinc_alloc(buffer->message_stride * buffer->max_nb_message, __alignof__(int32_t));
   buffer->messages_size = arinc_alloc(
       (size_t)buffer->max_nb_message * sizeof(*buffer->messages_size),
      __alignof__(*buffer->messages_size));

   if((buffer->messages == NULL) || (buffer->messages_size == NULL))
   {
      // Failed to allocate message queue.
      arinc_allocator_reset_state(astate);
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   // The name may be shorter than MAX_NAME_LENGTH: don't read past its end.
   (void) strncpy(buffer->buffer_name, BUFFER_NAME, MAX_NAME_LENGTH);
   msection_init(&buffer->section);
   msection_wq_init(&buffer->process_queue);
   buffer->base_offset = 0;
   buffer->discipline = QUEUING_DISCIPLINE;

   *BUFFER_ID = nbuffers_used + 1U;// Avoid 0 value.

   nbuffers_used++;

   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `SEND_BUFFER`
 *
 * @description
 * Sends a message to a buffer (ARINC 653 service): to the first waiting
 * receiver, or into the ring; if the buffer is full, the process waits up to a
 * time-out.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BUFFER_ID` | `BUFFER_ID_TYPE` | [in] The buffer. |
 * | `MESSAGE_ADDR` | `const APEX_BYTE *` | [in] The message. |
 * | `LENGTH` | `MESSAGE_SIZE_TYPE` | [in] Its length. |
 * | `TIME_OUT` | `SYSTEM_TIME_TYPE` | [in] The time-out (0: no wait). |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, NOT_AVAILABLE, INVALID_MODE (waiting isn't allowed) or TIMED_OUT. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_buffers` | `arinc_buffer_t *` | The buffers of the partition |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nbuffers_used` | `size_t` | The number of buffers created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffer` | `arinc_buffer_t *` | The buffer |
 * | `dst` | `char *` | Its buffer |
 * | `index` | `MESSAGE_RANGE_TYPE` | The index of the new message |
 * | `message_dest` | `char *` | The new message |
 * | `t` | `pok_thread_id_t` | The current process |
 * | `t_awoken` | `pok_thread_id_t` | The receiver released |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `BUFFER_ID` is less than or equal to `0` or `(size_t)BUFFER_ID` is
 *    greater than `nbuffers_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `buffer` to the address of `arinc_buffers[BUFFER_ID - 1]`.
 * 3. If `LENGTH` is less than or equal to `0` or `LENGTH` is greater than
 *    `buffer->max_message_size`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 4. Call `msection_enter()` with the address of `buffer->section`.
 * 5. If `buffer->nb_message` is less than `buffer->max_nb_message`:
 *    1. If
 *       `msection_wq_notify(&buffer->section, &buffer->process_queue, FALSE)`
 *       is equal to `POK_ERRNO_OK`:
 *       1. Set `t_awoken` to `buffer->process_queue.first`.
 *       2. Set `dst` to `kshd.tshd[t_awoken].wq_buffer.dst`.
 *       3. Call `memcpy()` with the arguments `dst`, `MESSAGE_ADDR` and
 *          `LENGTH`.
 *       4. Set `kshd.tshd[t_awoken].wq_len` to `LENGTH`.
 *       5. Call `msection_wq_del()` with the arguments the address of
 *          `buffer->process_queue` and `t_awoken`.
 *    2. Otherwise:
 *       1. Call `message_index()` with the arguments `buffer` and
 *          `buffer->nb_message`, and store the result in `index`.
 *       2. Call `message_at()` with the arguments `buffer` and `index`, and
 *          store the result in `message_dest`.
 *       3. Call `memcpy()` with the arguments `message_dest`, `MESSAGE_ADDR`
 *          and `LENGTH`.
 *       4. Set `buffer->messages_size[index]` to `LENGTH`.
 *       5. Increment `buffer->nb_message`.
 *    3. Set `*RETURN_CODE` to `NO_ERROR`.
 * 6. Otherwise, if `TIME_OUT` is equal to `0`:
 *    1. Set `*RETURN_CODE` to `NOT_AVAILABLE`.
 * 7. Otherwise:
 *    1. Set `t` to `kshd.current_thread_id`.
 *    2. Set `kshd.tshd[t].wq_buffer.src` to `MESSAGE_ADDR`.
 *    3. Set `kshd.tshd[t].wq_len` to `LENGTH`.
 *    4. Call `arinc_process_queue_add_common()` with the arguments the address
 *       of `buffer->process_queue` and `buffer->discipline`.
 *    5. Depending on `msection_wait(&buffer->section, TIME_OUT)`:
 *       1. Case `POK_ERRNO_OK`:
 *          1. Set `*RETURN_CODE` to `NO_ERROR`.
 *          2. Leave the switch.
 *       2. Case `POK_ERRNO_MODE` or `POK_ERRNO_CANCELLED`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `buffer->process_queue` and `t`.
 *          2. Set `*RETURN_CODE` to `INVALID_MODE`.
 *          3. Leave the switch.
 *       3. Case `POK_ERRNO_TIMEOUT`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `buffer->process_queue` and `t`.
 *          2. Set `*RETURN_CODE` to `TIMED_OUT`.
 *          3. Leave the switch.
 *       4. Otherwise (default):
 *          1. Check that `FALSE` holds (assert_os: stop otherwise).
 *          2. Leave the switch.
 * 8. Call `msection_leave()` with the address of `buffer->section`.
 *
 * @calls
 * - `msection_enter()`
 * - `msection_wq_notify()`
 * - `memcpy()`
 * - `msection_wq_del()`
 * - `message_index()`
 * - `message_at()`
 * - `arinc_process_queue_add_common()`
 * - `msection_wait()`
 * - `assertion_os_fail()`
 * - `msection_leave()`
 */
void SEND_BUFFER (
       /*in */ BUFFER_ID_TYPE           BUFFER_ID,
       /*in */ const APEX_BYTE *       MESSAGE_ADDR,       /* by reference */
       /*in */ MESSAGE_SIZE_TYPE        LENGTH,
       /*in */ SYSTEM_TIME_TYPE         TIME_OUT,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{
   if ((BUFFER_ID <= 0) || ((size_t)BUFFER_ID > nbuffers_used)) {
      // Incorrect buffer identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_buffer_t* buffer = &arinc_buffers[BUFFER_ID - 1];

   if((LENGTH <= 0) || (LENGTH > buffer->max_message_size)) {
      // LENGTH is non-positive or too big.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   msection_enter(&buffer->section);

   if(buffer->nb_message < buffer->max_nb_message)
   {
      // Buffer is not full.
      if(msection_wq_notify(&buffer->section,
         &buffer->process_queue, FALSE) == POK_ERRNO_OK) {
         // There are waiters on buffer. We have already awoken the first of them.
         pok_thread_id_t t_awoken = buffer->process_queue.first;

         char* dst = kshd.tshd[t_awoken].wq_buffer.dst;

         (void) memcpy(dst, MESSAGE_ADDR, LENGTH);
         kshd.tshd[t_awoken].wq_len = LENGTH;

         msection_wq_del(&buffer->process_queue, t_awoken);
      }
      else {
         // No processes are waiting on buffer.
         MESSAGE_RANGE_TYPE index = message_index(buffer,
            buffer->nb_message);

         char* message_dest = message_at(buffer, index);

         (void) memcpy(message_dest, MESSAGE_ADDR, LENGTH);
         buffer->messages_size[index] = LENGTH;

         buffer->nb_message++;
      }

      *RETURN_CODE = NO_ERROR;
   }
   else if(TIME_OUT == 0) {
      // Buffer is full but waiting is not requested.
      *RETURN_CODE = NOT_AVAILABLE;
   }
   else {
      // Buffer is full and waiting is *requested* by the caller.
      // (whether waiting is *allowed* will be checked by the kernel.)
      pok_thread_id_t t = kshd.current_thread_id;

      kshd.tshd[t].wq_buffer.src = MESSAGE_ADDR;
      kshd.tshd[t].wq_len = LENGTH;

      arinc_process_queue_add_common(&buffer->process_queue, buffer->discipline);

      switch(msection_wait(&buffer->section, TIME_OUT))
      {
      case POK_ERRNO_OK:
         // Message is already copied into the buffer by the notifier.
         *RETURN_CODE = NO_ERROR;
         break;
      case POK_ERRNO_MODE: // Waiting is not allowed
      case POK_ERRNO_CANCELLED: // Thread has been STOP()-ed or [IPPC] server thread has been cancelled.
         msection_wq_del(&buffer->process_queue, t);
         *RETURN_CODE = INVALID_MODE;
         break;
      case POK_ERRNO_TIMEOUT:
         // Timeout
         msection_wq_del(&buffer->process_queue, t);
         *RETURN_CODE = TIMED_OUT;
         break;
      default:
         assert_os(FALSE);
         break;
      }
   }

   msection_leave(&buffer->section);
}

/**
 * @funcname `RECEIVE_BUFFER`
 *
 * @description
 * Receives the first message of a buffer (ARINC 653 service); the first waiting
 * sender's message then goes into the ring. If the buffer is empty, the process
 * waits up to a time-out.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BUFFER_ID` | `BUFFER_ID_TYPE` | [in] The buffer. |
 * | `TIME_OUT` | `SYSTEM_TIME_TYPE` | [in] The time-out (0: no wait). |
 * | `MESSAGE_ADDR` | `MESSAGE_ADDR_TYPE` | [out] Where the message is stored. |
 * | `LENGTH` | `MESSAGE_SIZE_TYPE *` | [out] Where its length is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, NOT_AVAILABLE, INVALID_MODE (waiting isn't allowed) or TIMED_OUT. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_buffers` | `arinc_buffer_t *` | The buffers of the partition |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nbuffers_used` | `size_t` | The number of buffers created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffer` | `arinc_buffer_t *` | The buffer |
 * | `index` | `MESSAGE_RANGE_TYPE` | The index of the first message |
 * | `len` | `MESSAGE_SIZE_TYPE` | Its length |
 * | `src` | `const char *` | The first message |
 * | `t` | `pok_thread_id_t` | The current process |
 * | `t_awoken` | `pok_thread_id_t` | The sender released |
 * | `w_dest` | `char *` | Its place in the ring |
 * | `w_index` | `MESSAGE_RANGE_TYPE` | The index of its message |
 * | `w_len` | `MESSAGE_SIZE_TYPE` | Its length |
 * | `w_src` | `const char *` | Its message |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `BUFFER_ID` is less than or equal to `0` or `(size_t)BUFFER_ID` is
 *    greater than `nbuffers_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `buffer` to the address of `arinc_buffers[BUFFER_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `buffer->section`.
 * 4. If `buffer->nb_message` is greater than `0`:
 *    1. Call `message_index()` with the arguments `buffer` and `0`, and store
 *       the result in `index`.
 *    2. Call `message_at()` with the arguments `buffer` and `index`, and store
 *       the result in `src`.
 *    3. Set `len` to `buffer->messages_size[index]`.
 *    4. Call `memcpy()` with the arguments `MESSAGE_ADDR`, `src` and `len`.
 *    5. Call `message_index()` with the arguments `buffer` and `1`, and store
 *       the result in `buffer->base_offset`.
 *    6. Decrement `buffer->nb_message`.
 *    7. Set `*RETURN_CODE` to `NO_ERROR`.
 *    8. Set `*LENGTH` to `len`.
 *    9. If
 *       `msection_wq_notify(&buffer->section, &buffer->process_queue, FALSE)`
 *       is equal to `POK_ERRNO_OK`:
 *       1. Set `t_awoken` to `buffer->process_queue.first`.
 *       2. Call `message_index()` with the arguments `buffer` and
 *          `buffer->nb_message`, and store the result in `w_index`.
 *       3. Call `message_at()` with the arguments `buffer` and `w_index`, and
 *          store the result in `w_dest`.
 *       4. Set `w_src` to `kshd.tshd[t_awoken].wq_buffer.src`.
 *       5. Set `w_len` to `kshd.tshd[t_awoken].wq_len`.
 *       6. Call `memcpy()` with the arguments `w_dest`, `w_src` and `w_len`.
 *       7. Set `buffer->messages_size[w_index]` to `w_len`.
 *       8. Increment `buffer->nb_message`.
 *       9. Call `msection_wq_del()` with the arguments the address of
 *          `buffer->process_queue` and `t_awoken`.
 * 5. Otherwise, if `TIME_OUT` is equal to `0`:
 *    1. Set `*LENGTH` to `0`.
 *    2. Set `*RETURN_CODE` to `NOT_AVAILABLE`.
 * 6. Otherwise:
 *    1. Set `t` to `kshd.current_thread_id`.
 *    2. Set `kshd.tshd[t].wq_buffer.dst` to `MESSAGE_ADDR`.
 *    3. Call `arinc_process_queue_add_common()` with the arguments the address
 *       of `buffer->process_queue` and `buffer->discipline`.
 *    4. Depending on `msection_wait(&buffer->section, TIME_OUT)`:
 *       1. Case `POK_ERRNO_OK`:
 *          1. Set `*LENGTH` to `kshd.tshd[t].wq_len`.
 *          2. Set `*RETURN_CODE` to `NO_ERROR`.
 *          3. Leave the switch.
 *       2. Case `POK_ERRNO_MODE` or `POK_ERRNO_CANCELLED`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `buffer->process_queue` and `t`.
 *          2. Set `*LENGTH` to `0`.
 *          3. Set `*RETURN_CODE` to `INVALID_MODE`.
 *          4. Leave the switch.
 *       3. Case `POK_ERRNO_TIMEOUT`:
 *          1. Call `msection_wq_del()` with the arguments the address of
 *             `buffer->process_queue` and `t`.
 *          2. Set `*LENGTH` to `0`.
 *          3. Set `*RETURN_CODE` to `TIMED_OUT`.
 *          4. Leave the switch.
 *       4. Otherwise (default):
 *          1. Check that `FALSE` holds (assert_os: stop otherwise).
 *          2. Leave the switch.
 * 7. Call `msection_leave()` with the address of `buffer->section`.
 *
 * @calls
 * - `msection_enter()`
 * - `message_index()`
 * - `message_at()`
 * - `memcpy()`
 * - `msection_wq_notify()`
 * - `msection_wq_del()`
 * - `arinc_process_queue_add_common()`
 * - `msection_wait()`
 * - `assertion_os_fail()`
 * - `msection_leave()`
 */
void RECEIVE_BUFFER (
       /*in */ BUFFER_ID_TYPE           BUFFER_ID,
       /*in */ SYSTEM_TIME_TYPE         TIME_OUT,
       /*out*/ MESSAGE_ADDR_TYPE        MESSAGE_ADDR,
       /*out*/ MESSAGE_SIZE_TYPE        *LENGTH,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{
   if ((BUFFER_ID <= 0) || ((size_t)BUFFER_ID > nbuffers_used)) {
      // Incorrect buffer identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_buffer_t* buffer = &arinc_buffers[BUFFER_ID - 1];

   msection_enter(&buffer->section);

   if(buffer->nb_message > 0)
   {
      // Buffer is not empty.
      MESSAGE_RANGE_TYPE index = message_index(buffer, 0);
      const char* src = message_at(buffer, index);
      MESSAGE_SIZE_TYPE len = buffer->messages_size[index];

      (void) memcpy(MESSAGE_ADDR, src, len);

      buffer->base_offset = message_index(buffer, 1);
      buffer->nb_message--;

      *RETURN_CODE = NO_ERROR;
      *LENGTH = len;

      if(msection_wq_notify(&buffer->section,
         &buffer->process_queue, FALSE) == POK_ERRNO_OK) {
         // There are waiters on buffer. We have already awoken the first of them.
         pok_thread_id_t t_awoken = buffer->process_queue.first;

         MESSAGE_RANGE_TYPE w_index = message_index(buffer, buffer->nb_message);
         char* w_dest = message_at(buffer, w_index);

         const char* w_src = kshd.tshd[t_awoken].wq_buffer.src;
         MESSAGE_SIZE_TYPE w_len = kshd.tshd[t_awoken].wq_len;

         (void) memcpy(w_dest, w_src, w_len);
         buffer->messages_size[w_index] = w_len;

         buffer->nb_message++;

         msection_wq_del(&buffer->process_queue, t_awoken);
      }
   }
   else if(TIME_OUT == 0) {
      // Buffer is empty but waiting is not requested.
      *LENGTH = 0;
      *RETURN_CODE = NOT_AVAILABLE;
   }
   else {
      // Buffer is empty and waiting is *requested* by the caller.
      // (whether waiting is *allowed* will be checked by the kernel.)
      pok_thread_id_t t = kshd.current_thread_id;

      kshd.tshd[t].wq_buffer.dst = MESSAGE_ADDR;

      arinc_process_queue_add_common(&buffer->process_queue, buffer->discipline);

      switch(msection_wait(&buffer->section, TIME_OUT))
      {
      case POK_ERRNO_OK:
         // Message is already copied from the buffer by the notifier.
         *LENGTH = kshd.tshd[t].wq_len;
         *RETURN_CODE = NO_ERROR;
         break;
      case POK_ERRNO_MODE: // Waiting is not allowed
      case POK_ERRNO_CANCELLED: // Thread has been STOP()-ed or [IPPC] server thread has been cancelled.
         msection_wq_del(&buffer->process_queue, t);
         *LENGTH = 0;
         *RETURN_CODE = INVALID_MODE;
         break;
      case POK_ERRNO_TIMEOUT:
         // Timeout
         msection_wq_del(&buffer->process_queue, t);
         *LENGTH = 0;
         *RETURN_CODE = TIMED_OUT;
         break;
      default:
         assert_os(FALSE);
         break;
      }
   }

   msection_leave(&buffer->section);
}

/**
 * @funcname `GET_BUFFER_ID`
 *
 * @description
 * Returns the ID of a buffer by name (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BUFFER_NAME` | `const BUFFER_NAME_TYPE` | [in] The name. |
 * | `BUFFER_ID` | `BUFFER_ID_TYPE *` | [out] Where the ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_CONFIG. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_buffers` | `arinc_buffer_t *` | The buffers of the partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffer` | `arinc_buffer_t *` | The buffer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `find_buffer()` with the argument `BUFFER_NAME`, and store the result
 *    in `buffer`.
 * 2. If `buffer` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 3. Set `*BUFFER_ID` to `(buffer - arinc_buffers) + 1`.
 * 4. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_buffer()`
 */
void GET_BUFFER_ID (
       /*in */ const BUFFER_NAME_TYPE         BUFFER_NAME,
       /*out*/ BUFFER_ID_TYPE           *BUFFER_ID,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{

   arinc_buffer_t* buffer = find_buffer(BUFFER_NAME);
   if(buffer == NULL) {
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   *BUFFER_ID = (buffer - arinc_buffers) + 1;
   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `GET_BUFFER_STATUS`
 *
 * @description
 * Returns the status of a buffer (ARINC 653 service): the number of messages,
 * the maximum number and size, and the number of waiting processes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `BUFFER_ID` | `BUFFER_ID_TYPE` | [in] The buffer. |
 * | `BUFFER_STATUS` | `BUFFER_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_buffers` | `arinc_buffer_t *` | The buffers of the partition |
 * | `nbuffers_used` | `size_t` | The number of buffers created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `buffer` | `arinc_buffer_t *` | The buffer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `BUFFER_ID` is less than or equal to `0` or `(size_t)BUFFER_ID` is
 *    greater than `nbuffers_used`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `buffer` to the address of `arinc_buffers[BUFFER_ID - 1]`.
 * 3. Call `msection_enter()` with the address of `buffer->section`.
 * 4. Set `BUFFER_STATUS->NB_MESSAGE` to `buffer->nb_message`.
 * 5. Set `BUFFER_STATUS->MAX_NB_MESSAGE` to `buffer->max_nb_message`.
 * 6. Set `BUFFER_STATUS->MAX_MESSAGE_SIZE` to `buffer->max_message_size`.
 * 7. Call `msection_wq_size()` with the arguments the address of
 *    `buffer->section` and the address of `buffer->process_queue`, and store
 *    the result in `BUFFER_STATUS->WAITING_PROCESSES`.
 * 8. Call `msection_leave()` with the address of `buffer->section`.
 * 9. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `msection_enter()`
 * - `msection_wq_size()`
 * - `msection_leave()`
 */
void GET_BUFFER_STATUS (
       /*in */ BUFFER_ID_TYPE           BUFFER_ID,
       /*out*/ BUFFER_STATUS_TYPE       *BUFFER_STATUS,
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE )
{
   if ((BUFFER_ID <= 0) || ((size_t)BUFFER_ID > nbuffers_used)) {
      // Incorrect buffer identificator.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   arinc_buffer_t* buffer = &arinc_buffers[BUFFER_ID - 1];

   msection_enter(&buffer->section);

   BUFFER_STATUS->NB_MESSAGE = buffer->nb_message;
   BUFFER_STATUS->MAX_NB_MESSAGE = buffer->max_nb_message;
   BUFFER_STATUS->MAX_MESSAGE_SIZE = buffer->max_message_size;
   BUFFER_STATUS->WAITING_PROCESSES = msection_wq_size(&buffer->section,
      &buffer->process_queue);

   msection_leave(&buffer->section);

   *RETURN_CODE = NO_ERROR;
}

#endif
