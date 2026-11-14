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
 * Modified by Kirill Brazhnikov, 2026-11-01: derived from
 * libpok/arinc653/semaphore.c; implements the ARINC 653P1-4 mutex
 * services CREATE_MUTEX, ACQUIRE_MUTEX, RELEASE_MUTEX, RESET_MUTEX,
 * GET_MUTEX_ID, GET_MUTEX_STATUS and GET_PROCESS_MUTEX_STATE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; operands of the
 * same essential type; if-else-if chains end with else.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized; deviations are documented where a
 * guideline can't be followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: name
 * parameters are const (they are only read), so callers can pass string
 * literals.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: msection_t is used by its typedef
 * name.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * ARINC 653 mutexes with the priority ceiling protocol.
 *
 * A mutex has a priority (its ceiling). A process that acquires it runs
 * at that priority until it releases the mutex, then gets back its
 * previous (retained) priority. A process owns at most one mutex at a
 * time, but may acquire it again (up to MAX_LOCK_LEVEL times); it's
 * released when the lock count returns to zero. When it's released, the
 * first waiting process (FIFO or PRIORITY order) becomes the owner.
 *
 * STOP of the owner doesn't release the mutex: RESET_MUTEX does, typically
 * from the error handler.
 *
 * All mutex state is protected by one msection, which also provides
 * waiting with a time-out (like the other intra-partition objects).
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_MUTEX

#include "mutex.h"

#include <core/dependencies.h>
#include <core/thread.h>
#include <core/partition.h>
#include <arinc653/types.h>
#include <arinc653/process.h>
#include <arinc653/mutex.h>
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

static size_t nmutexes_used = 0;

/* Protects all mutexes and their wait queues. */
static msection_t mutex_section;

/**
 * @funcname `arinc_mutex_init`
 *
 * @description
 * Initializes the mutexes at partition start: none is created, and the section
 * that protects them.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex_section` | `msection_t` | Protects all mutexes and their wait queues |
 * | `nmutexes_used` | `size_t` | The number of mutexes created |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `msection_init()` with the address of `mutex_section`.
 * 2. Set `nmutexes_used` to `0`.
 *
 * @calls
 * - `msection_init()`
 */
void arinc_mutex_init(void)
{
   msection_init(&mutex_section);
   nmutexes_used = 0;
}

/**
 * @funcname `find_mutex`
 *
 * @description
 * Finds a mutex by name (case insensitive).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char *` | [in] The name. |
 *
 * @return The mutex, or NULL if there is none.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_mutexes` | `arinc_mutex_t *` | The mutexes of the partition |
 * | `nmutexes_used` | `size_t` | The number of mutexes created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of a mutex |
 * | `mutex` | `arinc_mutex_t *` | The mutex |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0`; then while `i` is less than `nmutexes_used`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `mutex` to the address of `arinc_mutexes[i]`.
 *    2. If `strncasecmp(mutex->mutex_name, name, MAX_NAME_LENGTH)` is equal to
 *       `0`:
 *       1. Return `mutex`.
 * 2. Return `NULL`.
 *
 * @calls
 * - `strncasecmp()`
 */
static arinc_mutex_t* find_mutex(const char* name)
{
   for(size_t i = 0; i < nmutexes_used; i++)
   {
      arinc_mutex_t* mutex = &arinc_mutexes[i];
      if(strncasecmp(mutex->mutex_name, name, MAX_NAME_LENGTH) == 0) {
         return mutex;
      }
   }

   return NULL;
}

/**
 * @funcname `get_mutex`
 *
 * @description
 * Returns a mutex by ID. IDs start from 1, so NO_MUTEX_OWNED and
 * PREEMPTION_LOCK_MUTEX are never valid here.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex_id` | `MUTEX_ID_TYPE` | [in] The ID. |
 *
 * @return The mutex, or NULL.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_mutexes` | `arinc_mutex_t *` | The mutexes of the partition |
 * | `nmutexes_used` | `size_t` | The number of mutexes created |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `mutex_id` is less than or equal to `0` or `mutex_id` is greater than
 *    `(MUTEX_ID_TYPE)nmutexes_used`:
 *    1. Return `NULL`.
 * 2. Return the address of `arinc_mutexes[mutex_id - 1]`.
 *
 * @calls
 * None.
 */
static arinc_mutex_t* get_mutex(MUTEX_ID_TYPE mutex_id)
{
   if((mutex_id <= 0) || (mutex_id > (MUTEX_ID_TYPE)nmutexes_used)) {
      return NULL;
   }

   return &arinc_mutexes[mutex_id - 1];
}

/**
 * @funcname `mutex_id_of`
 *
 * @description
 * Returns the ID of a mutex.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex` | `const arinc_mutex_t *` | [in] The mutex. |
 *
 * @return The ID (from 1).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_mutexes` | `arinc_mutex_t *` | The mutexes of the partition |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `(mutex - arinc_mutexes) + 1`.
 *
 * @calls
 * None.
 */
