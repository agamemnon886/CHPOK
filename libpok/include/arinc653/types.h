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
 * Modified by Kirill Brazhnikov, 2026-10-31: APEX_INTEGER and
 * APEX_UNSIGNED are exactly 32 bits wide, as ARINC 653 requires, also on
 * 64-bit targets where long is 64 bits.
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-01: added PROCESSOR_CORE_ID_TYPE
 * and CORE_AFFINITY_NO_PREFERENCE (ARINC 653P1-4); added
 * SYSTEM_LIMIT_NUMBER_OF_MUTEXES.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: U suffixes for
 * unsigned constants.
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef APEX_TYPES
#define APEX_TYPES

#include <types.h>

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_PARTITIONS`
 *
 * @description
 * The maximum number of partitions of the module (module scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_PARTITIONS     32

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_MESSAGES`
 *
 * @description
 * The maximum number of messages of a port or buffer (module scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_MESSAGES       512

/**
 * @macroname `SYSTEM_LIMIT_MESSAGE_SIZE`
 *
 * @description
 * The maximum size of a message, in bytes (module scope).
 */
#define SYSTEM_LIMIT_MESSAGE_SIZE             8192

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_PROCESSES`
 *
 * @description
 * The maximum number of processes of a partition (partition scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_PROCESSES      128

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_SAMPLING_PORTS`
 *
 * @description
 * The maximum number of sampling ports of a partition (partition scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_SAMPLING_PORTS 512

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_QUEUING_PORTS`
 *
 * @description
 * The maximum number of queuing ports of a partition (partition scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_QUEUING_PORTS  512

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_BUFFERS`
 *
 * @description
 * The maximum number of buffers of a partition (partition scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_BUFFERS        256

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_BLACKBOARDS`
 *
 * @description
 * The maximum number of blackboards of a partition (partition scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_BLACKBOARDS    256

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_SEMAPHORES`
 *
 * @description
 * The maximum number of semaphores of a partition (partition scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_SEMAPHORES     256

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_EVENTS`
 *
 * @description
 * The maximum number of events of a partition (partition scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_EVENTS         256

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_MUTEXES`
 *
 * @description
 * The maximum number of mutexes of a partition (partition scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_MUTEXES        256

/*
 * Base APEX types.
 *
 * The actual size of these base types is system specific and the sizes must
 * match the sizes used by the implementation of the underlying Operating
 * System. The standard's C header uses long for the 32-bit types, which is
 * only right where long is 32 bits; the Ada binding fixes the sizes (e.g.
 * "for APEX_INTEGER'size use 32"), so fixed-width types are used here.
 */

/**
 * @typename `APEX_BYTE`
 *
 * @description
 * 8-bit unsigned.
 */
typedef   uint8_t          APEX_BYTE;

/**
 * @typename `APEX_INTEGER`
 *
 * @description
 * 32-bit signed.
 */
typedef   int32_t          APEX_INTEGER;

/**
 * @typename `APEX_UNSIGNED`
 *
 * @description
 * 32-bit unsigned.
 */
typedef   uint32_t         APEX_UNSIGNED;

/**
 * @typename `APEX_LONG_INTEGER`
 *
 * @description
 * 64-bit signed.
 */
typedef   int64_t          APEX_LONG_INTEGER;

_Static_assert(sizeof(APEX_INTEGER) == 4U, "APEX_INTEGER must be 32-bit");
_Static_assert(sizeof(APEX_UNSIGNED) == 4U, "APEX_UNSIGNED must be 32-bit");
_Static_assert(sizeof(APEX_LONG_INTEGER) == 8U,
               "APEX_LONG_INTEGER must be 64-bit");
/* General APEX types. */

/**
 * @typename `RETURN_CODE_TYPE`
 *
 * @description
 * The return code of an APEX service.
 */
typedef
enum {
   NO_ERROR        =  0, /**< Request valid and operation performed. */
   NO_ACTION       =  1, /**< Status of system unaffected by request. */
   NOT_AVAILABLE   =  2, /**< Resource required by request unavailable. */
   INVALID_PARAM   =  3, /**< Invalid parameter specified in request. */
   INVALID_CONFIG  =  4, /**< Parameter incompatible with configuration. */
   INVALID_MODE    =  5, /**< Request incompatible with current mode. */
   TIMED_OUT       =  6 /**< Time-out tied up with request has expired. */
} RETURN_CODE_TYPE;

/* Documented in uapi/types.h (the same value). */
#define   MAX_NAME_LENGTH              30

/**
 * @typename `NAME_TYPE`
 *
 * @description
 * The name of an APEX object (not necessarily terminated by a null character).
 */
typedef   char             NAME_TYPE[MAX_NAME_LENGTH];

/**
 * @typename `SYSTEM_ADDRESS_TYPE`
 *
 * @description
 * An address: the entry point of a process.
 */
typedef   void             (* SYSTEM_ADDRESS_TYPE);

/**
 * @typename `MESSAGE_ADDR_TYPE`
 *
 * @description
 * The address of a message.
 */
typedef   APEX_BYTE*       MESSAGE_ADDR_TYPE;

/**
 * @typename `MESSAGE_SIZE_TYPE`
 *
 * @description
 * The size of a message.
 */
typedef   APEX_INTEGER     MESSAGE_SIZE_TYPE;

/**
 * @typename `MESSAGE_RANGE_TYPE`
 *
 * @description
 * A number of messages.
 */
typedef   APEX_INTEGER     MESSAGE_RANGE_TYPE;

/**
 * @typename `PORT_DIRECTION_TYPE`
 *
 * @description
 * Whether a port sends or receives.
 */
typedef   enum {
   SOURCE = 0, /**< The port sends. */
   DESTINATION = 1 /**< The port receives. */
} PORT_DIRECTION_TYPE;

/**
 * @typename `QUEUING_DISCIPLINE_TYPE`
 *
 * @description
 * The order in which waiting processes get a resource.
 */
typedef   enum {
   FIFO = 0, /**< In the order of arrival. */
   /**
    * By priority (in the order of arrival for equal priorities).
    */
   PRIORITY = 1
} QUEUING_DISCIPLINE_TYPE;

/**
 * @typename `SYSTEM_TIME_TYPE`
 *
 * @description
 * 64-bit signed integer with a 1 nanosecond LSB.
 */
typedef   APEX_LONG_INTEGER     SYSTEM_TIME_TYPE;

/**
 * @macroname `INFINITE_TIME_VALUE`
 *
 * @description
 * An infinite time: no time-out, no period or no deadline.
 */
#define   INFINITE_TIME_VALUE       -1

/**
 * @typename `PROCESSOR_CORE_ID_TYPE`
 *
 * @description
 * The identifier of a processor core.
 */
typedef   APEX_INTEGER     PROCESSOR_CORE_ID_TYPE;

/**
 * @macroname `CORE_AFFINITY_NO_PREFERENCE`
 *
 * @description
 * Any processor core of the partition (CONFIGURE_ERROR_HANDLER()).
 */
#define   CORE_AFFINITY_NO_PREFERENCE  -1

#endif
