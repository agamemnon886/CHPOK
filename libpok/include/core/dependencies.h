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
 * Created by julien on Mon Apr  6 21:33:22 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-13: the unused macros
 * POK_NEEDS_SEMAPHORES and POK_NEEDS_ALLOCATOR are removed.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef LIBPOK_DEPENDENCIES_H
#define LIBPOK_DEPENDENCIES_H

#include <config.h>

#ifdef POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE

#ifdef POK_NEEDS_ARINC653_PARTITION

/* Documented in libpok/include/config.h. */
   #define POK_NEEDS_PARTITIONS 1
#endif

#ifdef POK_NEEDS_ARINC653_QUEUEING

/**
 * @macroname `POK_NEEDS_PORTS_QUEUEING`
 *
 * @description
 * Builds the queuing port system calls: needed by the APEX queuing port
 * services, or always without POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE.
 */
   #define POK_NEEDS_PORTS_QUEUEING 1
#endif

#ifdef POK_NEEDS_ARINC653_SAMPLING

/**
 * @macroname `POK_NEEDS_PORTS_SAMPLING`
 *
 * @description
 * Builds the sampling port system calls: needed by the APEX sampling port
 * services.
 */
   #define POK_NEEDS_PORTS_SAMPLING 1
#endif

#ifdef POK_NEEDS_ARINC653_BUFFER
   #ifndef POK_NEEDS_MIDDLEWARE

/* Documented in libpok/include/config.h. */
   #define POK_NEEDS_MIDDLEWARE 1
   #endif

   #ifndef POK_NEEDS_BUFFERS

/* Documented in libpok/include/config.h. */
   #define POK_NEEDS_BUFFERS 1
   #endif

#endif

#ifdef POK_NEEDS_ARINC653_BLACKBOARD
   #ifndef POK_NEEDS_MIDDLEWARE

/* Documented in libpok/include/config.h. */
   #define POK_NEEDS_MIDDLEWARE 1
   #endif

   #ifndef POK_NEEDS_BLACKBOARDS

/* Documented in libpok/include/config.h. */
   #define POK_NEEDS_BLACKBOARDS 1
   #endif
#endif

#ifdef POK_NEEDS_ARINC653_ERROR
   #ifndef POK_NEEDS_ERROR_HANDLING

/**
 * @macroname `POK_NEEDS_ERROR_HANDLING`
 *
 * @description
 * Builds the error handling system calls: needed by the APEX health monitoring
 * services.
 */
   #define POK_NEEDS_ERROR_HANDLING 1
   #endif
#endif

#if defined (POK_NEEDS_ERROR_HANDLING)
   #ifndef POK_NEEDS_FUNCTION_TIME_GETTICK

/**
 * @macroname `POK_NEEDS_FUNCTION_TIME_GETTICK`
 *
 * @description
 * The time services of POK, needed by the error handling, mutexes, events,
 * buffers, blackboards and time services; libpok's code doesn't test it.
 */
   #define POK_NEEDS_FUNCTION_TIME_GETTICK
   #endif
#endif

#ifdef POK_NEEDS_MUTEXES
   #ifndef POK_NEEDS_FUNCTION_TIME_GETTICK

/**
 * @macroname `POK_NEEDS_FUNCTION_TIME_GETTICK`
 *
 * @description
 * The time services of POK, needed by the error handling, mutexes, events,
 * buffers, blackboards and time services; libpok's code doesn't test it.
 */
   #define POK_NEEDS_FUNCTION_TIME_GETTICK
   #endif
#endif

#ifdef POK_NEEDS_EVENTS
   #ifndef POK_NEEDS_FUNCTION_TIME_GETTICK

/**
 * @macroname `POK_NEEDS_FUNCTION_TIME_GETTICK`
 *
 * @description
 * The time services of POK, needed by the error handling, mutexes, events,
 * buffers, blackboards and time services; libpok's code doesn't test it.
 */
   #define POK_NEEDS_FUNCTION_TIME_GETTICK
   #endif
#endif

#ifdef POK_NEEDS_BUFFERS
   #ifndef POK_NEEDS_FUNCTION_TIME_GETTICK

/**
 * @macroname `POK_NEEDS_FUNCTION_TIME_GETTICK`
 *
 * @description
 * The time services of POK, needed by the error handling, mutexes, events,
 * buffers, blackboards and time services; libpok's code doesn't test it.
 */
   #define POK_NEEDS_FUNCTION_TIME_GETTICK
   #endif
#endif

#ifdef POK_NEEDS_BLACKBOARDS
   #ifndef POK_NEEDS_FUNCTION_TIME_GETTICK

/**
 * @macroname `POK_NEEDS_FUNCTION_TIME_GETTICK`
 *
 * @description
 * The time services of POK, needed by the error handling, mutexes, events,
 * buffers, blackboards and time services; libpok's code doesn't test it.
 */
   #define POK_NEEDS_FUNCTION_TIME_GETTICK 1
   #endif
#endif

#ifdef POK_NEEDS_TIME
   #ifndef POK_NEEDS_FUNCTION_TIME_GETTICK

/**
 * @macroname `POK_NEEDS_FUNCTION_TIME_GETTICK`
 *
 * @description
 * The time services of POK, needed by the error handling, mutexes, events,
 * buffers, blackboards and time services; libpok's code doesn't test it.
 */
   #define POK_NEEDS_FUNCTION_TIME_GETTICK 1
   #endif
#endif
#endif /* POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE */

/*************************************************/

#ifndef POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE

/* core functions */
#ifndef POK_NEEDS_FUNCTIONS_TIME_GETTICK

/**
 * @macroname `POK_NEEDS_FUNCTION_TIME_GETTICK`
 *
 * @description
 * The time services of POK, needed by the error handling, mutexes, events,
 * buffers, blackboards and time services; libpok's code doesn't test it.
 */
#define POK_NEEDS_FUNCTION_TIME_GETTICK 1
#endif

#ifndef POK_NEEDS_THREADS

/* Documented in libpok/include/config.h. */
#define POK_NEEDS_THREADS               1
#endif

/**
 * @macroname `POK_NEEDS_PORTS_QUEUEING`
 *
 * @description
 * Builds the queuing port system calls: needed by the APEX queuing port
 * services, or always without POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE.
 */
#define POK_NEEDS_PORTS_QUEUEING        1

/* Documented in libpok/include/config.h. */
#define POK_NEEDS_PARTITIONS            1

/* Documented in libpok/include/config.h. */
#define POK_NEEDS_BUFFERS               1

/* Documented in libpok/include/config.h. */
#define POK_NEEDS_BLACKBOARDS           1

/* string.h functions */

/* Documented in libpok/include/config.h. */
#define POK_CONFIG_NEEDS_FUNC_UDIVDI3        1

#endif /* POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE */

#endif
