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
 * Modified by Kirill Brazhnikov, 2026-10-31: added NUM_CORES_TYPE and
 * the NUM_ASSIGNED_CORES field of PARTITION_STATUS_TYPE (ARINC 653P1-4).
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_PARTITION

#include <arinc653/types.h>
#include <arinc653/process.h>

#ifndef APEX_PARTITION
#define APEX_PARTITION

/**
 * @macroname `MAX_NUMBER_OF_PARTITIONS`
 *
 * @description
 * The maximum number of partitions of the module
 * (SYSTEM_LIMIT_NUMBER_OF_PARTITIONS).
 */
#define   MAX_NUMBER_OF_PARTITIONS   SYSTEM_LIMIT_NUMBER_OF_PARTITIONS

/**
 * @typename `OPERATING_MODE_TYPE`
 *
 * @description
 * The operating mode of a partition.
 */
typedef enum
{
   IDLE       = 0, /**< The partition is stopped. */
   COLD_START = 1, /**< The partition is initializing after a cold start. */
   WARM_START = 2, /**< The partition is initializing after a warm start. */
   NORMAL     = 3 /**< The partition runs its processes. */
} OPERATING_MODE_TYPE;

/**
 * @typename `PARTITION_ID_TYPE`
 *
 * @description
 * The identifier of a partition.
 */
typedef   APEX_INTEGER     PARTITION_ID_TYPE;

/**
 * @typename `START_CONDITION_TYPE`
 *
 * @description
 * Why a partition was (re)started.
 */
typedef enum
{
   NORMAL_START          = 0, /**< The module was powered on. */
   /**
    * The partition restarted itself (SET_PARTITION_MODE()).
    */
   PARTITION_RESTART     = 1,
   HM_MODULE_RESTART     = 2, /**< Health monitoring restarted the module. */
   HM_PARTITION_RESTART  = 3 /**< Health monitoring restarted the partition. */
} START_CONDITION_TYPE;

/**
 * @typename `NUM_CORES_TYPE`
 *
 * @description
 * A number of processor cores.
 */
typedef   APEX_UNSIGNED    NUM_CORES_TYPE;

/**
 * @typename `PARTITION_STATUS_TYPE`
 *
 * @description
 * The status of the partition (GET_PARTITION_STATUS()).
 */
typedef struct {
   PARTITION_ID_TYPE     IDENTIFIER; /**< The identifier of the partition. */
   SYSTEM_TIME_TYPE      PERIOD; /**< The period of the partition. */
   /**
    * The duration of the partition in its period.
    */
   SYSTEM_TIME_TYPE      DURATION;
   LOCK_LEVEL_TYPE       LOCK_LEVEL; /**< The preemption lock level. */
   OPERATING_MODE_TYPE   OPERATING_MODE; /**< The operating mode. */
   START_CONDITION_TYPE  START_CONDITION; /**< Why the partition was started. */
   /**
    * The number of processor cores of the partition.
    */
   NUM_CORES_TYPE        NUM_ASSIGNED_CORES;
} PARTITION_STATUS_TYPE;

/** \cond */
extern void GET_PARTITION_STATUS (
      /*out*/ PARTITION_STATUS_TYPE     *PARTITION_STATUS,
      /*out*/ RETURN_CODE_TYPE          *RETURN_CODE );
extern void SET_PARTITION_MODE (
      /*in */ OPERATING_MODE_TYPE       OPERATING_MODE,
      /*out*/ RETURN_CODE_TYPE          *RETURN_CODE );
/** \endcond */
#endif

#endif