static MUTEX_ID_TYPE mutex_id_of(const arinc_mutex_t* mutex)
{
   return (mutex - arinc_mutexes) + 1;
}

/**
 * @funcname `find_owned`
 *
 * @description
 * Returns the mutex owned by a thread. Called inside mutex_section.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_id_t` | [in] The thread. |
 *
 * @return The mutex, or NULL.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_mutexes` | `arinc_mutex_t *` | The mutexes of the partition |
 * | `nmutexes_used` | `size_t` | The number of mutexes created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of a mutex |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0`; then while `i` is less than `nmutexes_used`, repeat (after
 *    each pass: increment `i`):
 *    1. If `arinc_mutexes[i].owner` is equal to `thread`:
 *       1. Return the address of `arinc_mutexes[i]`.
 * 2. Return `NULL`.
 *
 * @calls
 * None.
 */
static arinc_mutex_t* find_owned(pok_thread_id_t thread)
{
   for(size_t i = 0; i < nmutexes_used; i++)
   {
      if(arinc_mutexes[i].owner == thread) {
         return &arinc_mutexes[i];
      }
   }

   return NULL;
}

/**
 * @funcname `process_id_of`
 *
 * @description
 * Returns the ARINC 653 process ID of a kernel thread.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_id_t` | [in] The thread. |
 *
 * @return The process ID (MAIN_PROCESS_ID for the main thread).
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
 * 1. Return `MAIN_PROCESS_ID` if `thread` is equal to `MAIN_THREAD_ID`,
 *    otherwise `((PROCESS_ID_TYPE)thread + 1)`.
 *
 * @calls
 * None.
 */
static PROCESS_ID_TYPE process_id_of(pok_thread_id_t thread)
{
   return (thread == MAIN_THREAD_ID) ? MAIN_PROCESS_ID
       : ((PROCESS_ID_TYPE)thread + 1);
}

/**
 * @funcname `thread_of`
 *
 * @description
 * Returns the kernel thread of a process: MAIN_PROCESS_ID or the ID of a
 * created process (not the error handler).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `process_id` | `PROCESS_ID_TYPE` | [in] The process ID. |
 * | `thread` | `pok_thread_id_t *` | [out] Where the thread is stored. |
 *
 * @return FALSE for any other ID.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `entry` | `void *` | Its entry (not used) |
 * | `name` | `char[30]` | The thread's name (not used) |
 * | `status` | `pok_thread_status_t` | Its status (not used) |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `process_id` is equal to `MAIN_PROCESS_ID`:
 *    1. Set `*thread` to `MAIN_THREAD_ID`.
 *    2. Return `TRUE`.
 * 2. If `process_id` is less than `1` or `process_id` is greater than
 *    `(PROCESS_ID_TYPE)kshd.max_n_threads`:
 *    1. Return `FALSE`.
 * 3. If `pok_thread_get_status(process_id - 1, name, &entry, &status)` is not
 *    equal to `POK_ERRNO_OK`:
 *    1. Return `FALSE`.
 * 4. Set `*thread` to `process_id - 1`.
 * 5. Return `TRUE`.
 *
 * @calls
 * - `pok_thread_get_status()`
 */
static pok_bool_t thread_of(PROCESS_ID_TYPE process_id, pok_thread_id_t* thread)
{
   char name[MAX_NAME_LENGTH];
   void* entry;
   pok_thread_status_t status;

   if(process_id == MAIN_PROCESS_ID)
   {
      *thread = MAIN_THREAD_ID;
      return TRUE;
   }

   // Check the range before narrowing to the 8-bit kernel thread ID.
   if((process_id < 1) || (process_id > (PROCESS_ID_TYPE)kshd.max_n_threads)) {
      return FALSE;
   }

   // The kernel rejects the main thread, the error handler and threads
   // which aren't created yet.
   if(pok_thread_get_status(process_id - 1, name, &entry,
                            &status) != POK_ERRNO_OK) {
      return FALSE;
   }

   *thread = process_id - 1;
   return TRUE;
}

/**
 * @funcname `set_thread_priority`
 *
 * @description
 * Sets the current priority of a thread. The main process has no ARINC priority
 * and runs alone during initialization, so it's left as is. A stopped thread
 * gets its base priority when it's started again.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_id_t` | [in] The thread. |
 * | `priority` | `PRIORITY_TYPE` | [in] The priority. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `thread` is equal to `MAIN_THREAD_ID` or
 *    `(PRIORITY_TYPE)kshd.tshd[thread].priority` is equal to `priority`:
 *    1. Return.
 * 2. Call `pok_thread_set_priority()` with the arguments `thread` and
 *    `priority`.
 *
 * @calls
 * - `pok_thread_set_priority()`
 */
static void set_thread_priority(pok_thread_id_t thread, PRIORITY_TYPE priority)
{
   if((thread == MAIN_THREAD_ID)
      || ((PRIORITY_TYPE)kshd.tshd[thread].priority == priority)) {
      return;
   }

   // Fails only for a stopped thread, which gets its base priority when
   // it's started again.
   (void) pok_thread_set_priority(thread, priority);
}

/**
 * @funcname `take_mutex`
 *
 * @description
 * Makes a thread the owner of an available mutex: it runs at the mutex's
 * priority, its own is retained. Called inside mutex_section.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex` | `arinc_mutex_t *` | [in,out] The mutex. |
 * | `thread` | `pok_thread_id_t` | [in] The thread. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `mutex->owner` to `thread`.
 * 2. Set `mutex->lock_count` to `1`.
 * 3. Set `mutex->retained_priority` to `kshd.tshd[thread].priority`.
 * 4. Call `set_thread_priority()` with the arguments `thread` and
 *    `mutex->priority`.
 *
 * @calls
 * - `set_thread_priority()`
 */
static void take_mutex(arinc_mutex_t* mutex, pok_thread_id_t thread)
{
   mutex->owner = thread;
   mutex->lock_count = 1;
   mutex->retained_priority = kshd.tshd[thread].priority;

   set_thread_priority(thread, mutex->priority);
}

/**
 * @funcname `release_mutex`
 *
 * @description
 * Releases a mutex completely: restores the owner's priority and passes the
 * mutex to the first waiting process, if any. Called inside mutex_section.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex` | `arinc_mutex_t *` | [in,out] The mutex. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex_section` | `msection_t` | Protects all mutexes and their wait queues |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `t_awoken` | `pok_thread_id_t` | The waiting process released, the new owner |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `set_thread_priority()` with the arguments `mutex->owner` and
 *    `mutex->retained_priority`.
 * 2. Set `mutex->owner` to `JET_THREAD_ID_NONE`.
 * 3. Set `mutex->lock_count` to `0`.
 * 4. If `msection_wq_notify(&mutex_section, &mutex->process_queue, FALSE)` is
 *    equal to `POK_ERRNO_OK`:
 *    1. Set `t_awoken` to `mutex->process_queue.first`.
 *    2. Call `msection_wq_del()` with the arguments the address of
 *       `mutex->process_queue` and `t_awoken`.
 *    3. Call `take_mutex()` with the arguments `mutex` and `t_awoken`.
 *
 * @calls
 * - `set_thread_priority()`
 * - `msection_wq_notify()`
 * - `msection_wq_del()`
 * - `take_mutex()`
 */
static void release_mutex(arinc_mutex_t* mutex)
{
   set_thread_priority(mutex->owner, mutex->retained_priority);

   mutex->owner = JET_THREAD_ID_NONE;
   mutex->lock_count = 0;

   if(msection_wq_notify(&mutex_section, &mutex->process_queue, FALSE) == POK_ERRNO_OK)
   {
      // The first waiting process is awoken; it owns the mutex now and
      // continues after we leave the section.
      pok_thread_id_t t_awoken = mutex->process_queue.first;
      msection_wq_del(&mutex->process_queue, t_awoken);
      take_mutex(mutex, t_awoken);
   }
}

/**
 * @funcname `CREATE_MUTEX`
 *
 * @description
 * Creates a mutex (ARINC 653 service), not in NORMAL mode.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `MUTEX_NAME` | `const MUTEX_NAME_TYPE` | [in] The name. |
 * | `MUTEX_PRIORITY` | `PRIORITY_TYPE` | [in] The priority (ceiling). |
 * | `QUEUING_DISCIPLINE` | `QUEUING_DISCIPLINE_TYPE` | [in] The discipline of the waiting processes (FIFO or PRIORITY). |
 * | `MUTEX_ID` | `MUTEX_ID_TYPE *` | [out] Where its ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_CONFIG (too many), NO_ACTION (the name exists), INVALID_PARAM or INVALID_MODE. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_config_nmutexes` | `size_t` | The maximum number of mutexes (configuration) |
 * | `arinc_mutexes` | `arinc_mutex_t *` | The mutexes of the partition |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `nmutexes_used` | `size_t` | The number of mutexes created |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex` | `arinc_mutex_t *` | The new mutex |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `nmutexes_used` is equal to `arinc_config_nmutexes`:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 2. If `find_mutex(MUTEX_NAME)` is not NULL:
 *    1. Set `*RETURN_CODE` to `NO_ACTION`.
 *    2. Return.
 * 3. If `MUTEX_PRIORITY` is less than `MIN_PRIORITY_VALUE` or `MUTEX_PRIORITY`
 *    is greater than `MAX_PRIORITY_VALUE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 4. If `QUEUING_DISCIPLINE` is not equal to `FIFO` and `QUEUING_DISCIPLINE` is
 *    not equal to `PRIORITY`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 5. If `kshd.partition_mode` is equal to `POK_PARTITION_MODE_NORMAL`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 *    2. Return.
 * 6. Set `mutex` to the address of `arinc_mutexes[nmutexes_used]`.
 * 7. Call `strncpy()` with the arguments `mutex->mutex_name`, `MUTEX_NAME` and
 *    `MAX_NAME_LENGTH`.
 * 8. Set `mutex->priority` to `MUTEX_PRIORITY`.
 * 9. Set `mutex->discipline` to `QUEUING_DISCIPLINE`.
 * 10. Set `mutex->owner` to `JET_THREAD_ID_NONE`.
 * 11. Set `mutex->lock_count` to `0`.
 * 12. Set `mutex->retained_priority` to `0`.
 * 13. Call `msection_wq_init()` with the address of `mutex->process_queue`.
 * 14. Set `*MUTEX_ID` to `nmutexes_used + 1U`.
 * 15. Increment `nmutexes_used`.
 * 16. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_mutex()`
 * - `strncpy()`
 * - `msection_wq_init()`
 */
