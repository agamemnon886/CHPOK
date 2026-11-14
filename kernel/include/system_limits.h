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
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

/*
 * System limits, used by the kernel.
 * 
 * Taken from Appendix E of ARINC653 P1-3.
 */

#ifndef POK_SYSTEM_LIMITS_H
#define POK_SYSTEM_LIMITS_H

// Number of partition should be contstrainted by integrator.

/**
 * @macroname `SYSTEM_LIMIT_NUMBER_OF_MESSAGES`
 *
 * @description
 * The maximum number of messages of a queuing port or buffer (module scope).
 */
#define SYSTEM_LIMIT_NUMBER_OF_MESSAGES       512

/**
 * @macroname `SYSTEM_LIMIT_MESSAGE_SIZE`
 *
 * @description
 * The maximum size of a message, in bytes (module scope).
 */
#define SYSTEM_LIMIT_MESSAGE_SIZE             8192

// SYSTEM_LIMIT_NUMBER_OF_.. should be constrainted by integrator.

/**
 * @macroname `MAX_LOCK_LEVEL`
 *
 * @description
 * The maximum lock level of a partition (LOCK_PREEMPTION).
 */
#define MAX_LOCK_LEVEL 16U

/**
 * @macroname `MIN_PRIORITY_VALUE`
 *
 * @description
 * The lowest priority of a process.
 */
#define MIN_PRIORITY_VALUE 1U

/**
 * @macroname `MAX_PRIORITY_VALUE`
 *
 * @description
 * The highest priority of a process.
 */
#define MAX_PRIORITY_VALUE 239U

#endif /* POK_SYSTEM_LIMITS_H */
