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
 * Modified by Kirill Brazhnikov, 2026-10-31: GET_PARTITION_STATUS
 * converts the start condition to START_CONDITION_TYPE explicitly and
 * fills NUM_ASSIGNED_CORES.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the status
 * switches have a default clause and the status syscall is checked.
 * Modified by Kirill Brazhnikov, 2026-11-03: the parameters have the names of
 * the declarations in <arinc653/partition.h> (Rule 8.3).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_PARTITION

#include <arinc653/types.h>
#include <arinc653/partition.h>

#include <core/partition.h>

#include <errno.h>
#include <utils.h>
#include <core/assert_os.h>

/*
 * Each MAP_ERROR()/MAP_ERROR_DEFAULT() expands to a whole switch-clause ending
 * with break (MISRA C:2025 Rule 16.3); cppcheck's MISRA addon doesn't see the
 * break inside the macro and reports these switches by mistake.
 */
#define MAP_ERROR(from, to) case (from): *RETURN_CODE = (to); break
#define MAP_ERROR_DEFAULT(to) default: *RETURN_CODE = (to); break

void GET_PARTITION_STATUS (PARTITION_STATUS_TYPE *PARTITION_STATUS,
                           RETURN_CODE_TYPE      *RETURN_CODE)
{
  pok_partition_status_t core_status;

  // Can't fail: core_status is a local object.
  pok_ret_t core_ret = pok_current_partition_get_status(&core_status);
  assert_os(core_ret == POK_ERRNO_OK);

  PARTITION_STATUS->IDENTIFIER = core_status.id;
  PARTITION_STATUS->PERIOD = core_status.period;
  PARTITION_STATUS->DURATION = core_status.duration;
  PARTITION_STATUS->LOCK_LEVEL = core_status.lock_level;

#define MAP(from, to) \
    case (from): PARTITION_STATUS->OPERATING_MODE = (to); break
  switch (core_status.mode) {
    MAP(POK_PARTITION_MODE_IDLE, IDLE);
    MAP(POK_PARTITION_MODE_NORMAL, NORMAL);
    MAP(POK_PARTITION_MODE_INIT_COLD, COLD_START);
    MAP(POK_PARTITION_MODE_INIT_WARM, WARM_START);
    default: assert_os(FALSE); break;   // the kernel has no other modes
  }
#undef MAP

#define MAP(from, to) \
    case (from): PARTITION_STATUS->START_CONDITION = (to); break
  switch (core_status.start_condition) {
    MAP(POK_START_CONDITION_PARTITION_RESTART, PARTITION_RESTART);
    MAP(POK_START_CONDITION_HM_MODULE_RESTART, HM_MODULE_RESTART);
    MAP(POK_START_CONDITION_HM_PARTITION_RESTART, HM_PARTITION_RESTART);
    MAP(POK_START_CONDITION_NORMAL_START, NORMAL_START);
    default: PARTITION_STATUS->START_CONDITION = NORMAL_START; break;
  }
#undef MAP

  // The kernel is single-core: each partition runs on one core.
  PARTITION_STATUS->NUM_ASSIGNED_CORES = 1;

  *RETURN_CODE = NO_ERROR;
}

void SET_PARTITION_MODE (OPERATING_MODE_TYPE OPERATING_MODE,
                         RETURN_CODE_TYPE *RETURN_CODE)
{
  pok_partition_mode_t core_mode;
  pok_ret_t            core_ret;

   switch (OPERATING_MODE)
   {
      case IDLE:
         core_mode = POK_PARTITION_MODE_IDLE;
         break;

      case NORMAL:
         core_mode = POK_PARTITION_MODE_NORMAL;
         break;

      case COLD_START:
         core_mode = POK_PARTITION_MODE_INIT_COLD;
         break;

      case WARM_START:
         core_mode = POK_PARTITION_MODE_INIT_WARM;
         break;

      default:
         *RETURN_CODE = INVALID_PARAM;
         return;
   }

   core_ret = pok_partition_set_mode (core_mode);

   switch (core_ret) {
      MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
      MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
      MAP_ERROR(POK_ERRNO_PARTITION_MODE, INVALID_MODE);
      MAP_ERROR_DEFAULT(INVALID_PARAM);
   }
}

#endif