void CREATE_MUTEX (const MUTEX_NAME_TYPE MUTEX_NAME,
                   PRIORITY_TYPE MUTEX_PRIORITY,
                   QUEUING_DISCIPLINE_TYPE QUEUING_DISCIPLINE,
                   MUTEX_ID_TYPE *MUTEX_ID,
                   RETURN_CODE_TYPE *RETURN_CODE)
{
   if(nmutexes_used == arinc_config_nmutexes) {
      // Too many mutexes (ARINC653_Mutexes Count in the configuration).
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   if(find_mutex(MUTEX_NAME) != NULL) {
      *RETURN_CODE = NO_ACTION;
      return;
   }

   if((MUTEX_PRIORITY < MIN_PRIORITY_VALUE)
      || (MUTEX_PRIORITY > MAX_PRIORITY_VALUE)) {
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if((QUEUING_DISCIPLINE != FIFO) && (QUEUING_DISCIPLINE != PRIORITY)) {
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if(kshd.partition_mode == POK_PARTITION_MODE_NORMAL) {
      *RETURN_CODE = INVALID_MODE;
      return;
   }

   arinc_mutex_t* mutex = &arinc_mutexes[nmutexes_used];

   // Names may be shorter than MAX_NAME_LENGTH (NUL-terminated); don't
   // read past them.
   (void) strncpy(mutex->mutex_name, MUTEX_NAME, MAX_NAME_LENGTH);
   mutex->priority = MUTEX_PRIORITY;
   mutex->discipline = QUEUING_DISCIPLINE;
   mutex->owner = JET_THREAD_ID_NONE;
   mutex->lock_count = 0;
   mutex->retained_priority = 0;
   msection_wq_init(&mutex->process_queue);

   *MUTEX_ID = nmutexes_used + 1U; // Avoid 0 and the negative special IDs.

   nmutexes_used++;

   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `ACQUIRE_MUTEX`
 *
 * @description
 * Acquires a mutex (ARINC 653 service), or again if the process owns it; if
 * another process owns it, the process waits up to a time-out. A process owns
 * at most one mutex, the error handler none, and a process of a higher priority
 * than the mutex can't acquire it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `MUTEX_ID` | `MUTEX_ID_TYPE` | [in] The mutex. |
 * | `TIME_OUT` | `SYSTEM_TIME_TYPE` | [in] The time-out (0: no wait). |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, INVALID_MODE, INVALID_CONFIG (MAX_LOCK_LEVEL reached), NOT_AVAILABLE or TIMED_OUT. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `mutex_section` | `msection_t` | Protects all mutexes and their wait queues |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex` | `arinc_mutex_t *` | The mutex |
 * | `owned` | `arinc_mutex_t *` | The mutex it owns |
 * | `t` | `pok_thread_id_t` | The current process |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_mutex()` with the argument `MUTEX_ID`, and store the result in
 *    `mutex`.
 * 2. Set `t` to `kshd.current_thread_id`.
 * 3. If `mutex` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 4. If `TIME_OUT` is less than `0` and `TIME_OUT` is not equal to
 *    `INFINITE_TIME_VALUE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 5. Call `msection_enter()` with the address of `mutex_section`.
 * 6. Call `find_owned()` with the argument `t`, and store the result in
 *    `owned`.
 * 7. If `owned` is not NULL and `owned` is not equal to `mutex`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 * 8. Otherwise, if `t` is equal to `kshd.error_thread_id`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 * 9. Otherwise, if `t` is not equal to `MAIN_THREAD_ID` and
 *    `(PRIORITY_TYPE)kshd.tshd[t].priority` is greater than `mutex->priority`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 * 10. Otherwise, if `owned` is equal to `mutex`:
 *     1. If `mutex->lock_count` is equal to `MAX_LOCK_LEVEL`:
 *        1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *     2. Otherwise:
 *        1. Increment `mutex->lock_count`.
 *        2. Set `*RETURN_CODE` to `NO_ERROR`.
 * 11. Otherwise, if `mutex->owner` is equal to `JET_THREAD_ID_NONE`:
 *     1. Call `take_mutex()` with the arguments `mutex` and `t`.
 *     2. Set `*RETURN_CODE` to `NO_ERROR`.
 * 12. Otherwise, if `TIME_OUT` is equal to `0`:
 *     1. Set `*RETURN_CODE` to `NOT_AVAILABLE`.
 * 13. Otherwise:
 *     1. Call `arinc_process_queue_add_common()` with the arguments the address
 *        of `mutex->process_queue` and `mutex->discipline`.
 *     2. Depending on `msection_wait(&mutex_section, TIME_OUT)`:
 *        1. Case `POK_ERRNO_OK`:
 *           1. Check that `mutex->owner == t` holds (assert_os: stop
 *              otherwise).
 *           2. Set `*RETURN_CODE` to `NO_ERROR`.
 *           3. Leave the switch.
 *        2. Case `POK_ERRNO_MODE` or `POK_ERRNO_CANCELLED`:
 *           1. Call `msection_wq_del()` with the arguments the address of
 *              `mutex->process_queue` and `t`.
 *           2. Set `*RETURN_CODE` to `INVALID_MODE`.
 *           3. Leave the switch.
 *        3. Case `POK_ERRNO_TIMEOUT`:
 *           1. Call `msection_wq_del()` with the arguments the address of
 *              `mutex->process_queue` and `t`.
 *           2. Set `*RETURN_CODE` to `TIMED_OUT`.
 *           3. Leave the switch.
 *        4. Otherwise (default):
 *           1. Check that `FALSE` holds (assert_os: stop otherwise).
 *           2. Leave the switch.
 * 14. Call `msection_leave()` with the address of `mutex_section`.
 *
 * @calls
 * - `get_mutex()`
 * - `msection_enter()`
 * - `find_owned()`
 * - `take_mutex()`
 * - `arinc_process_queue_add_common()`
 * - `msection_wait()`
 * - `assertion_os_fail()`
 * - `msection_wq_del()`
 * - `msection_leave()`
 */
void ACQUIRE_MUTEX (MUTEX_ID_TYPE MUTEX_ID,
                    SYSTEM_TIME_TYPE TIME_OUT,
                    RETURN_CODE_TYPE *RETURN_CODE)
{
   arinc_mutex_t* mutex = get_mutex(MUTEX_ID);
   pok_thread_id_t t = kshd.current_thread_id;

   if(mutex == NULL) {
      // Not created, or PREEMPTION_LOCK_MUTEX.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if((TIME_OUT < 0) && (TIME_OUT != INFINITE_TIME_VALUE)) {
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   msection_enter(&mutex_section);

   arinc_mutex_t* owned = find_owned(t);

   if((owned != NULL) && (owned != mutex)) {
      // A process owns at most one mutex at a time.
      *RETURN_CODE = INVALID_MODE;
   }

   else if(t == kshd.error_thread_id) {
      // The error handler can't acquire mutexes.
      *RETURN_CODE = INVALID_MODE;
   }

   else if((t != MAIN_THREAD_ID)
           && ((PRIORITY_TYPE)kshd.tshd[t].priority > mutex->priority)) {
      // Priority ceiling: the process mustn't have a higher priority
      // than the mutex.
      *RETURN_CODE = INVALID_MODE;
   }

   else if(owned == mutex) {
      // Already ours: acquire it again.
      if(mutex->lock_count == MAX_LOCK_LEVEL) {
         *RETURN_CODE = INVALID_CONFIG;
      } else {
         mutex->lock_count++;
         *RETURN_CODE = NO_ERROR;
      }
   }

   else if(mutex->owner == JET_THREAD_ID_NONE) {
      take_mutex(mutex, t);
      *RETURN_CODE = NO_ERROR;
   }

   else if(TIME_OUT == 0) {
      // Owned by another process and waiting isn't requested.
      *RETURN_CODE = NOT_AVAILABLE;
   }

   else {
      // Owned by another process and waiting is *requested* by the caller.
      // (whether waiting is *allowed* will be checked by the kernel.)
      arinc_process_queue_add_common(&mutex->process_queue, mutex->discipline);

      switch(msection_wait(&mutex_section, TIME_OUT))
      {
      case POK_ERRNO_OK:
         // The releasing process has made us the owner.
         assert_os(mutex->owner == t);
         *RETURN_CODE = NO_ERROR;
         break;
      case POK_ERRNO_MODE: // Waiting is not allowed (e.g. preemption is locked)
      case POK_ERRNO_CANCELLED: // Process has been STOP()-ed.
         msection_wq_del(&mutex->process_queue, t);
         *RETURN_CODE = INVALID_MODE;
         break;
      case POK_ERRNO_TIMEOUT:
         msection_wq_del(&mutex->process_queue, t);
         *RETURN_CODE = TIMED_OUT;
         break;
      default:
         assert_os(FALSE);
         break;
      }
   }

   msection_leave(&mutex_section);
}

/**
 * @funcname `RELEASE_MUTEX`
 *
 * @description
 * Releases a mutex owned by the current process (ARINC 653 service): when the
 * lock count reaches zero, it goes to the first waiting process.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `MUTEX_ID` | `MUTEX_ID_TYPE` | [in] The mutex. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM or INVALID_MODE (not the owner). |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `mutex_section` | `msection_t` | Protects all mutexes and their wait queues |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex` | `arinc_mutex_t *` | The mutex |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_mutex()` with the argument `MUTEX_ID`, and store the result in
 *    `mutex`.
 * 2. If `mutex` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 3. Call `msection_enter()` with the address of `mutex_section`.
 * 4. If `mutex->owner` is not equal to `kshd.current_thread_id`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 * 5. Otherwise:
 *    1. If `--mutex->lock_count` is equal to `0`:
 *       1. Call `release_mutex()` with the argument `mutex`.
 *    2. Set `*RETURN_CODE` to `NO_ERROR`.
 * 6. Call `msection_leave()` with the address of `mutex_section`.
 *
 * @calls
 * - `get_mutex()`
 * - `msection_enter()`
 * - `release_mutex()`
 * - `msection_leave()`
 */
void RELEASE_MUTEX (MUTEX_ID_TYPE MUTEX_ID,
                    RETURN_CODE_TYPE *RETURN_CODE)
{
   arinc_mutex_t* mutex = get_mutex(MUTEX_ID);

   if(mutex == NULL) {
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   msection_enter(&mutex_section);

   if(mutex->owner != kshd.current_thread_id) {
      // Not owned by the current process.
      *RETURN_CODE = INVALID_MODE;
   }

   else {
      if(--mutex->lock_count == 0) {
         release_mutex(mutex);
      }

      *RETURN_CODE = NO_ERROR;
   }

   msection_leave(&mutex_section);
}

/**
 * @funcname `RESET_MUTEX`
 *
 * @description
 * Releases a mutex owned by a process, whatever the lock count (ARINC 653
 * service; STOP doesn't release a mutex: this does, typically from the error
 * handler).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `MUTEX_ID` | `MUTEX_ID_TYPE` | [in] The mutex. |
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The owner. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM or INVALID_MODE (not the owner). |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex_section` | `msection_t` | Protects all mutexes and their wait queues |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex` | `arinc_mutex_t *` | The mutex |
 * | `t` | `pok_thread_id_t` | The owner's thread |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_mutex()` with the argument `MUTEX_ID`, and store the result in
 *    `mutex`.
 * 2. If `mutex` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 3. If `thread_of(PROCESS_ID, &t)` is false:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 4. Call `msection_enter()` with the address of `mutex_section`.
 * 5. If `mutex->owner` is not equal to `t`:
 *    1. Set `*RETURN_CODE` to `INVALID_MODE`.
 * 6. Otherwise:
 *    1. Call `release_mutex()` with the argument `mutex`.
 *    2. Set `*RETURN_CODE` to `NO_ERROR`.
 * 7. Call `msection_leave()` with the address of `mutex_section`.
 *
 * @calls
 * - `get_mutex()`
 * - `thread_of()`
 * - `msection_enter()`
 * - `release_mutex()`
 * - `msection_leave()`
 */
void RESET_MUTEX (MUTEX_ID_TYPE MUTEX_ID,
                  PROCESS_ID_TYPE PROCESS_ID,
                  RETURN_CODE_TYPE *RETURN_CODE)
{
   arinc_mutex_t* mutex = get_mutex(MUTEX_ID);
   pok_thread_id_t t;

   if(mutex == NULL) {
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if(!thread_of(PROCESS_ID, &t)) {
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   msection_enter(&mutex_section);

   if(mutex->owner != t) {
      // The process doesn't own this mutex.
      *RETURN_CODE = INVALID_MODE;
   }

   else {
      // Release it whatever the lock count.
      release_mutex(mutex);
      *RETURN_CODE = NO_ERROR;
   }

   msection_leave(&mutex_section);
}

/**
 * @funcname `GET_MUTEX_ID`
 *
 * @description
 * Returns the ID of a mutex by name (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `MUTEX_NAME` | `const MUTEX_NAME_TYPE` | [in] The name. |
 * | `MUTEX_ID` | `MUTEX_ID_TYPE *` | [out] Where the ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_CONFIG. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex` | `arinc_mutex_t *` | The mutex |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `find_mutex()` with the argument `MUTEX_NAME`, and store the result
 *    in `mutex`.
 * 2. If `mutex` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
 * 3. Call `mutex_id_of()` with the argument `mutex`, and store the result in
 *    `*MUTEX_ID`.
 * 4. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `find_mutex()`
 * - `mutex_id_of()`
 */
void GET_MUTEX_ID (const MUTEX_NAME_TYPE MUTEX_NAME,
                   MUTEX_ID_TYPE *MUTEX_ID,
                   RETURN_CODE_TYPE *RETURN_CODE)
{
   arinc_mutex_t* mutex = find_mutex(MUTEX_NAME);

   if(mutex == NULL) {
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   *MUTEX_ID = mutex_id_of(mutex);
   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `GET_MUTEX_STATUS`
 *
 * @description
 * Returns the status of a mutex (ARINC 653 service): its owner and state,
 * priority, lock count and number of waiting processes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `MUTEX_ID` | `MUTEX_ID_TYPE` | [in] The mutex. |
 * | `MUTEX_STATUS` | `MUTEX_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex_section` | `msection_t` | Protects all mutexes and their wait queues |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex` | `arinc_mutex_t *` | The mutex |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_mutex()` with the argument `MUTEX_ID`, and store the result in
 *    `mutex`.
 * 2. If `mutex` is NULL:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 3. Call `msection_enter()` with the address of `mutex_section`.
 * 4. If `mutex->owner` is equal to `JET_THREAD_ID_NONE`:
 *    1. Set `MUTEX_STATUS->MUTEX_OWNER` to `NULL_PROCESS_ID`.
 *    2. Set `MUTEX_STATUS->MUTEX_STATE` to `AVAILABLE`.
 * 5. Otherwise:
 *    1. Call `process_id_of()` with the argument `mutex->owner`, and store the
 *       result in `MUTEX_STATUS->MUTEX_OWNER`.
 *    2. Set `MUTEX_STATUS->MUTEX_STATE` to `OWNED`.
 * 6. Set `MUTEX_STATUS->MUTEX_PRIORITY` to `mutex->priority`.
 * 7. Set `MUTEX_STATUS->LOCK_COUNT` to `mutex->lock_count`.
 * 8. Call `msection_wq_size()` with the arguments the address of
 *    `mutex_section` and the address of `mutex->process_queue`, and store the
 *    result in `MUTEX_STATUS->WAITING_PROCESSES`.
 * 9. Call `msection_leave()` with the address of `mutex_section`.
 * 10. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `get_mutex()`
 * - `msection_enter()`
 * - `process_id_of()`
 * - `msection_wq_size()`
 * - `msection_leave()`
 */
void GET_MUTEX_STATUS (MUTEX_ID_TYPE MUTEX_ID,
                       MUTEX_STATUS_TYPE *MUTEX_STATUS,
                       RETURN_CODE_TYPE *RETURN_CODE)
{
   arinc_mutex_t* mutex = get_mutex(MUTEX_ID);

   if(mutex == NULL) {
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   msection_enter(&mutex_section);

   if(mutex->owner == JET_THREAD_ID_NONE) {
      MUTEX_STATUS->MUTEX_OWNER = NULL_PROCESS_ID;
      MUTEX_STATUS->MUTEX_STATE = AVAILABLE;
   } else {
      MUTEX_STATUS->MUTEX_OWNER = process_id_of(mutex->owner);
      MUTEX_STATUS->MUTEX_STATE = OWNED;
   }
   MUTEX_STATUS->MUTEX_PRIORITY = mutex->priority;
   MUTEX_STATUS->LOCK_COUNT = mutex->lock_count;
   MUTEX_STATUS->WAITING_PROCESSES = msection_wq_size(&mutex_section,
      &mutex->process_queue);

   msection_leave(&mutex_section);

   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `GET_PROCESS_MUTEX_STATE`
 *
 * @description
 * Returns the mutex that a process owns (ARINC 653 service):
 * PREEMPTION_LOCK_MUTEX if it holds the preemption lock in NORMAL mode,
 * NO_MUTEX_OWNED if none.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PROCESS_ID` | `PROCESS_ID_TYPE` | [in] The process. |
 * | `MUTEX_ID` | `MUTEX_ID_TYPE *` | [out] Where the mutex ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_preemption_lock_holder` | `pok_thread_id_t` | The process that holds the preemption lock, or JET_THREAD_ID_NONE |
 * | `kshd` | `jet_kernel_shared_t` | The data that the kernel shares with the partition (current thread, threads, partition mode) |
 * | `mutex_section` | `msection_t` | Protects all mutexes and their wait queues |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `owned` | `arinc_mutex_t *` | The mutex it owns |
 * | `status` | `pok_partition_status_t` | The partition's status |
 * | `t` | `pok_thread_id_t` | The process's thread |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `thread_of(PROCESS_ID, &t)` is false:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. If `pok_current_partition_get_status(&status)` is not equal to
 *    `POK_ERRNO_OK`:
 *    1. Set `status.lock_level` to `0`.
 * 3. Call `msection_enter()` with the address of `mutex_section`.
 * 4. Call `find_owned()` with the argument `t`, and store the result in
 *    `owned`.
 * 5. If `owned` is not NULL:
 *    1. Call `mutex_id_of()` with the argument `owned`, and store the result in
 *       `*MUTEX_ID`.
 * 6. Otherwise, if `kshd.partition_mode` is equal to
 *    `POK_PARTITION_MODE_NORMAL` and `status.lock_level` is greater than `0`
 *    and `arinc_preemption_lock_holder` is equal to `t`:
 *    1. Set `*MUTEX_ID` to `PREEMPTION_LOCK_MUTEX`.
 * 7. Otherwise:
 *    1. Set `*MUTEX_ID` to `NO_MUTEX_OWNED`.
 * 8. Call `msection_leave()` with the address of `mutex_section`.
 * 9. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `thread_of()`
 * - `pok_current_partition_get_status()`
 * - `msection_enter()`
 * - `find_owned()`
 * - `mutex_id_of()`
 * - `msection_leave()`
 */
void GET_PROCESS_MUTEX_STATE (PROCESS_ID_TYPE PROCESS_ID,
                              MUTEX_ID_TYPE *MUTEX_ID,
                              RETURN_CODE_TYPE *RETURN_CODE)
{
   pok_thread_id_t t;
   pok_partition_status_t status;

   if(!thread_of(PROCESS_ID, &t)) {
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if(pok_current_partition_get_status(&status) != POK_ERRNO_OK) {
      status.lock_level = 0;     // treated as "no preemption lock"
   }

   msection_enter(&mutex_section);

   arinc_mutex_t* owned = find_owned(t);

   if(owned != NULL) {
      *MUTEX_ID = mutex_id_of(owned);
   } else if((kshd.partition_mode == POK_PARTITION_MODE_NORMAL)
      && (status.lock_level > 0) && (arinc_preemption_lock_holder == t)) {
      *MUTEX_ID = PREEMPTION_LOCK_MUTEX;
   } else {
      *MUTEX_ID = NO_MUTEX_OWNED;
   }

   msection_leave(&mutex_section);

   *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `arinc_mutex_set_priority`
 *
 * @description
 * Sets the priority of a thread (SET_PRIORITY): for the owner of a mutex, the
 * priority it gets back when it releases the mutex.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `thread` | `pok_thread_id_t` | [in] The thread. |
 * | `priority` | `PRIORITY_TYPE` | [in] The priority. |
 *
 * @return POK_ERRNO_OK, POK_ERRNO_UNAVAILABLE for a stopped owner, or the error
 *         of the kernel.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mutex_section` | `msection_t` | Protects all mutexes and their wait queues |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `entry` | `void *` | Its entry (not used) |
 * | `name` | `char[30]` | The thread's name (not used) |
 * | `owned` | `arinc_mutex_t *` | The mutex the thread owns |
 * | `ret` | `pok_ret_e` | The result |
 * | `status` | `pok_thread_status_t` | Its status |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `msection_enter()` with the address of `mutex_section`.
 * 2. Call `find_owned()` with the argument `thread`, and store the result in
 *    `owned`.
 * 3. If `owned` is not NULL and `thread` is not equal to `MAIN_THREAD_ID`:
 *    1. Call `pok_thread_get_status()` with the arguments `thread`, `name`, the
 *       address of `entry` and the address of `status`, and store the result in
 *       `ret`.
 *    2. If `ret` is equal to `POK_ERRNO_OK` and `status.state` is equal to
 *       `POK_STATE_STOPPED`:
 *       1. Set `ret` to `POK_ERRNO_UNAVAILABLE`.
 *    3. Otherwise, if `ret` is equal to `POK_ERRNO_OK`:
 *       1. Set `owned->retained_priority` to `priority`.
 *    4. Otherwise:
 *       1. Do nothing.
 * 4. Otherwise:
 *    1. Call `pok_thread_set_priority()` with the arguments `thread` and
 *       `priority`, and store the result in `ret`.
 * 5. Call `msection_leave()` with the address of `mutex_section`.
 * 6. Return `ret`.
 *
 * @calls
 * - `msection_enter()`
 * - `find_owned()`
 * - `pok_thread_get_status()`
 * - `pok_thread_set_priority()`
 * - `msection_leave()`
 */
pok_ret_e arinc_mutex_set_priority(pok_thread_id_t thread,
    PRIORITY_TYPE priority)
{
   pok_ret_e ret;

   msection_enter(&mutex_section);

   arinc_mutex_t* owned = find_owned(thread);

   if((owned != NULL) && (thread != MAIN_THREAD_ID)) {
      char name[MAX_NAME_LENGTH];
      void* entry;
      pok_thread_status_t status;

      ret = pok_thread_get_status(thread, name, &entry, &status);
      if((ret == POK_ERRNO_OK) && (status.state == POK_STATE_STOPPED)) {
         // Like pok_thread_set_priority() for a DORMANT process.
         ret = POK_ERRNO_UNAVAILABLE;
      } else if(ret == POK_ERRNO_OK) {
         // The owner keeps the mutex priority until it releases the mutex.
         owned->retained_priority = priority;
      } else {
         // ret is the error from pok_thread_get_status().
      }
   } else {
      ret = pok_thread_set_priority(thread, priority);
   }

   msection_leave(&mutex_section);

   return ret;
}

#endif /* POK_NEEDS_ARINC653_MUTEX */
