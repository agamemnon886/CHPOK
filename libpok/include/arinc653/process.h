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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: STOP_SELF() is
 * declared as a prototype with (void).
 * Modified by Kirill Brazhnikov, 2026-11-03: SET_PRIORITY()'s priority
 * parameter is lower case, as in the definition: PRIORITY is an enumerator of
 * QUEUING_DISCIPLINE_TYPE (Rules 8.3, 5.3).
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: name
 * parameters are const (they are only read), so callers can pass string
 * literals.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_PROCESS

#include <arinc653/types.h>

#ifndef APEX_PROCESS
#define APEX_PROCESS

/**
 * @macroname `MAX_NUMBER_OF_PROCESSES`
 *
 * @description
 * The maximum number of processes of a partition
 * (SYSTEM_LIMIT_NUMBER_OF_PROCESSES).
 */
#define MAX_NUMBER_OF_PROCESSES  SYSTEM_LIMIT_NUMBER_OF_PROCESSES

/**
 * @macroname `MIN_PRIORITY_VALUE`
 *
 * @description
 * The lowest priority of a process.
 */
#define MIN_PRIORITY_VALUE       1

/**
 * @macroname `MAX_PRIORITY_VALUE`
 *
 * @description
 * The highest priority of a process.
 */
#define MAX_PRIORITY_VALUE       239

/**
 * @macroname `MAX_LOCK_LEVEL`
 *
 * @description
 * The maximum preemption lock level (LOCK_PREEMPTION()).
 */
#define MAX_LOCK_LEVEL           16

/**
 * @typename `PROCESS_NAME_TYPE`
 *
 * @description
 * The name of a process.
 */
typedef NAME_TYPE            PROCESS_NAME_TYPE;

/**
 * @typename `PROCESS_ID_TYPE`
 *
 * @description
 * The identifier of a process (or NULL_PROCESS_ID, MAIN_PROCESS_ID).
 */
typedef APEX_INTEGER         PROCESS_ID_TYPE;

/**
 * @macroname `NULL_PROCESS_ID`
 *
 * @description
 * No process; never a valid process ID.
 */
#define NULL_PROCESS_ID      0

/**
 * @macroname `MAIN_PROCESS_ID`
 *
 * @description
 * The partition's main process.
 */
#define MAIN_PROCESS_ID     -1

/**
 * @typename `PROCESS_INDEX_TYPE`
 *
 * @description
 * The index of a process in the partition.
 */
typedef APEX_INTEGER         PROCESS_INDEX_TYPE;

/**
 * @typename `LOCK_LEVEL_TYPE`
 *
 * @description
 * A preemption lock level.
 */
typedef APEX_INTEGER         LOCK_LEVEL_TYPE;

/**
 * @typename `STACK_SIZE_TYPE`
 *
 * @description
 * The size of the stack of a process.
 */
typedef APEX_UNSIGNED        STACK_SIZE_TYPE;

/**
 * @typename `WAITING_RANGE_TYPE`
 *
 * @description
 * A number of waiting processes.
 */
typedef APEX_INTEGER         WAITING_RANGE_TYPE;

/**
 * @typename `PRIORITY_TYPE`
 *
 * @description
 * The priority of a process.
 */
typedef APEX_INTEGER         PRIORITY_TYPE;

/**
 * @typename `PROCESS_STATE_TYPE`
 *
 * @description
 * The state of a process.
 */
typedef enum
{
   DORMANT = 0, /**< Not started, or stopped. */
   READY    = 1, /**< Able to run. */
   RUNNING = 2, /**< Running. */
   WAITING = 3, /**< Waiting (for a resource, a time or a resumption). */
   FAULTED = 4 /**< Not reported by JetOS yet. */
} PROCESS_STATE_TYPE;

/**
 * @typename `DEADLINE_TYPE`
 *
 * @description
 * How a missed deadline of a process is treated.
 */
typedef enum
{
   SOFT = 0, /**< A soft deadline. */
   HARD = 1 /**< A hard deadline. */
} DEADLINE_TYPE;

/**
 * @typename `PROCESS_ATTRIBUTE_TYPE`
 *
 * @description
 * The attributes of a process (CREATE_PROCESS()).
 */
typedef struct {
   PROCESS_NAME_TYPE       NAME; /**< The name of the process. */
   SYSTEM_ADDRESS_TYPE     ENTRY_POINT; /**< The function the process runs. */
   STACK_SIZE_TYPE         STACK_SIZE; /**< The size of its stack. */
   PRIORITY_TYPE           BASE_PRIORITY; /**< Its base priority. */
   /**
    * Its period (INFINITE_TIME_VALUE: aperiodic).
    */
   SYSTEM_TIME_TYPE        PERIOD;
   /**
    * Its time capacity (INFINITE_TIME_VALUE: no deadline).
    */
   SYSTEM_TIME_TYPE        TIME_CAPACITY;
   /**
    * Whether its deadline is soft or hard.
    */
   DEADLINE_TYPE           DEADLINE;
} PROCESS_ATTRIBUTE_TYPE;

/**
 * @typename `PROCESS_STATUS_TYPE`
 *
 * @description
 * The status of a process (GET_PROCESS_STATUS()).
 */
typedef struct {
   PROCESS_ATTRIBUTE_TYPE    ATTRIBUTES; /**< The attributes of the process. */
   PRIORITY_TYPE             CURRENT_PRIORITY; /**< Its current priority. */
   /**
    * Its current deadline (a system time).
    */
   SYSTEM_TIME_TYPE          DEADLINE_TIME;
   PROCESS_STATE_TYPE        PROCESS_STATE; /**< Its state. */
} PROCESS_STATUS_TYPE;

/** \cond */
extern void CREATE_PROCESS (
      /*in */ PROCESS_ATTRIBUTE_TYPE     *ATTRIBUTES,
      /*out*/ PROCESS_ID_TYPE            *PROCESS_ID,
      /*out*/ RETURN_CODE_TYPE           *RETURN_CODE );

extern void SET_PRIORITY (
      /*in */ PROCESS_ID_TYPE            PROCESS_ID,
      /*in */ PRIORITY_TYPE              priority,
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

extern void STOP_SELF (void);

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
      /*in */ const PROCESS_NAME_TYPE   PROCESS_NAME,
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
/** \endcond */

#endif
#endif
