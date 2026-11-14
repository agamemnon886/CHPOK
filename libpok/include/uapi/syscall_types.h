/*
 * COPIED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify original one (kernel/include/uapi/syscall_types.h).
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
 * Modified by Kirill Brazhnikov, 2026-10-31: removed
 * POK_SYSCALL_GET_BSP_INFO (PowerPC only).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: added
 * POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-01: added
 * POK_SYSCALL_ERROR_HANDLER_CONFIGURE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_syscall_id_e (was pok_syscall_id_t); the Doxygen comment of the file is
 * removed (header files are not documented); the descriptions that start with a
 * function name write it in lower case, as in the code.
 */

#ifndef LIBJET_SYSCALL_TYPES_H
#define LIBJET_SYSCALL_TYPES_H

#include <config.h>

/**
 * @typename `pok_syscall_id_e`
 *
 * @description
 * The ID of a syscall: the dispatcher (pok_core_syscall_internal()) calls the
 * kernel function given for each, with the arguments of the syscall. Some exist
 * only in the configurations that need them (POK_NEEDS_*).
 */
typedef enum
{
     /**
      * jet_console_write_user(): writes to the console.
      */
     POK_SYSCALL_CONSWRITE                           =  10,
     /**
      * pok_clock_gettime(): the time of a clock.
      */
     POK_SYSCALL_CLOCK_GETTIME                       =  20,
     /**
      * jet_time(): the calendar time.
      */
     POK_SYSCALL_TIME                                =  21,
     /**
      * The interrupt number of syscalls. Not used as a syscall.
      */
     POK_SYSCALL_INT_NUMBER                          =  42,
     /**
      * pok_thread_create().
      */
     POK_SYSCALL_THREAD_CREATE                       =  50,
     /**
      * pok_thread_sleep_until().
      */
     POK_SYSCALL_THREAD_SLEEP_UNTIL                  =  51,
#ifdef POK_NEEDS_THREAD_SLEEP
     /**
      * pok_thread_sleep().
      */
     POK_SYSCALL_THREAD_SLEEP                        =  52,
#endif /*POK_NEEDS_THREAD_SLEEP*/
     /**
      * pok_thread_suspend().
      */
     POK_SYSCALL_THREAD_SUSPEND                      =  53,
//     POK_SYSCALL_THREAD_RESTART                      =  54,
     /**
      * pok_thread_stop_target().
      */
     POK_SYSCALL_THREAD_STOP                         =  55,
     /**
      * pok_sched_end_period() (PERIODIC_WAIT).
      */
     POK_SYSCALL_THREAD_PERIOD                       =  56,
     /**
      * pok_thread_stop().
      */
     POK_SYSCALL_THREAD_STOPSELF                     =  57,
     POK_SYSCALL_THREAD_ID                           =  58, /**< Not used. */
     /**
      * pok_thread_get_status().
      */
     POK_SYSCALL_THREAD_STATUS                       =  59,
     /**
      * pok_thread_set_priority().
      */
     POK_SYSCALL_THREAD_SET_PRIORITY                 =  60,
     /**
      * pok_thread_resume().
      */
     POK_SYSCALL_THREAD_RESUME                       =  61,
     /**
      * pok_thread_suspend_target().
      */
     POK_SYSCALL_THREAD_SUSPEND_TARGET               =  62,
     POK_SYSCALL_THREAD_DEADLINE                     =  63, /**< Not used. */
     POK_SYSCALL_THREAD_STATE                        =  64, /**< Not used. */
     /**
      * pok_thread_delayed_start().
      */
     POK_SYSCALL_THREAD_DELAYED_START                =  65,
     /**
      * pok_thread_yield().
      */
     POK_SYSCALL_THREAD_YIELD                        =  66,
     /**
      * pok_sched_replenish().
      */
     POK_SYSCALL_THREAD_REPLENISH                    =  67,
     /**
      * pok_thread_find() (GET_PROCESS_ID).
      */
     POK_SYSCALL_THREAD_FIND                         =  68,

     /**
      * jet_resched().
      */
     POK_SYSCALL_RESCHED                             =  80,
     /**
      * jet_msection_enter_helper().
      */
     POK_SYSCALL_MSECTION_ENTER_HELPER               =  81,
     /**
      * jet_msection_wait().
      */
     POK_SYSCALL_MSECTION_WAIT                       =  82,
     /**
      * jet_msection_notify().
      */
     POK_SYSCALL_MSECTION_NOTIFY                     =  83,
     /**
      * jet_msection_wq_notify().
      */
     POK_SYSCALL_MSECTION_WQ_NOTIFY                  =  84,
     /**
      * jet_msection_wq_size().
      */
     POK_SYSCALL_MSECTION_WQ_SIZE                    =  85,

#ifdef POK_NEEDS_PORTS_SAMPLING
     /**
      * pok_port_sampling_id().
      */
     POK_SYSCALL_MIDDLEWARE_SAMPLING_ID              = 101,
     /**
      * pok_port_sampling_read().
      */
     POK_SYSCALL_MIDDLEWARE_SAMPLING_READ            = 102,
     /**
      * pok_port_sampling_status().
      */
     POK_SYSCALL_MIDDLEWARE_SAMPLING_STATUS          = 103,
     /**
      * pok_port_sampling_write().
      */
     POK_SYSCALL_MIDDLEWARE_SAMPLING_WRITE           = 104,
     /**
      * pok_port_sampling_create().
      */
     POK_SYSCALL_MIDDLEWARE_SAMPLING_CREATE          = 105,
     /**
      * pok_port_sampling_check().
      */
     POK_SYSCALL_MIDDLEWARE_SAMPLING_CHECK           = 106,
#endif
#ifdef POK_NEEDS_PORTS_QUEUEING
     /**
      * pok_port_queuing_create_packed().
      */
     POK_SYSCALL_MIDDLEWARE_QUEUEING_CREATE          = 110,
     /**
      * pok_port_queuing_send().
      */
     POK_SYSCALL_MIDDLEWARE_QUEUEING_SEND            = 111,
     /**
      * pok_port_queuing_receive().
      */
     POK_SYSCALL_MIDDLEWARE_QUEUEING_RECEIVE         = 112,
     /**
      * pok_port_queuing_id().
      */
     POK_SYSCALL_MIDDLEWARE_QUEUEING_ID              = 113,
     /**
      * pok_port_queuing_status().
      */
     POK_SYSCALL_MIDDLEWARE_QUEUEING_STATUS          = 114,
     /**
      * pok_port_queuing_clear().
      */
     POK_SYSCALL_MIDDLEWARE_QUEUEING_CLEAR           = 115,
#endif

#ifdef POK_NEEDS_ERROR_HANDLING
     /**
      * pok_error_thread_create().
      */
     POK_SYSCALL_ERROR_HANDLER_CREATE                = 301,
     /**
      * pok_error_thread_configure().
      */
     POK_SYSCALL_ERROR_HANDLER_CONFIGURE             = 302,
     /**
      * pok_error_raise_application_error().
      */
     POK_SYSCALL_ERROR_RAISE_APPLICATION_ERROR       = 303,
     /**
      * pok_error_get().
      */
     POK_SYSCALL_ERROR_GET                           = 304,
     POK_SYSCALL_ERROR_IS_HANDLER                    = 305, /**< Not used. */
     /**
      * pok_error_report_application_message().
      */
     POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE    = 306,
#endif

     /**
      * pok_error_raise_os_error().
      */
     POK_SYSCALL_ERROR_RAISE_OS_ERROR       = 310,

#ifdef POK_NEEDS_PARTITIONS
     /**
      * pok_partition_set_mode_current().
      */
     POK_SYSCALL_PARTITION_SET_MODE                  = 404,
     /**
      * pok_current_partition_get_status().
      */
     POK_SYSCALL_PARTITION_GET_STATUS                = 405,
     /**
      * pok_current_partition_inc_lock_level().
      */
     POK_SYSCALL_PARTITION_INC_LOCK_LEVEL            = 411,
     /**
      * pok_current_partition_dec_lock_level().
      */
     POK_SYSCALL_PARTITION_DEC_LOCK_LEVEL            = 412,
#endif
#ifdef POK_NEEDS_IO
     /**
      * Reads a byte from an I/O port (x86_64).
      */
     POK_SYSCALL_INB                                 = 501,
     /**
      * Writes a byte to an I/O port (x86_64).
      */
     POK_SYSCALL_OUTB                                = 502,
#endif
#ifdef POK_NEEDS_PCI
     POK_SYSCALL_PCI_REGISTER                        = 601, /**< Not used. */
#endif
     /**
      * The physical address of a partition address.
      */
     POK_SYSCALL_MEM_VIRT_TO_PHYS                    = 701,
     /**
      * The partition address of a physical address.
      */
     POK_SYSCALL_MEM_PHYS_TO_VIRT                    = 702,

     /**
      * pok_memory_block_get_status().
      */
     POK_SYSCALL_MEMORY_BLOCK_GET_STATUS             = 704,
} pok_syscall_id_e;

#endif /* LIBJET_SYSCALL_TYPES_H */
