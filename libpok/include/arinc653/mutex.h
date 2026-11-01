/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ARINC 653 mutex services (Part 1, added in Supplement 4).
 *
 * Types, constants and prototypes as in the ARINC653.h header published
 * with ARINC 653P1-5. The number of mutexes of a partition is set by
 * <ARINC653_Mutexes Count="..."/> in the partition's configuration.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_MUTEX
#ifndef APEX_MUTEX
#define APEX_MUTEX

#ifndef POK_NEEDS_ARINC653_PROCESS
#define POK_NEEDS_ARINC653_PROCESS 1
#endif

#include <arinc653/types.h>
#include <arinc653/process.h>

#define MAX_NUMBER_OF_MUTEXES   SYSTEM_LIMIT_NUMBER_OF_MUTEXES

typedef NAME_TYPE        MUTEX_NAME_TYPE;

typedef APEX_INTEGER     MUTEX_ID_TYPE;
#define NO_MUTEX_OWNED          -2  /* GET_PROCESS_MUTEX_STATE: no mutex */
#define PREEMPTION_LOCK_MUTEX   -3  /* the partition's preemption lock */

typedef APEX_INTEGER     LOCK_COUNT_TYPE;

typedef enum { AVAILABLE = 0, OWNED = 1 } MUTEX_STATE_TYPE;

typedef struct {
   PROCESS_ID_TYPE       MUTEX_OWNER;   /* NULL_PROCESS_ID if AVAILABLE */
   MUTEX_STATE_TYPE      MUTEX_STATE;
   PRIORITY_TYPE         MUTEX_PRIORITY;
   LOCK_COUNT_TYPE       LOCK_COUNT;
   WAITING_RANGE_TYPE    WAITING_PROCESSES;
} MUTEX_STATUS_TYPE;

extern void CREATE_MUTEX (
      /*in */ MUTEX_NAME_TYPE          MUTEX_NAME,
      /*in */ PRIORITY_TYPE            MUTEX_PRIORITY,
      /*in */ QUEUING_DISCIPLINE_TYPE  QUEUING_DISCIPLINE,
      /*out*/ MUTEX_ID_TYPE            *MUTEX_ID,
      /*out*/ RETURN_CODE_TYPE         *RETURN_CODE );

extern void ACQUIRE_MUTEX (
      /*in */ MUTEX_ID_TYPE            MUTEX_ID,
      /*in */ SYSTEM_TIME_TYPE         TIME_OUT,
      /*out*/ RETURN_CODE_TYPE         *RETURN_CODE );

extern void RELEASE_MUTEX (
      /*in */ MUTEX_ID_TYPE            MUTEX_ID,
      /*out*/ RETURN_CODE_TYPE         *RETURN_CODE );

extern void RESET_MUTEX (
      /*in */ MUTEX_ID_TYPE            MUTEX_ID,
      /*in */ PROCESS_ID_TYPE          PROCESS_ID,
      /*out*/ RETURN_CODE_TYPE         *RETURN_CODE );

extern void GET_MUTEX_ID (
      /*in */ MUTEX_NAME_TYPE          MUTEX_NAME,
      /*out*/ MUTEX_ID_TYPE            *MUTEX_ID,
      /*out*/ RETURN_CODE_TYPE         *RETURN_CODE );

extern void GET_MUTEX_STATUS (
      /*in */ MUTEX_ID_TYPE            MUTEX_ID,
      /*out*/ MUTEX_STATUS_TYPE        *MUTEX_STATUS,
      /*out*/ RETURN_CODE_TYPE         *RETURN_CODE );

extern void GET_PROCESS_MUTEX_STATE (
      /*in */ PROCESS_ID_TYPE          PROCESS_ID,
      /*out*/ MUTEX_ID_TYPE            *MUTEX_ID,
      /*out*/ RETURN_CODE_TYPE         *RETURN_CODE );

#endif /* APEX_MUTEX */
#endif /* POK_NEEDS_ARINC653_MUTEX */
