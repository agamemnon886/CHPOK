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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * pok_time_get() of the old POK interface.
 */

#include <core/dependencies.h>

#if ( (! defined (POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE )) || ( defined (POK_NEEDS_FUNCTION_TIME_GET)))

#include <errno.h>
#include <types.h>
#include <core/time.h>

/**
 * @funcname `pok_time_get`
 *
 * @description
 * Stores the current time (old POK interface).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ms` | `pok_time_t *` | [out] Where the time is stored. |
 *
 * @return The result of pok_time_gettick().
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `(pok_time_gettick (ms))`.
 *
 * @calls
 * - `pok_time_gettick()`
 */
pok_ret_e pok_time_get (pok_time_t* ms)
{
   return (pok_time_gettick (ms));
}

#endif /* POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE */

