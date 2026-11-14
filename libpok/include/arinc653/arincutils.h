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
 * Created by matias on Wed May 04 09:34:13 2012 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of
 * libpok/arinc653/error.c, partition.c, process.c, queueing.c, sampling.c and
 * time.c are moved here; an include guard is added.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef LIBJET_ARINC_ARINCUTILS_H
#define LIBJET_ARINC_ARINCUTILS_H

#include <config.h>

#ifdef POK_NEEDS_ARINC653_PROCESS

#include <arinc653/process.h>

/*
 * Struct for save data
 * NAME => Not use by pok
 * BASE_PRIORITY => This value, in pok, is modified. Here we save the base value
 */
/**
 * @typename `ARINC_ATTRIBUTE`
 *
 * @description
 * The attributes of a process that the kernel doesn't keep, saved by libpok:
 * the kernel changes the priority of a thread, so the base priority is saved
 * here.
 */
typedef struct {
  PROCESS_NAME_TYPE       NAME; /**< The name of the process (not used). */
  /**
   * The base priority of the process.
   */
  PRIORITY_TYPE           BASE_PRIORITY;
  /**
   * The size of the stack of the process.
   */
  STACK_SIZE_TYPE         STACK_SIZE;
} ARINC_ATTRIBUTE;

#endif

/**
 * @macroname `MAP_ERROR`
 *
 * @description
 * A case of a switch on a kernel result: for the result `from`, sets
 * `*RETURN_CODE` (the return code of the service, a parameter of the function
 * that uses the macro) to `to` and leaves the switch.
 *
 * Each MAP_ERROR()/MAP_ERROR_DEFAULT() expands to a whole switch-clause ending
 * with break (MISRA C:2025 Rule 16.3); cppcheck's MISRA addon doesn't see the
 * break inside the macro and reports these switches by mistake.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `from` | The kernel result (POK_ERRNO_*). |
 * | `to` | The return code. |
 */
#define MAP_ERROR(from, to) case (from): *RETURN_CODE = (to); break

/**
 * @macroname `MAP_ERROR_DEFAULT`
 *
 * @description
 * The default case of a switch on a kernel result: sets `*RETURN_CODE` to `to`
 * and leaves the switch (see MAP_ERROR()).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `to` | The return code. |
 */
#define MAP_ERROR_DEFAULT(to) default: *RETURN_CODE = (to); break

/**
 * @macroname `MAP_ERROR_CODE`
 *
 * @description
 * A case of the switch of error_pok_to_arinc(): for the kernel error kind
 * `from`, returns the error code `to`.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `from` | The kernel error kind (POK_ERROR_KIND_*). |
 * | `to` | The ARINC 653 error code. |
 */
#define MAP_ERROR_CODE(from, to) case (from): return (to)

/**
 * @macroname `MAP_OPERATING_MODE`
 *
 * @description
 * A case of the switch on the kernel's mode of the partition in
 * GET_PARTITION_STATUS(): for the mode `from`, sets
 * `PARTITION_STATUS->OPERATING_MODE` to `to` and leaves the switch.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `from` | The kernel's mode (POK_PARTITION_MODE_*). |
 * | `to` | The ARINC 653 operating mode. |
 */
#define MAP_OPERATING_MODE(from, to) \
    case (from): PARTITION_STATUS->OPERATING_MODE = (to); break

/**
 * @macroname `MAP_START_CONDITION`
 *
 * @description
 * A case of the switch on the kernel's start condition of the partition in
 * GET_PARTITION_STATUS(): for the condition `from`, sets
 * `PARTITION_STATUS->START_CONDITION` to `to` and leaves the switch.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `from` | The kernel's start condition (POK_START_CONDITION_*). |
 * | `to` | The ARINC 653 start condition. |
 */
#define MAP_START_CONDITION(from, to) \
    case (from): PARTITION_STATUS->START_CONDITION = (to); break

/**
 * @macroname `CHECK_PROCESS_ID`
 *
 * @description
 * Returns from the service with `*RETURN_CODE` set to INVALID_PARAM if
 * `PROCESS_ID` (a parameter of the function that uses the macro) is 0, which is
 * no process.
 */
#define CHECK_PROCESS_ID() \
    if (PROCESS_ID == 0) { \
        *RETURN_CODE = INVALID_PARAM; \
        return; \
    }

/**
 * @macroname `MAP_STATUS`
 *
 * @description
 * A case of the switch on the kernel's state of the process in
 * GET_PROCESS_STATUS(): for the state `from`, sets
 * `PROCESS_STATUS->PROCESS_STATE` to `to` and leaves the switch.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `from` | The kernel's state (POK_STATE_*). |
 * | `to` | The ARINC 653 process state. |
 */
#define MAP_STATUS(from, to) \
    case (from): PROCESS_STATUS->PROCESS_STATE = (to); break

/**
 * @macroname `MAP_TIME_ERROR`
 *
 * @description
 * A case of a switch on a kernel result in the time services: for the result
 * `from`, sets `*return_code` (the return code of the service, a parameter of
 * the function that uses the macro) to `to` and leaves the switch.
 *
 * Each MAP_TIME_ERROR()/MAP_TIME_ERROR_DEFAULT() expands to a whole
 * switch-clause ending with break (MISRA C:2025 Rule 16.3); cppcheck's MISRA
 * addon doesn't see the break inside the macro and reports these switches by
 * mistake.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `from` | The kernel result (POK_ERRNO_*). |
 * | `to` | The return code. |
 */
#define MAP_TIME_ERROR(from, to) case (from): *return_code = (to); break

/**
 * @macroname `MAP_TIME_ERROR_DEFAULT`
 *
 * @description
 * The default case of a switch on a kernel result in the time services: sets
 * `*return_code` to `to` and leaves the switch (see MAP_TIME_ERROR()).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `to` | The return code. |
 */
#define MAP_TIME_ERROR_DEFAULT(to) default: *return_code = (to); break

#endif /* LIBJET_ARINC_ARINCUTILS_H */
