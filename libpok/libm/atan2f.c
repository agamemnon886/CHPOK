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

/* w_atan2f.c -- float version of w_atan2.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 */

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
 * atan2f() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

/*
 * wrapper atan2f(y,x)
 */

#include <libm.h>
#include "namespace.h"
#include "math_private.h"

#ifdef __weak_alias
__weak_alias(atan2f, _atan2f)
#endif

/**
 * @funcname `atan2f`
 *
 * @description
 * Computes the arc tangent of y/x, in the quadrant of (x, y), in float (C
 * standard): the wrapper of `__ieee754_atan2f()`.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `y` | `float` | [in] The second argument. |
 * | `x` | `float` | [in] The argument. |
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
 * 1. Call `__ieee754_atan2f()` with the arguments `y` and `x`, and return its
 *    result.
 *
 * @calls
 * - `__ieee754_atan2f()`
 */
float
atan2f(float y, float x)		/* wrapper atan2f */
{
#ifdef _IEEE_LIBM
	return __ieee754_atan2f(y,x);
#else
	float z;
	z = __ieee754_atan2f(y,x);
	if(_LIB_VERSION == _IEEE_||isnanf(x)||isnanf(y)) return z;
	if(x==(float)0.0&&y==(float)0.0) {
		/* atan2f(+-0,+-0) */
	        return (float)__kernel_standard((double)y,(double)x,103);
	} else
	    return z;
#endif
}

#endif
