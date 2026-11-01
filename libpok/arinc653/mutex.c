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
 */

/**
 * \file mutex.c
 * \brief ARINC 653 mutexes with the priority ceiling protocol.
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

#define MAIN_THREAD_ID  0   /* kernel thread of the main process */

static size_t nmutexes_used = 0;

/* Protects all mutexes and their wait queues. */
static struct msection mutex_section;

void arinc_mutex_init(void)
{
   msection_init(&mutex_section);
   nmutexes_used = 0;
}

/* Find mutex by name (case insensitive). Returns NULL if not found. */
static struct arinc_mutex* find_mutex(const char* name)
{
   for(size_t i = 0; i < nmutexes_used; i++)
   {
      struct arinc_mutex* mutex = &arinc_mutexes[i];
      if(strncasecmp(mutex->mutex_name, name, MAX_NAME_LENGTH) == 0)
         return mutex;
   }

   return NULL;
}

/*
 * Mutex by ID, or NULL. IDs start from 1, so NO_MUTEX_OWNED and
 * PREEMPTION_LOCK_MUTEX are never valid here.
 */
static struct arinc_mutex* get_mutex(MUTEX_ID_TYPE mutex_id)
{
   if(mutex_id <= 0 || mutex_id > (MUTEX_ID_TYPE)nmutexes_used)
      return NULL;

   return &arinc_mutexes[mutex_id - 1];
}

static MUTEX_ID_TYPE mutex_id_of(const struct arinc_mutex* mutex)
{
   return (mutex - arinc_mutexes) + 1;
}

/* Mutex owned by the thread, or NULL. Call inside mutex_section. */
static struct arinc_mutex* find_owned(pok_thread_id_t thread)
{
   for(size_t i = 0; i < nmutexes_used; i++)
   {
      if(arinc_mutexes[i].owner == thread)
         return &arinc_mutexes[i];
   }

   return NULL;
}

/* ARINC process ID of a kernel thread. */
static PROCESS_ID_TYPE process_id_of(pok_thread_id_t thread)
{
   return thread == MAIN_THREAD_ID ? MAIN_PROCESS_ID : (PROCESS_ID_TYPE)thread + 1;
}

/*
 * Kernel thread of a process: MAIN_PROCESS_ID or the ID of a created
 * process (not the error handler). Returns FALSE for anything else.
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
   if(process_id < 1 || process_id > (PROCESS_ID_TYPE)kshd.max_n_threads)
      return FALSE;

   // The kernel rejects the main thread, the error handler and threads
   // which aren't created yet.
   if(pok_thread_get_status(process_id - 1, name, &entry, &status) != POK_ERRNO_OK)
      return FALSE;

   *thread = process_id - 1;
   return TRUE;
}

/*
 * Set the current priority of a thread. The main process has no ARINC
 * priority and runs alone during initialization, so it's left as is.
 * A stopped thread gets its base priority when it's started again.
 */
static void set_thread_priority(pok_thread_id_t thread, PRIORITY_TYPE priority)
{
   if(thread == MAIN_THREAD_ID || kshd.tshd[thread].priority == priority)
      return;

   pok_thread_set_priority(thread, priority);
}

/* Make thread the owner of an available mutex. Call inside mutex_section. */
static void take_mutex(struct arinc_mutex* mutex, pok_thread_id_t thread)
{
   mutex->owner = thread;
   mutex->lock_count = 1;
   mutex->retained_priority = kshd.tshd[thread].priority;

   set_thread_priority(thread, mutex->priority);
}

/*
 * Release the mutex completely: restore the owner's priority and pass
 * the mutex to the first waiting process, if any. Call inside
 * mutex_section.
 */
static void release_mutex(struct arinc_mutex* mutex)
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

