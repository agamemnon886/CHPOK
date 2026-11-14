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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_start_condition_e (was pok_start_condition_t); the Doxygen comment of the
 * file is removed (header files are not documented).
 */

#ifndef JET_UAPI_PARTITION_H
#define JET_UAPI_PARTITION_H

/**
 * @typename `pok_start_condition_e`
 *
 * @description
 * Why a partition was started (ARINC 653 START_CONDITION_TYPE).
 */
typedef enum
{
  /**
   * Normal start (power on).
   */
  POK_START_CONDITION_NORMAL_START          = 0,
  /**
   * Restarted by the partition (SET_PARTITION_MODE).
   */
  POK_START_CONDITION_PARTITION_RESTART     = 1,
  /**
   * Restarted by the health monitor at module level.
   */
  POK_START_CONDITION_HM_MODULE_RESTART     = 2,
  /**
   * Restarted by the health monitor at partition level.
   */
  POK_START_CONDITION_HM_PARTITION_RESTART  = 3
}pok_start_condition_e;

#endif /* JET_UAPI_PARTITION_H */
