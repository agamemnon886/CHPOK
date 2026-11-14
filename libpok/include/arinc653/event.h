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

#ifdef POK_NEEDS_ARINC653_EVENT

#ifndef APEX_EVENT
#define APEX_EVENT

#ifndef POK_NEEDS_ARINC653_PROCESS

/* Documented in libpok/include/config.h. */
#define POK_NEEDS_ARINC653_PROCESS 1
#endif

#include <arinc653/process.h>

/**
 * @macroname `POK_EVENT_MAX_NAME_LENGTH`
 *
 * @description
 * The size of the name of an event in pok_arinc653_event_layer_t; it must be at
 * least the MAX_NAME_LENGTH of ARINC 653.
 */
#define POK_EVENT_MAX_NAME_LENGTH 30

/**
 * @typename `pok_arinc653_event_layer_t`
 *
 * @description
 * An event of the partition as the kernel knows it: its kernel identifier and
 * name.
 */
typedef struct
{
   pok_event_id_t    core_id; /**< The kernel's identifier of the event. */
   pok_bool_t        ready; /**< Whether the event is created. */
   pok_bool_t        is_up; /**< Whether the event is up. */
   /**
    * The name of the event.
    */
   char              name[POK_EVENT_MAX_NAME_LENGTH];
} pok_arinc653_event_layer_t;

extern pok_arinc653_event_layer_t pok_arinc653_events_layers[];

/**
 * @macroname `MAX_NUMBER_OF_EVENTS`
 *
 * @description
 * The maximum number of events of a partition (implementation dependent).
 */
#define MAX_NUMBER_OF_EVENTS 63

/**
 * @typename `EVENT_ID_TYPE`
 *
 * @description
 * The identifier of an event.
 */
typedef APEX_INTEGER EVENT_ID_TYPE;

/**
 * @typename `EVENT_STATE_TYPE`
 *
 * @description
 * The state of an event.
 */
typedef enum {
   UP, /**< The event is up: the waiting processes go on. */
   DOWN /**< The event is down: the processes wait for it. */
} EVENT_STATE_TYPE;

/**
 * @typename `EVENT_NAME_TYPE`
 *
 * @description
 * The name of an event.
 */
typedef NAME_TYPE EVENT_NAME_TYPE;

/**
 * @typename `EVENT_STATUS_TYPE`
 *
 * @description
 * The status of an event (GET_EVENT_STATUS()).
 */
typedef struct {
   EVENT_STATE_TYPE     EVENT_STATE; /**< The state of the event. */
   /**
    * The number of processes waiting.
    */
   WAITING_RANGE_TYPE   WAITING_PROCESSES;
} EVENT_STATUS_TYPE;

/** \cond */
extern void CREATE_EVENT (
      /*IN */ const EVENT_NAME_TYPE EVENT_NAME,
      /*OUT*/ EVENT_ID_TYPE *EVENT_ID,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
extern void SET_EVENT (
      /*IN */ EVENT_ID_TYPE EVENT_ID,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
extern void RESET_EVENT (
      /*IN */ EVENT_ID_TYPE EVENT_ID,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
extern void WAIT_EVENT (
      /*IN */ EVENT_ID_TYPE EVENT_ID,
      /*IN */ SYSTEM_TIME_TYPE TIME_OUT,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
extern void GET_EVENT_ID (
      /*IN */ const EVENT_NAME_TYPE EVENT_NAME,
      /*OUT*/ EVENT_ID_TYPE *EVENT_ID,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
extern void GET_EVENT_STATUS (
      /*IN */ EVENT_ID_TYPE EVENT_ID,
      /*OUT*/ EVENT_STATUS_TYPE *EVENT_STATUS ,
      /*OUT*/ RETURN_CODE_TYPE *RETURN_CODE );
/** \endcond */
#endif
#endif
