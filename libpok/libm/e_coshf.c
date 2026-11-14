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

/* e_coshf.c -- float version of e_cosh.c.
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
 * `__ieee754_coshf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH
#include <libm.h>
#include "math_private.h"

static const float huge = 1.0e30;
static const float one = 1.0, half=0.5;

/**
 * @funcname `__ieee754_coshf`
 *
 * @description
 * Computes the hyperbolic cosine of x, in float: the IEEE 754 core of coshf().
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
 * | `half` | `const float` | A constant of the algorithm |
 * | `huge` | `const float` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ix` | `int32_t` | The bits of x |
 * | `t` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 * 2. Keep only the bits `0x7fffffff` of `ix`.
 * 3. If `ix` is greater than or equal to `0x7f800000`:
 *    1. Return `x*x`.
 * 4. If `ix` is less than `0x3eb17218`:
 *    1. Call `expm1f()` with the argument `fabsf(x)`, and store the result in
 *       `t`.
 *    2. Set `w` to `one+t`.
 *    3. If `ix` is less than `0x24000000`:
 *       1. Return `w`.
 *    4. Return `one+(t*t)/(w+w)`.
 * 5. If `ix` is less than `0x41b00000`:
 *    1. Call `__ieee754_expf()` with the argument `fabsf(x)`, and store the
 *       result in `t`.
 *    2. Return `half*t+half/t`.
 * 6. If `ix` is less than `0x42b17180`:
 *    1. Return `half*__ieee754_expf(fabsf(x))`.
 * 7. If `ix` is less than or equal to `0x42b2d4fc`:
 *    1. Call `__ieee754_expf()` with the argument `half*fabsf(x)`, and store
 *       the result in `w`.
 *    2. Set `t` to `half*w`.
 *    3. Return `t*w`.
 * 8. Return `huge*huge`.
 *
 * @calls
 * - `expm1f()`
 * - `fabsf()`
 * - `__ieee754_expf()`
 */
float
__ieee754_coshf(float x)
{
	float t,w;
	int32_t ix;

	GET_FLOAT_WORD(ix,x);
	ix &= 0x7fffffff;

    /* x is INF or NaN */
	if(ix>=0x7f800000) return x*x;

    /* |x| in [0,0.5*ln2], return 1+expm1(|x|)^2/(2*exp(|x|)) */
	if(ix<0x3eb17218) {
	    t = expm1f(fabsf(x));
	    w = one+t;
	    if (ix<0x24000000) return w;	/* cosh(tiny) = 1 */
	    return one+(t*t)/(w+w);
	}

    /* |x| in [0.5*ln2,22], return (exp(|x|)+1/exp(|x|)/2; */
	if (ix < 0x41b00000) {
		t = __ieee754_expf(fabsf(x));
		return half*t+half/t;
	}

    /* |x| in [22, log(maxdouble)] return half*exp(|x|) */
	if (ix < 0x42b17180)  return half*__ieee754_expf(fabsf(x));

    /* |x| in [log(maxdouble), overflowthresold] */
	if (ix<=0x42b2d4fc) {
	    w = __ieee754_expf(half*fabsf(x));
	    t = half*w;
	    return t*w;
	}

    /* |x| > overflowthresold, cosh(x) overflow */
	return huge*huge;
}
#endif

