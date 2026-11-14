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
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/* s_sinf.c -- float version of s_sin.c.
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
 * sinf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "namespace.h"
#include "math_private.h"

#if 0 /* notyet */
#ifdef __weak_alias
__weak_alias(sinf, _sinf)
#endif
#endif

/**
 * @funcname `sinf`
 *
 * @description
 * Computes the sine of x, in float (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ix` | `int32_t` | The bits of x |
 * | `n` | `int32_t` | A temporary of the computation |
 * | `y` | `float[2]` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `z` to `0.0`.
 * 2. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 * 3. Keep only the bits `0x7fffffff` of `ix`.
 * 4. If `ix` is less than or equal to `0x3f490fd8`:
 *    1. Call `__kernel_sinf()` with the arguments `x`, `z` and `0`, and return
 *       its result.
 * 5. Otherwise, if `ix` is greater than or equal to `0x7f800000`:
 *    1. Return `x-x`.
 * 6. Otherwise:
 *    1. Call `__ieee754_rem_pio2f()` with the arguments `x` and `y`, and store
 *       the result in `n`.
 *    2. Depending on `n&3`:
 *       1. Case `0`:
 *          1. Call `__kernel_sinf()` with the arguments `y[0]`, `y[1]` and `1`,
 *             and return its result.
 *       2. Case `1`:
 *          1. Call `__kernel_cosf()` with the arguments `y[0]` and `y[1]`, and
 *             return its result.
 *       3. Case `2`:
 *          1. Return `-__kernel_sinf(y[0],y[1],1)`.
 *       4. Otherwise (default):
 *          1. Return `-__kernel_cosf(y[0],y[1])`.
 *
 * @calls
 * - `__kernel_sinf()`
 * - `__ieee754_rem_pio2f()`
 * - `__kernel_cosf()`
 */
float
sinf(float x)
{
	float y[2],z=0.0;
	int32_t n, ix;

	GET_FLOAT_WORD(ix,x);

    /* |x| ~< pi/4 */
	ix &= 0x7fffffff;
	if(ix <= 0x3f490fd8) return __kernel_sinf(x,z,0);

    /* sin(Inf or NaN) is NaN */
	else if (ix>=0x7f800000) return x-x;

    /* argument reduction needed */
	else {
	    n = __ieee754_rem_pio2f(x,y);
	    switch(n&3) {
		case 0: return  __kernel_sinf(y[0],y[1],1);
		case 1: return  __kernel_cosf(y[0],y[1]);
		case 2: return -__kernel_sinf(y[0],y[1],1);
		default:
			return -__kernel_cosf(y[0],y[1]);
	    }
	}
}

#endif
