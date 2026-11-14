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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t); the Doxygen comment of the file is removed (header
 * files are not documented).
 */

#ifndef JET_UAPI_ERRNO_H
#define JET_UAPI_ERRNO_H

/**
 * @typename `pok_ret_e`
 *
 * @description
 * The result of a kernel service (a syscall): POK_ERRNO_OK, or the reason of
 * the failure. libpok turns it into the ARINC 653 return code of the service.
 */
typedef enum
{
		POK_ERRNO_OK                    =   0, /**< Success. */
		/**
		 * An invalid argument.
		 */
		POK_ERRNO_EINVAL                =   1,

		/**
		 * The service or the object is unavailable (in this
		 * configuration or state).
		 */
		POK_ERRNO_UNAVAILABLE           =   2,
		/**
		 * An invalid parameter.
		 */
		POK_ERRNO_PARAM									=   3,
		/**
		 * Too many objects: no room for another one.
		 */
		POK_ERRNO_TOOMANY               =   5,
		POK_ERRNO_EPERM                 =   6, /**< Not permitted. */
		/**
		 * The object already exists (its name is taken).
		 */
		POK_ERRNO_EXISTS                =   7,

		/**
		 * A value out of range.
		 */
		POK_ERRNO_ERANGE                =   8,
		/**
		 * A math argument out of the domain of the function.
		 */
		POK_ERRNO_EDOM                  =   9,
		/**
		 * A math result too large (used by libm).
		 */
		POK_ERRNO_HUGE_VAL              =  10,

		/**
		 * An address that isn't valid for the partition (a user
		 * pointer).
		 */
		POK_ERRNO_EFAULT                =  11,

		/**
		 * An invalid process (thread).
		 */
		POK_ERRNO_THREAD                =  49,
		/**
		 * Invalid attributes of a process.
		 */
		POK_ERRNO_THREADATTR            =  50,

		POK_ERRNO_TIME                 =  100, /**< An invalid time. */

		/**
		 * Invalid attributes of a partition. Not used.
		 */
		POK_ERRNO_PARTITION_ATTR        = 200,

		POK_ERRNO_PORT                 =  301, /**< An invalid port. */
		/**
		 * Not found: no object with that name or ID.
		 */
		POK_ERRNO_NOTFOUND             =  302,
		/**
		 * The port has the wrong direction for the operation.
		 */
		POK_ERRNO_DIRECTION            =  303,
		/**
		 * An invalid size (of a message or a buffer).
		 */
		POK_ERRNO_SIZE                 =  304,
		/**
		 * An invalid queuing discipline.
		 */
		POK_ERRNO_DISCIPLINE           =  305,
		/**
		 * The port belongs to another partition. Not used.
		 */
		POK_ERRNO_PORTPART             =  307,
		/**
		 * Empty: no message to receive.
		 */
		POK_ERRNO_EMPTY                =  308,
		/**
		 * The object is of the wrong kind. Not used.
		 */
		POK_ERRNO_KIND                 =  309,
		/**
		 * Full: no room for the message.
		 */
		POK_ERRNO_FULL                 =  311,
		/**
		 * Not ready. Not used.
		 */
		POK_ERRNO_READY                =  310,
		/**
		 * The time-out expired.
		 */
		POK_ERRNO_TIMEOUT              =  250,
		/**
		 * Not allowed in the current mode (of the partition or the
		 * process).
		 */
		POK_ERRNO_MODE                 =  251,

		/**
		 * Native thread is STOP()-ed or server thread is cancelled.
		 */
		POK_ERRNO_CANCELLED            =  500,

		/**
		 * An invalid partition mode.
		 */
		POK_ERRNO_PARTITION_MODE       =  601,

		/**
		 * An invalid partition. Not used.
		 */
		POK_ERRNO_PARTITION            =  401
} pok_ret_e;

#endif /* JET_UAPI_ERRNO_H */
