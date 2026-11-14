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
 * Created by julien on Fri Jan 30 13:44:27 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-09: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/* @(#)s_isnan.c 5.1 93/09/24 */
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
 * isnan() of libpok's math library (fdlibm, from Sun Microsystems).
 */
/*
 * isnan(x) returns 1 is x is nan, else 0;
 * no branching!
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH
#include <types.h>
#include "math_private.h"

/**
 * @funcname `isnan`
 *
 * @description
 * Tells whether x is a NaN.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 *
 * @return Non-zero if it is.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The high word of x |
 * | `lx` | `int32_t` | The low word of x |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 2. Keep only the bits `0x7fffffff` of `hx`.
 * 3. Set the bits `(uint32_t)(lx|(-lx))>>31` in `hx`.
 * 4. Set `hx` to `0x7ff00000 - hx`.
 * 5. Return `(int)((uint32_t)(hx))>>31`.
 *
 * @calls
 * None.
 */
int
isnan(double x)
{
	int32_t hx,lx;
	EXTRACT_WORDS(hx,lx,x);
	hx &= 0x7fffffff;
	hx |= (uint32_t)(lx|(-lx))>>31;
	hx = 0x7ff00000 - hx;
	return (int)((uint32_t)(hx))>>31;
}
#endif