void CREATE_MUTEX (MUTEX_NAME_TYPE MUTEX_NAME,
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

   if(MUTEX_PRIORITY < MIN_PRIORITY_VALUE || MUTEX_PRIORITY > MAX_PRIORITY_VALUE) {
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

   struct arinc_mutex* mutex = &arinc_mutexes[nmutexes_used];

   // Names may be shorter than MAX_NAME_LENGTH (NUL-terminated); don't
   // read past them.
   strncpy(mutex->mutex_name, MUTEX_NAME, MAX_NAME_LENGTH);
   mutex->priority = MUTEX_PRIORITY;
   mutex->discipline = QUEUING_DISCIPLINE;
   mutex->owner = JET_THREAD_ID_NONE;
   mutex->lock_count = 0;
   mutex->retained_priority = 0;
   msection_wq_init(&mutex->process_queue);

   *MUTEX_ID = nmutexes_used + 1; // Avoid 0 and the negative special IDs.

   nmutexes_used++;

   *RETURN_CODE = NO_ERROR;
}

void ACQUIRE_MUTEX (MUTEX_ID_TYPE MUTEX_ID,
                    SYSTEM_TIME_TYPE TIME_OUT,
                    RETURN_CODE_TYPE *RETURN_CODE)
{
   struct arinc_mutex* mutex = get_mutex(MUTEX_ID);
   pok_thread_id_t t = kshd.current_thread_id;

   if(mutex == NULL) {
      // Not created, or PREEMPTION_LOCK_MUTEX.
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   if(TIME_OUT < 0 && TIME_OUT != INFINITE_TIME_VALUE) {
      *RETURN_CODE = INVALID_PARAM;
      return;
   }

   msection_enter(&mutex_section);

   struct arinc_mutex* owned = find_owned(t);

   if(owned != NULL && owned != mutex) {
      // A process owns at most one mutex at a time.
      *RETURN_CODE = INVALID_MODE;
   }

   else if(t == kshd.error_thread_id) {
      // The error handler can't acquire mutexes.
      *RETURN_CODE = INVALID_MODE;
   }

   else if(t != MAIN_THREAD_ID && kshd.tshd[t].priority > mutex->priority) {
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
      }
   }

   msection_leave(&mutex_section);
}

void RELEASE_MUTEX (MUTEX_ID_TYPE MUTEX_ID,
                    RETURN_CODE_TYPE *RETURN_CODE)
{
   struct arinc_mutex* mutex = get_mutex(MUTEX_ID);

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
      if(--mutex->lock_count == 0)
         release_mutex(mutex);

      *RETURN_CODE = NO_ERROR;
   }

   msection_leave(&mutex_section);
}

void RESET_MUTEX (MUTEX_ID_TYPE MUTEX_ID,
                  PROCESS_ID_TYPE PROCESS_ID,
                  RETURN_CODE_TYPE *RETURN_CODE)
{
   struct arinc_mutex* mutex = get_mutex(MUTEX_ID);
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

void GET_MUTEX_ID (MUTEX_NAME_TYPE MUTEX_NAME,
                   MUTEX_ID_TYPE *MUTEX_ID,
                   RETURN_CODE_TYPE *RETURN_CODE)
{
   struct arinc_mutex* mutex = find_mutex(MUTEX_NAME);

   if(mutex == NULL) {
      *RETURN_CODE = INVALID_CONFIG;
      return;
   }

   *MUTEX_ID = mutex_id_of(mutex);
   *RETURN_CODE = NO_ERROR;
}

void GET_MUTEX_STATUS (MUTEX_ID_TYPE MUTEX_ID,
                       MUTEX_STATUS_TYPE *MUTEX_STATUS,
                       RETURN_CODE_TYPE *RETURN_CODE)
{
   struct arinc_mutex* mutex = get_mutex(MUTEX_ID);

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

   pok_current_partition_get_status(&status);

   msection_enter(&mutex_section);

   struct arinc_mutex* owned = find_owned(t);

   if(owned != NULL)
      *MUTEX_ID = mutex_id_of(owned);
   else if(kshd.partition_mode == POK_PARTITION_MODE_NORMAL
      && status.lock_level > 0 && arinc_preemption_lock_holder == t)
      *MUTEX_ID = PREEMPTION_LOCK_MUTEX;
   else
      *MUTEX_ID = NO_MUTEX_OWNED;

   msection_leave(&mutex_section);

   *RETURN_CODE = NO_ERROR;
}

pok_ret_t arinc_mutex_set_priority(pok_thread_id_t thread, PRIORITY_TYPE priority)
{
   pok_ret_t ret;

   msection_enter(&mutex_section);

   struct arinc_mutex* owned = find_owned(thread);

   if(owned != NULL && thread != MAIN_THREAD_ID) {
      char name[MAX_NAME_LENGTH];
      void* entry;
      pok_thread_status_t status;

      ret = pok_thread_get_status(thread, name, &entry, &status);
      if(ret == POK_ERRNO_OK && status.state == POK_STATE_STOPPED) {
         // Like pok_thread_set_priority() for a DORMANT process.
         ret = POK_ERRNO_UNAVAILABLE;
      } else if(ret == POK_ERRNO_OK) {
         // The owner keeps the mutex priority until it releases the mutex.
         owned->retained_priority = priority;
      }
   } else {
      ret = pok_thread_set_priority(thread, priority);
   }

   msection_leave(&mutex_section);

   return ret;
}

#endif /* POK_NEEDS_ARINC653_MUTEX */
