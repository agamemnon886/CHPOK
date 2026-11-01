/*
 * COPIED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify original one (kernel/include/uapi/error_arinc_types.h).
 */
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
 */

#ifndef __JET_UAPI_ERROR_ARINC_TYPES_H__
#define __JET_UAPI_ERROR_ARINC_TYPES_H__

typedef uint8_t pok_error_kind_t;
#define POK_ERROR_KIND_INVALID                   0 // this is "NULL" error - that is, no error
#define POK_ERROR_KIND_DEADLINE_MISSED          10
#define POK_ERROR_KIND_APPLICATION_ERROR        11
#define POK_ERROR_KIND_NUMERIC_ERROR            12
#define POK_ERROR_KIND_ILLEGAL_REQUEST          13
#define POK_ERROR_KIND_STACK_OVERFLOW           14
#define POK_ERROR_KIND_MEMORY_VIOLATION         15
#define POK_ERROR_KIND_HARDWARE_FAULT           16
#define POK_ERROR_KIND_POWER_FAIL               17
#define POK_ERROR_KIND_PARTITION_CONFIGURATION  30 
#define POK_ERROR_KIND_PARTITION_INIT           31
#define POK_ERROR_KIND_PARTITION_SCHEDULING     32
#define POK_ERROR_KIND_PARTITION_HANDLER        33
#define POK_ERROR_KIND_PARTITION_PROCESS        34
#define POK_ERROR_KIND_KERNEL_INIT              50
#define POK_ERROR_KIND_KERNEL_SCHEDULING        51
#define POK_ERROR_KIND_KERNEL_CONFIG            52

typedef struct
{
   pok_error_kind_t     error_kind;
   pok_thread_id_t      failed_thread;
   uintptr_t            failed_addr;
   size_t               msg_size;
} pok_error_status_t;

/*
 * Error handler concurrency control, set by CONFIGURE_ERROR_HANDLER.
 * The values are those of ARINC ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE.
 */
typedef uint8_t pok_error_handler_concurrency_t;
#define POK_ERROR_HANDLER_PROCESSES_PAUSE       0 // other processes don't run
#define POK_ERROR_HANDLER_PROCESSES_SCHEDULED   1 // other cores keep scheduling

/* Processor core ID meaning "any core of the partition" (ARINC CORE_AFFINITY_NO_PREFERENCE). */
#define POK_CORE_AFFINITY_NO_PREFERENCE       (-1)

#endif /* __JET_UAPI_ERROR_ARINC_TYPES_H__ */
