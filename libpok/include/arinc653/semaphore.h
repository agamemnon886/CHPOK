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

#ifdef POK_NEEDS_ARINC653_SEMAPHORE
#ifndef APEX_SEMAPHORE
#define APEX_SEMAPHORE

#ifndef POK_NEEDS_ARINC653_PROCESS

/* Documented in libpok/include/config.h. */
#define POK_NEEDS_ARINC653_PROCESS 1
#endif

#include <arinc653/types.h>
#include <arinc653/process.h>

/**
 * @macroname `POK_SEM_MAX_NAME_LENGTH`
 *
 * @description
 * The size of the name of a semaphore in pok_arinc653_semaphore_layer_t; it
 * must be at least the MAX_NAME_LENGTH of ARINC 653.
 */
#define POK_SEM_MAX_NAME_LENGTH 30

/**
 * @typename `pok_arinc653_semaphore_layer_t`
 *
 * @description
 * A semaphore of the partition as the kernel knows it: its kernel identifier
 * and name.
 */
typedef struct
{
   pok_bool_t        ready; /**< Whether the semaphore is created. */
   pok_sem_id_t      core_id; /**< The kernel's identifier of the semaphore. */
   /**
    * The name of the semaphore.
    */
   char              name[POK_SEM_MAX_NAME_LENGTH];
} pok_arinc653_semaphore_layer_t;

extern pok_arinc653_semaphore_layer_t pok_arinc653_semaphores_layers[];

/**
 * @macroname `MAX_NUMBER_OF_SEMAPHORES`
 *
 * @description
 * The maximum number of semaphores of a partition (implementation dependent).
 */
#define MAX_NUMBER_OF_SEMAPHORES 63

/**
 * @macroname `MAX_SEMAPHORE_VALUE`
 *
 * @description
 * The maximum value of a semaphore.
 */
#define MAX_SEMAPHORE_VALUE 32767

/**
 * @typename `SEMAPHORE_ID_TYPE`
 *
 * @description
 * The identifier of a semaphore.
 */
typedef APEX_INTEGER SEMAPHORE_ID_TYPE;

/**
 * @typename `SEMAPHORE_VALUE_TYPE`
 *
 * @description
 * The value of a semaphore.
 */
typedef APEX_INTEGER SEMAPHORE_VALUE_TYPE;

/**
 * @typename `SEMAPHORE_NAME_TYPE`
 *
 * @description
 * The name of a semaphore.
 */
typedef NAME_TYPE SEMAPHORE_NAME_TYPE;

/**
 * @typename `SEMAPHORE_STATUS_TYPE`
 *
 * @description
 * The status of a semaphore (GET_SEMAPHORE_STATUS()).
 */
typedef
struct {
   SEMAPHORE_VALUE_TYPE CURRENT_VALUE; /**< The current value. */
   SEMAPHORE_VALUE_TYPE MAXIMUM_VALUE; /**< The maximum value. */
   /**
    * The number of processes waiting.
    */
   WAITING_RANGE_TYPE WAITING_PROCESSES;
} SEMAPHORE_STATUS_TYPE;
/** \cond */
extern void CREATE_SEMAPHORE (
      /*IN */ const SEMAPHORE_NAME_TYPE SEMAPHORE_NAME,
      /*IN */ SEMAPHORE_VALUE_TYPE CURRENT_VALUE,
      /*IN */ SEMAPHORE_VALUE_TYPE MAXIMUM_VALUE,
      /*IN */ QUEUING_DISCIPLINE_TYPE QUEUING_DISCIPLINE,
      /*OUT*/ SEMAPHORE_ID_TYPE *SEMAPHORE_ID,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
extern void WAIT_SEMAPHORE (
      /*IN */ SEMAPHORE_ID_TYPE SEMAPHORE_ID,
      /*IN */ SYSTEM_TIME_TYPE TIME_OUT,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
extern void SIGNAL_SEMAPHORE (
      /*IN */ SEMAPHORE_ID_TYPE SEMAPHORE_ID,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
extern void GET_SEMAPHORE_ID (
      /*IN */ const SEMAPHORE_NAME_TYPE SEMAPHORE_NAME,
      /*OUT*/ SEMAPHORE_ID_TYPE *SEMAPHORE_ID,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
extern void GET_SEMAPHORE_STATUS (
      /*IN */ SEMAPHORE_ID_TYPE SEMAPHORE_ID,
      /*OUT*/ SEMAPHORE_STATUS_TYPE *SEMAPHORE_STATUS,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
/** \endcond */
#endif
#endif
