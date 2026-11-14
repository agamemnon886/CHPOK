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
 * Created by julien on Fri Jan 30 14:41:34 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-09: Doxygen headers for the file
 * and its functions.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/* @(#)s_ldexp0.c 5.1 93/09/24 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/**
 * @file
 * ldexp() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"
#include <errno.h>

/**
 * @funcname `ldexp`
 *
 * @description
 * Returns value times 2 raised to the power exp0.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `double` | [in] The value. |
 * | `exp0` | `int` | [in] The power of 2. |
 *
 * @return The result.
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
 * 1. If `finite(value)` is false or `value` is equal to `0.0`:
 *    1. Return `value`.
 * 2. Call `scalbn()` with the arguments `value` and `exp0`, and store the
 *    result in `value`.
 * 3. Return `value`.
 *
 * @calls
 * - `finite()`
 * - `scalbn()`
 */
double
ldexp(double value, int exp0)
{
	if(!finite(value)||value==0.0) return value;
	value = scalbn(value,exp0);
	/*if(!finite(value)||value==0.0) errno = POK_ERRNO_ERANGE;*/
	return value;
}

#endif
