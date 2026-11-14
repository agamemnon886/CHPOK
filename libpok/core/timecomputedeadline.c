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
 * pok_time_compute_deadline() of the old POK interface (not built: the
 * generated code configuration always has
 * POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE).
 */

#include <config.h>

#ifndef POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE

#ifdef POK_NEEDS_TIME
#include <errno.h>
#include <types.h>
#include <core/time.h>

/**
 * @funcname `pok_time_compute_deadline`
 *
 * @description
 * Computes a deadline from now: the current time plus a relative time.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `relative` | `const pok_time_t` | [in] The relative time. |
 * | `absolute` | `pok_time_t *` | [out] Where the deadline is stored. |
 *
 * @return POK_ERRNO_OK, or POK_ERRNO_TIME if the time can't be read.
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
 * 1. If `pok_time_gettick(absolute)` is not equal to `POK_ERRNO_OK`:
 *    1. Return `POK_ERRNO_TIME`.
 * 2. Set `*absolute` to `*absolute + relative`.
 * 3. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `pok_time_gettick()`
 */
pok_ret_e pok_time_compute_deadline (const pok_time_t relative,
    pok_time_t* absolute)
{
   if (pok_time_gettick (absolute) != POK_ERRNO_OK)
   {
      return (POK_ERRNO_TIME);
   }
   *absolute = *absolute + relative;
   return POK_ERRNO_OK;
}

#endif

#endif /* POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE */

