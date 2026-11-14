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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros MAP_ERROR() and
 * MAP_ERROR_DEFAULT() are moved to libpok/include/arinc653/arincutils.h; the
 * two MAP() of GET_PARTITION_STATUS() are MAP_OPERATING_MODE() and
 * MAP_START_CONDITION(), in libpok/include/arinc653/arincutils.h.
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_partition_mode_e (was pok_partition_mode_t), pok_ret_e (was pok_ret_t).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * ARINC 653 partition services: GET_PARTITION_STATUS and
 * SET_PARTITION_MODE.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_PARTITION

#include <arinc653/types.h>
#include <arinc653/partition.h>

#include <core/partition.h>

#include <errno.h>
#include <utils.h>
#include <core/assert_os.h>
#include <arinc653/arincutils.h>

/**
 * @funcname `GET_PARTITION_STATUS`
 *
 * @description
 * Returns the status of the partition (ARINC 653 service): its ID, period,
 * duration, lock level, operating mode, start condition and number of cores
 * (1).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PARTITION_STATUS` | `PARTITION_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `core_status` | `pok_partition_status_t` | The kernel's status |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_current_partition_get_status()` with the address of
 *    `core_status`, and store the result in `core_ret`.
 * 2. Check that `core_ret == POK_ERRNO_OK` holds (assert_os: stop otherwise).
 * 3. Set `PARTITION_STATUS->IDENTIFIER` to `core_status.id`.
 * 4. Set `PARTITION_STATUS->PERIOD` to `core_status.period`.
 * 5. Set `PARTITION_STATUS->DURATION` to `core_status.duration`.
 * 6. Set `PARTITION_STATUS->LOCK_LEVEL` to `core_status.lock_level`.
 * 7. Depending on `core_status.mode`:
 *    1. Case `MAP_OPERATING_MODE(POK_PARTITION_MODE_IDLE, IDLE)`:
 *       1. Use the macro `MAP_OPERATING_MODE()` with the arguments
 *          `POK_PARTITION_MODE_IDLE` and `IDLE`.
 *       2. Leave the switch.
 *    2. Case `MAP_OPERATING_MODE(POK_PARTITION_MODE_NORMAL, NORMAL)`:
 *       1. Use the macro `MAP_OPERATING_MODE()` with the arguments
 *          `POK_PARTITION_MODE_NORMAL` and `NORMAL`.
 *       2. Leave the switch.
 *    3. Case `MAP_OPERATING_MODE(POK_PARTITION_MODE_INIT_COLD, COLD_START)`:
 *       1. Use the macro `MAP_OPERATING_MODE()` with the arguments
 *          `POK_PARTITION_MODE_INIT_COLD` and `COLD_START`.
 *       2. Leave the switch.
 *    4. Case `MAP_OPERATING_MODE(POK_PARTITION_MODE_INIT_WARM, WARM_START)`:
 *       1. Use the macro `MAP_OPERATING_MODE()` with the arguments
 *          `POK_PARTITION_MODE_INIT_WARM` and `WARM_START`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Check that `FALSE` holds (assert_os: stop otherwise).
 *       2. Leave the switch.
 * 8. Depending on `core_status.start_condition`:
 *    1. Case `MAP_START_CONDITION(POK_START_CONDITION_PARTITION_RESTART,
 *       PARTITION_RESTART)`:
 *       1. Use the macro `MAP_START_CONDITION()` with the arguments
 *          `POK_START_CONDITION_PARTITION_RESTART` and `PARTITION_RESTART`.
 *       2. Leave the switch.
 *    2. Case `MAP_START_CONDITION(POK_START_CONDITION_HM_MODULE_RESTART,
 *       HM_MODULE_RESTART)`:
 *       1. Use the macro `MAP_START_CONDITION()` with the arguments
 *          `POK_START_CONDITION_HM_MODULE_RESTART` and `HM_MODULE_RESTART`.
 *       2. Leave the switch.
 *    3. Case `MAP_START_CONDITION(POK_START_CONDITION_HM_PARTITION_RESTART,
 *       HM_PARTITION_RESTART)`:
 *       1. Use the macro `MAP_START_CONDITION()` with the arguments
 *          `POK_START_CONDITION_HM_PARTITION_RESTART` and
 *          `HM_PARTITION_RESTART`.
 *       2. Leave the switch.
 *    4. Case
 *       `MAP_START_CONDITION(POK_START_CONDITION_NORMAL_START, NORMAL_START)`:
 *       1. Use the macro `MAP_START_CONDITION()` with the arguments
 *          `POK_START_CONDITION_NORMAL_START` and `NORMAL_START`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Set `PARTITION_STATUS->START_CONDITION` to `NORMAL_START`.
 *       2. Leave the switch.
 * 9. Set `PARTITION_STATUS->NUM_ASSIGNED_CORES` to `1`.
 * 10. Set `*RETURN_CODE` to `NO_ERROR`.
 *
 * @calls
 * - `pok_current_partition_get_status()`
 * - `assertion_os_fail()`
 */
void GET_PARTITION_STATUS (PARTITION_STATUS_TYPE *PARTITION_STATUS,
                           RETURN_CODE_TYPE      *RETURN_CODE)
{
  pok_partition_status_t core_status;

  // Can't fail: core_status is a local object.
  pok_ret_e core_ret = pok_current_partition_get_status(&core_status);
  assert_os(core_ret == POK_ERRNO_OK);

  PARTITION_STATUS->IDENTIFIER = core_status.id;
  PARTITION_STATUS->PERIOD = core_status.period;
  PARTITION_STATUS->DURATION = core_status.duration;
  PARTITION_STATUS->LOCK_LEVEL = core_status.lock_level;

  switch (core_status.mode) {
    MAP_OPERATING_MODE(POK_PARTITION_MODE_IDLE, IDLE);
    MAP_OPERATING_MODE(POK_PARTITION_MODE_NORMAL, NORMAL);
    MAP_OPERATING_MODE(POK_PARTITION_MODE_INIT_COLD, COLD_START);
    MAP_OPERATING_MODE(POK_PARTITION_MODE_INIT_WARM, WARM_START);
    default: assert_os(FALSE); break;   // the kernel has no other modes
  }

  switch (core_status.start_condition) {
    MAP_START_CONDITION(POK_START_CONDITION_PARTITION_RESTART,
                        PARTITION_RESTART);
    MAP_START_CONDITION(POK_START_CONDITION_HM_MODULE_RESTART,
                        HM_MODULE_RESTART);
    MAP_START_CONDITION(POK_START_CONDITION_HM_PARTITION_RESTART,
                        HM_PARTITION_RESTART);
    MAP_START_CONDITION(POK_START_CONDITION_NORMAL_START, NORMAL_START);
    default: PARTITION_STATUS->START_CONDITION = NORMAL_START; break;
  }

  // The kernel is single-core: each partition runs on one core.
  PARTITION_STATUS->NUM_ASSIGNED_CORES = 1;

  *RETURN_CODE = NO_ERROR;
}

/**
 * @funcname `SET_PARTITION_MODE`
 *
 * @description
 * Sets the operating mode of the partition (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `OPERATING_MODE` | `OPERATING_MODE_TYPE` | [in] The mode: IDLE, NORMAL, COLD_START or WARM_START. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION, INVALID_MODE or INVALID_PARAM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_mode` | `pok_partition_mode_e` | The kernel's mode |
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Depending on `OPERATING_MODE`:
 *    1. Case `IDLE`:
 *       1. Set `core_mode` to `POK_PARTITION_MODE_IDLE`.
 *       2. Leave the switch.
 *    2. Case `NORMAL`:
 *       1. Set `core_mode` to `POK_PARTITION_MODE_NORMAL`.
 *       2. Leave the switch.
 *    3. Case `COLD_START`:
 *       1. Set `core_mode` to `POK_PARTITION_MODE_INIT_COLD`.
 *       2. Leave the switch.
 *    4. Case `WARM_START`:
 *       1. Set `core_mode` to `POK_PARTITION_MODE_INIT_WARM`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *       2. Return.
 * 2. Call `pok_partition_set_mode_current()` with the argument `core_mode`, and
 *    store the result in `core_ret`.
 * 3. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_PARTITION_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_PARTITION_MODE` and `INVALID_MODE`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_partition_set_mode_current()`
 */
void SET_PARTITION_MODE (OPERATING_MODE_TYPE OPERATING_MODE,
                         RETURN_CODE_TYPE *RETURN_CODE)
{
  pok_partition_mode_e core_mode;
  pok_ret_e            core_ret;

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
