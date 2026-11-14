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

/* e_atanhf.c -- float version of e_atanh.c.
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
 * `__ieee754_atanhf()` of libpok's math library (fdlibm, from Sun
 * Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH
#include <libm.h>
#include "math_private.h"

static const float one = 1.0, huge = 1e30;

static const float zero = 0.0;

/**
 * @funcname `__ieee754_atanhf`
 *
 * @description
 * Computes the inverse hyperbolic tangent of x, in float: the IEEE 754 core of
 * atanhf().
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
 * | `huge` | `const float` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The bits of x |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `t` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `hx&0x7fffffff`.
 * 3. If `ix` is greater than `0x3f800000`:
 *    1. Return `(x-x)/(x-x)`.
 * 4. If `ix` is equal to `0x3f800000`:
 *    1. Return `x/zero`.
 * 5. If `ix` is less than `0x31800000` and `(huge+x)` is greater than `zero`:
 *    1. Return `x`.
 * 6. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `ix`.
 * 7. If `ix` is less than `0x3f000000`:
 *    1. Set `t` to `x+x`.
 *    2. Set `t` to `(float)0.5*log1pf(t+t*x/(one-x))`.
 * 8. Otherwise:
 *    1. Set `t` to `(float)0.5*log1pf((x+x)/(one-x))`.
 * 9. If `hx` is greater than or equal to `0`:
 *    1. Return `t`.
 * 10. Otherwise:
 *     1. Return `-t`.
 *
 * @calls
 * - `log1pf()`
 */
float
__ieee754_atanhf(float x)
{
	float t;
	int32_t hx,ix;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if (ix>0x3f800000) 		/* |x|>1 */
	    return (x-x)/(x-x);
	if(ix==0x3f800000)
	    return x/zero;
	if(ix<0x31800000&&(huge+x)>zero) return x;	/* x<2**-28 */
	SET_FLOAT_WORD(x,ix);
	if(ix<0x3f000000) {		/* x < 0.5 */
	    t = x+x;
	    t = (float)0.5*log1pf(t+t*x/(one-x));
	} else
	    t = (float)0.5*log1pf((x+x)/(one-x));
	if(hx>=0) return t; else return -t;
}
#endif
