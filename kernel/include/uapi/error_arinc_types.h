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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-11-01: added the error handler
 * concurrency control values and POK_CORE_AFFINITY_NO_PREFERENCE, used by
 * CONFIGURE_ERROR_HANDLER (ARINC 653P1-4).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros; pok_error_handler_concurrency_t is renamed
 * pok_error_concurrency_t (names too long for the tables of the documentation).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef JET_UAPI_ERROR_ARINC_TYPES_H
#define JET_UAPI_ERROR_ARINC_TYPES_H

/**
 * @typename `pok_error_kind_t`
 *
 * @description
 * The kind of an error (POK_ERROR_KIND_*): the error codes of ARINC 653 and
 * those of the partition and kernel levels.
 */
typedef uint8_t pok_error_kind_t;

/**
 * @name Error kinds
 * The values of pok_error_kind_t: the errors of the processes (ARINC 653 error
 * codes), of the partitions and of the kernel.
 * @{
 */

/**
 * @macroname `POK_ERROR_KIND_INVALID`
 *
 * @description
 * This is "NULL" error - that is, no error.
 */
#define POK_ERROR_KIND_INVALID                   0

/**
 * @macroname `POK_ERROR_KIND_DEADLINE_MISSED`
 *
 * @description
 * A process missed its deadline.
 */
#define POK_ERROR_KIND_DEADLINE_MISSED          10

/**
 * @macroname `POK_ERROR_KIND_APPLICATION_ERROR`
 *
 * @description
 * An error raised by the application (RAISE_APPLICATION_ERROR).
 */
#define POK_ERROR_KIND_APPLICATION_ERROR        11

/**
 * @macroname `POK_ERROR_KIND_NUMERIC_ERROR`
 *
 * @description
 * A numeric error (e.g. a division by zero).
 */
#define POK_ERROR_KIND_NUMERIC_ERROR            12

/**
 * @macroname `POK_ERROR_KIND_ILLEGAL_REQUEST`
 *
 * @description
 * An illegal request (e.g. an invalid instruction or service call).
 */
#define POK_ERROR_KIND_ILLEGAL_REQUEST          13

/**
 * @macroname `POK_ERROR_KIND_STACK_OVERFLOW`
 *
 * @description
 * A process overflowed its stack.
 */
#define POK_ERROR_KIND_STACK_OVERFLOW           14

/**
 * @macroname `POK_ERROR_KIND_MEMORY_VIOLATION`
 *
 * @description
 * An access to memory the process may not access.
 */
#define POK_ERROR_KIND_MEMORY_VIOLATION         15

/**
 * @macroname `POK_ERROR_KIND_HARDWARE_FAULT`
 *
 * @description
 * A hardware fault.
 */
#define POK_ERROR_KIND_HARDWARE_FAULT           16

/**
 * @macroname `POK_ERROR_KIND_POWER_FAIL`
 *
 * @description
 * A power failure.
 */
#define POK_ERROR_KIND_POWER_FAIL               17

/**
 * @macroname `POK_ERROR_KIND_PARTITION_CONFIGURATION`
 *
 * @description
 * A partition-level error: in the configuration of the partition.
 */
#define POK_ERROR_KIND_PARTITION_CONFIGURATION  30 

/**
 * @macroname `POK_ERROR_KIND_PARTITION_INIT`
 *
 * @description
 * A partition-level error: during the initialization of the partition.
 */
#define POK_ERROR_KIND_PARTITION_INIT           31

/**
 * @macroname `POK_ERROR_KIND_PARTITION_SCHEDULING`
 *
 * @description
 * A partition-level error: in the scheduling of the partition.
 */
#define POK_ERROR_KIND_PARTITION_SCHEDULING     32

/**
 * @macroname `POK_ERROR_KIND_PARTITION_HANDLER`
 *
 * @description
 * A partition-level error: in the error handler of the partition.
 */
#define POK_ERROR_KIND_PARTITION_HANDLER        33

/**
 * @macroname `POK_ERROR_KIND_PARTITION_PROCESS`
 *
 * @description
 * A partition-level error: of a process of the partition.
 */
#define POK_ERROR_KIND_PARTITION_PROCESS        34

/**
 * @macroname `POK_ERROR_KIND_KERNEL_INIT`
 *
 * @description
 * A module-level error: during the initialization of the kernel.
 */
#define POK_ERROR_KIND_KERNEL_INIT              50

/**
 * @macroname `POK_ERROR_KIND_KERNEL_SCHEDULING`
 *
 * @description
 * A module-level error: in the scheduling of the kernel.
 */
#define POK_ERROR_KIND_KERNEL_SCHEDULING        51

/**
 * @macroname `POK_ERROR_KIND_KERNEL_CONFIG`
 *
 * @description
 * A module-level error: in the configuration of the kernel.
 */
#define POK_ERROR_KIND_KERNEL_CONFIG            52

/** @} */

/**
 * @typename `pok_error_status_t`
 *
 * @description
 * The status of the error that the error handler process handles
 * (GET_ERROR_STATUS): its kind, the process and address where it occurred and
 * the size of its message.
 */
typedef struct
{
   pok_error_kind_t     error_kind; /**< The kind of the error. */
   pok_thread_id_t      failed_thread; /**< The process in which it occurred. */
   /**
    * The address where it occurred (0 if there is none).
    */
   uintptr_t            failed_addr;
   size_t               msg_size; /**< The size of its message. */
} pok_error_status_t;

/**
 * @typename `pok_error_concurrency_t`
 *
 * @description
 * Error handler concurrency control, set by CONFIGURE_ERROR_HANDLER. The values
 * are those of ARINC ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE.
 */
typedef uint8_t pok_error_concurrency_t;

/**
 * @name Error handler concurrency
 * The values of pok_error_concurrency_t.
 * @{
 */

/**
 * @macroname `POK_ERROR_HANDLER_PROCESSES_PAUSE`
 *
 * @description
 * Other processes don't run.
 */
#define POK_ERROR_HANDLER_PROCESSES_PAUSE       0U

/**
 * @macroname `POK_ERROR_HANDLER_PROCESSES_SCHEDULED`
 *
 * @description
 * Other cores keep scheduling.
 */
#define POK_ERROR_HANDLER_PROCESSES_SCHEDULED   1U

/** @} */

/**
 * @macroname `POK_CORE_AFFINITY_NO_PREFERENCE`
 *
 * @description
 * Processor core ID meaning "any core of the partition" (ARINC
 * CORE_AFFINITY_NO_PREFERENCE).
 */
#define POK_CORE_AFFINITY_NO_PREFERENCE       (-1)

#endif /* JET_UAPI_ERROR_ARINC_TYPES_H */
