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

/* e_sinhf.c -- float version of e_sinh.c.
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
 * `__ieee754_sinhf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float one = 1.0, shuge = 1.0e37;

/**
 * @funcname `__ieee754_sinhf`
 *
 * @description
 * Computes the hyperbolic sine of x, in float: the IEEE 754 core of sinhf().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `one` | `const float` | A constant of the algorithm |
 * | `shuge` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `h` | `float` | A temporary of the computation |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `jx` | `int32_t` | The bits of x |
 * | `t` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `jx` and `x`.
 * 2. Set `ix` to `jx&0x7fffffff`.
 * 3. If `ix` is greater than or equal to `0x7f800000`:
 *    1. Return `x+x`.
 * 4. Set `h` to `0.5`.
 * 5. If `jx` is less than `0`:
 *    1. Set `h` to `-h`.
 * 6. If `ix` is less than `0x41b00000`:
 *    1. If `ix` is less than `0x31800000`:
 *       1. If `shuge+x` is greater than `one`:
 *          1. Return `x`.
 *    2. Call `expm1f()` with the argument `fabsf(x)`, and store the result in
 *       `t`.
 *    3. If `ix` is less than `0x3f800000`:
 *       1. Return `h*((float)2.0*t-t*t/(t+one))`.
 *    4. Return `h*(t+t/(t+one))`.
 * 7. If `ix` is less than `0x42b17180`:
 *    1. Return `h*__ieee754_expf(fabsf(x))`.
 * 8. If `ix` is less than or equal to `0x42b2d4fc`:
 *    1. Call `__ieee754_expf()` with the argument `(float)0.5*fabsf(x)`, and
 *       store the result in `w`.
 *    2. Set `t` to `h*w`.
 *    3. Return `t*w`.
 * 9. Return `x*shuge`.
 *
 * @calls
 * - `expm1f()`
 * - `fabsf()`
 * - `__ieee754_expf()`
 */
float
__ieee754_sinhf(float x)
{
	float t,w,h;
	int32_t ix,jx;

	GET_FLOAT_WORD(jx,x);
	ix = jx&0x7fffffff;

    /* x is INF or NaN */
	if(ix>=0x7f800000) return x+x;

	h = 0.5;
	if (jx<0) h = -h;
    /* |x| in [0,22], return sign(x)*0.5*(E+E/(E+1))) */
	if (ix < 0x41b00000) {		/* |x|<22 */
	    if (ix<0x31800000) 		/* |x|<2**-28 */
		if(shuge+x>one) return x;/* sinh(tiny) = tiny with inexact */
	    t = expm1f(fabsf(x));
	    if(ix<0x3f800000) return h*((float)2.0*t-t*t/(t+one));
	    return h*(t+t/(t+one));
	}

    /* |x| in [22, log(maxdouble)] return 0.5*exp(|x|) */
	if (ix < 0x42b17180)  return h*__ieee754_expf(fabsf(x));

    /* |x| in [log(maxdouble), overflowthresold] */
	if (ix<=0x42b2d4fc) {
	    w = __ieee754_expf((float)0.5*fabsf(x));
	    t = h*w;
	    return t*w;
	}

    /* |x| > overflowthresold, sinh(x) overflow */
	return x*shuge;
}
#endif

