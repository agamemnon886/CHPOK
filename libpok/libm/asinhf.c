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

/* s_asinhf.c -- float version of s_asinh.c.
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
 * asinhf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <types.h>
#include <libm.h>
#include "math_private.h"

static const float
one =  1.0000000000e+00, /* 0x3F800000 */
ln2 =  6.9314718246e-01, /* 0x3f317218 */
huge=  1.0000000000e+30;

/**
 * @funcname `asinhf`
 *
 * @description
 * Computes the inverse hyperbolic sine of x, in float (C standard).
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
 * | `ln2` | `const float` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The bits of x |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `t` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `hx&0x7fffffff`.
 * 3. If `ix` is greater than or equal to `0x7f800000`:
 *    1. Return `x+x`.
 * 4. If `ix` is less than `0x31800000`:
 *    1. If `huge+x` is greater than `one`:
 *       1. Return `x`.
 * 5. If `ix` is greater than `0x4d800000`:
 *    1. Set `w` to `__ieee754_logf(fabsf(x))+ln2`.
 * 6. Otherwise, if `ix` is greater than `0x40000000`:
 *    1. Call `fabsf()` with the argument `x`, and store the result in `t`.
 *    2. Call `__ieee754_logf()` with the argument
 *       `(float)2.0*t+one/(__ieee754_sqrtf(x*x+one)+t)`, and store the result
 *       in `w`.
 * 7. Otherwise:
 *    1. Set `t` to `x*x`.
 *    2. Call `log1pf()` with the argument
 *       `fabsf(x)+t/(one+__ieee754_sqrtf(one+t))`, and store the result in
 *       `w`.
 * 8. If `hx` is greater than `0`:
 *    1. Return `w`.
 * 9. Otherwise:
 *    1. Return `-w`.
 *
 * @calls
 * - `__ieee754_logf()`
 * - `fabsf()`
 * - `__ieee754_sqrtf()`
 * - `log1pf()`
 */
float
asinhf(float x)
{
	float t,w;
	int32_t hx,ix;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix>=0x7f800000) return x+x;	/* x is inf or NaN */
	if(ix< 0x31800000) {	/* |x|<2**-28 */
	    if(huge+x>one) return x;	/* return x inexact except 0 */
	}
	if(ix>0x4d800000) {	/* |x| > 2**28 */
	    w = __ieee754_logf(fabsf(x))+ln2;
	} else if (ix>0x40000000) {	/* 2**28 > |x| > 2.0 */
	    t = fabsf(x);
	    w = __ieee754_logf((float)2.0*t+one/(__ieee754_sqrtf(x*x+one)+t));
	} else {		/* 2.0 > |x| > 2**-28 */
	    t = x*x;
	    w =log1pf(fabsf(x)+t/(one+__ieee754_sqrtf(one+t)));
	}
	if(hx>0) return w; else return -w;
}
#endif

