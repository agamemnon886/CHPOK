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
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_partition_mode_e (was pok_partition_mode_t), pok_start_condition_e (was
 * pok_start_condition_t); the Doxygen comment of the file is removed (header
 * files are not documented).
 */

#ifndef JET_UAPI_PARTITION_ARINC_H
#define JET_UAPI_PARTITION_ARINC_H

#include <uapi/partition_types.h>

/**
 * @typename `pok_partition_mode_e`
 *
 * @description
 * The different modes of a partition (ARINC 653 OPERATING_MODE_TYPE).
 */
typedef enum
{ 
   /**
    * In init mode, only main thread (process) is run. This's the only mode
    * where one can create various resources.
    *
    * There's really no difference between cold and warm init.
    *
    * When partition is initially started, it's in cold init.
    *
    * HM table and set_partition_mode function may restart partition into either
    * cold or warm init mode.
    *
    * The exact type of init can be introspected by an application, but
    * otherwise, it makes no difference.
    */
   POK_PARTITION_MODE_INIT_COLD = 1, 
   /**
    * Warm init: as cold init (see POK_PARTITION_MODE_INIT_COLD).
    */
   POK_PARTITION_MODE_INIT_WARM = 2,

   /**
    * In normal mode, all threads except main can be run.
    *
    * No resources can be allocated.
    */
   POK_PARTITION_MODE_NORMAL    = 3, 

   /**
    * Partition is stopped.
    */
   POK_PARTITION_MODE_IDLE      = 4,
}pok_partition_mode_e;

/**
 * @typename `pok_partition_status_t`
 *
 * @description
 * The status of a partition (ARINC 653 GET_PARTITION_STATUS).
 */
typedef struct {
   pok_partition_id_t id; /**< The identifier of the partition. */
   pok_time_t period; /**< Its period. */
   pok_time_t duration; /**< Its duration (time capacity) in each period. */
   /**
    * Its lock level (preemption is disabled when it is above 0).
    */
   int32_t lock_level;
   pok_partition_mode_e mode; /**< Its mode. */
   pok_start_condition_e  start_condition; /**< Why it was started. */
} pok_partition_status_t;

#endif /* JET_UAPI_PARTITION_ARINC_H */
