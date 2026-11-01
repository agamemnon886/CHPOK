/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-01: added PROCESS_INDEX_TYPE and
 * the ARINC 653P1-4 multicore services INITIALIZE_PROCESS_CORE_AFFINITY,
 * GET_MY_PROCESSOR_CORE_ID and GET_MY_INDEX; added NULL_PROCESS_ID,
 * MAIN_PROCESS_ID and the FAULTED process state (ARINC 653P1-4).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_PROCESS

#include <arinc653/types.h>

#ifndef APEX_PROCESS
#define APEX_PROCESS

#define MAX_NUMBER_OF_PROCESSES  SYSTEM_LIMIT_NUMBER_OF_PROCESSES
#define MIN_PRIORITY_VALUE       1
#define MAX_PRIORITY_VALUE       239
#define MAX_LOCK_LEVEL           16

typedef NAME_TYPE            PROCESS_NAME_TYPE;

typedef APEX_INTEGER         PROCESS_ID_TYPE;
#define NULL_PROCESS_ID      0      /* no process; never a valid process ID */
#define MAIN_PROCESS_ID     -1      /* the partition's main process */

typedef APEX_INTEGER         PROCESS_INDEX_TYPE;

typedef APEX_INTEGER         LOCK_LEVEL_TYPE;

typedef APEX_UNSIGNED        STACK_SIZE_TYPE;

typedef APEX_INTEGER         WAITING_RANGE_TYPE;

typedef APEX_INTEGER         PRIORITY_TYPE;

typedef enum
{
   DORMANT = 0,
   READY    = 1,
   RUNNING = 2,
   WAITING = 3,
   FAULTED = 4      /* not reported by JetOS yet */
} PROCESS_STATE_TYPE;

typedef enum
{
   SOFT = 0,
   HARD = 1
} DEADLINE_TYPE;

typedef struct {
   PROCESS_NAME_TYPE       NAME;
   SYSTEM_ADDRESS_TYPE     ENTRY_POINT;
   STACK_SIZE_TYPE         STACK_SIZE;
   PRIORITY_TYPE           BASE_PRIORITY;
   SYSTEM_TIME_TYPE        PERIOD;
   SYSTEM_TIME_TYPE        TIME_CAPACITY;
   DEADLINE_TYPE           DEADLINE;
} PROCESS_ATTRIBUTE_TYPE;

typedef struct {
   PROCESS_ATTRIBUTE_TYPE    ATTRIBUTES;
   PRIORITY_TYPE             CURRENT_PRIORITY;
   SYSTEM_TIME_TYPE          DEADLINE_TIME;
   PROCESS_STATE_TYPE        PROCESS_STATE;
} PROCESS_STATUS_TYPE;

extern void CREATE_PROCESS (
      /*in */ PROCESS_ATTRIBUTE_TYPE     *ATTRIBUTES,
      /*out*/ PROCESS_ID_TYPE            *PROCESS_ID,
      /*out*/ RETURN_CODE_TYPE           *RETURN_CODE );

extern void SET_PRIORITY (
      /*in */ PROCESS_ID_TYPE            PROCESS_ID,
      /*in */ PRIORITY_TYPE              PRIORITY,
      /*out*/ RETURN_CODE_TYPE           *RETURN_CODE );

extern void SUSPEND_SELF (
      /*in */ SYSTEM_TIME_TYPE           TIME_OUT,
      /*out*/ RETURN_CODE_TYPE           *RETURN_CODE );

extern void SUSPEND (
      /*in */ PROCESS_ID_TYPE            PROCESS_ID,
      /*out*/ RETURN_CODE_TYPE           *RETURN_CODE );

extern void RESUME (
      /*in */ PROCESS_ID_TYPE            PROCESS_ID,
      /*out*/ RETURN_CODE_TYPE           *RETURN_CODE );

extern void STOP_SELF ();

extern void STOP (
      /*in */ PROCESS_ID_TYPE            PROCESS_ID,
      /*out*/ RETURN_CODE_TYPE           *RETURN_CODE );

extern void START (
      /*in */ PROCESS_ID_TYPE     PROCESS_ID,
      /*out*/ RETURN_CODE_TYPE    *RETURN_CODE );

extern void DELAYED_START (
      /*in */ PROCESS_ID_TYPE     PROCESS_ID,
      /*in */ SYSTEM_TIME_TYPE    DELAY_TIME,
      /*out*/ RETURN_CODE_TYPE    *RETURN_CODE );

extern void LOCK_PREEMPTION (
      /*out*/ LOCK_LEVEL_TYPE     *LOCK_LEVEL,
      /*out*/ RETURN_CODE_TYPE    *RETURN_CODE );

extern void UNLOCK_PREEMPTION (
      /*out*/ LOCK_LEVEL_TYPE     *LOCK_LEVEL,
      /*out*/ RETURN_CODE_TYPE    *RETURN_CODE );

extern void GET_MY_ID (
      /*out*/ PROCESS_ID_TYPE     *PROCESS_ID,
      /*out*/ RETURN_CODE_TYPE    *RETURN_CODE );

extern void GET_PROCESS_ID (
      /*in */ PROCESS_NAME_TYPE   PROCESS_NAME,
      /*out*/ PROCESS_ID_TYPE     *PROCESS_ID,
      /*out*/ RETURN_CODE_TYPE    *RETURN_CODE );

extern void GET_PROCESS_STATUS (
      /*in */ PROCESS_ID_TYPE     PROCESS_ID,
      /*out*/ PROCESS_STATUS_TYPE *PROCESS_STATUS,
      /*out*/ RETURN_CODE_TYPE    *RETURN_CODE );

extern void INITIALIZE_PROCESS_CORE_AFFINITY (
      /*in */ PROCESS_ID_TYPE        PROCESS_ID,
      /*in */ PROCESSOR_CORE_ID_TYPE PROCESSOR_CORE_ID,
      /*out*/ RETURN_CODE_TYPE       *RETURN_CODE );

extern void GET_MY_PROCESSOR_CORE_ID (
      /*out*/ PROCESSOR_CORE_ID_TYPE *PROCESSOR_CORE_ID,
      /*out*/ RETURN_CODE_TYPE       *RETURN_CODE );

extern void GET_MY_INDEX (
      /*out*/ PROCESS_INDEX_TYPE     *PROCESS_INDEX,
      /*out*/ RETURN_CODE_TYPE       *RETURN_CODE );

#endif
#endif
