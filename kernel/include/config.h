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
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its macros; the unused macros POK_NEEDS_SERIAL, POK_NEEDS_THREAD_ID,
 * POK_NEEDS_SEMAPHORES and POK_TEST_SUPPORT_PRINT_WHEN_ALL_THREADS_STOPPED are
 * removed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

/**
 * @name Kernel features
 * The features of the kernel (POK_NEEDS_*): defined to 1 when the kernel is
 * built with them.
 * @{
 */

/**
 * @macroname `POK_NEEDS_THREADS`
 *
 * @description
 * Threads (ARINC 653 processes).
 */
#define POK_NEEDS_THREADS      1

/**
 * @macroname `POK_NEEDS_PARTITIONS`
 *
 * @description
 * Partitions.
 */
#define POK_NEEDS_PARTITIONS   1

/**
 * @macroname `POK_NEEDS_SCHED`
 *
 * @description
 * The scheduler.
 */
#define POK_NEEDS_SCHED        1

/**
 * @macroname `POK_NEEDS_TIME`
 *
 * @description
 * The system time.
 */
#define POK_NEEDS_TIME         1

/**
 * @macroname `POK_NEEDS_GETTICK`
 *
 * @description
 * The clock_gettime syscall.
 */
#define POK_NEEDS_GETTICK      1

/**
 * @macroname `POK_NEEDS_DEBUG`
 *
 * @description
 * Debug output (printf() of the kernel).
 */
#define POK_NEEDS_DEBUG        1

/**
 * @macroname `POK_NEEDS_CONSOLE`
 *
 * @description
 * The consoles.
 */
#define POK_NEEDS_CONSOLE      1

/**
 * @macroname `POK_NEEDS_ERROR_HANDLING`
 *
 * @description
 * Health monitoring (error handlers).
 */
#define POK_NEEDS_ERROR_HANDLING 1

/**
 * @macroname `POK_NEEDS_THREAD_SUSPEND`
 *
 * @description
 * Suspension of threads.
 */
#define POK_NEEDS_THREAD_SUSPEND 1

/**
 * @macroname `POK_NEEDS_THREAD_SLEEP`
 *
 * @description
 * The sleep of a thread for a duration.
 */
#define POK_NEEDS_THREAD_SLEEP 1

/**
 * @macroname `POK_NEEDS_MONITOR`
 *
 * @description
 * The kernel monitor.
 */
#define POK_NEEDS_MONITOR 1

#ifndef POK_DISABLE_GDB

/**
 * @macroname `POK_NEEDS_GDB`
 *
 * @description
 * The GDB server, unless POK_DISABLE_GDB is defined. One may set option
 * POK_DISABLE_GDB for some arch/board (in CFLAGS in misc/SConscript), so JET OS
 * will be built on that arch/board without gdb.
 */
#define POK_NEEDS_GDB 1
#endif

/**
 * @macroname `POK_NEEDS_PORTS_SAMPLING`
 *
 * @description
 * Sampling ports.
 */
#define POK_NEEDS_PORTS_SAMPLING 1

/**
 * @macroname `POK_NEEDS_PORTS_QUEUEING`
 *
 * @description
 * Queuing ports.
 */
#define POK_NEEDS_PORTS_QUEUEING 1

/**
 * @macroname `POK_NEEDS_BUFFERS`
 *
 * @description
 * Buffers.
 */
#define POK_NEEDS_BUFFERS 1

/**
 * @macroname `POK_NEEDS_BLACKBOARDS`
 *
 * @description
 * Blackboards.
 */
#define POK_NEEDS_BLACKBOARDS 1

/**
 * @macroname `POK_NEEDS_EVENTS`
 *
 * @description
 * Events.
 */
#define POK_NEEDS_EVENTS 1

// TODO: Is this needed?

/** @} */

