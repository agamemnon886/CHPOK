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

/* @(#)e_cosh.c 5.1 93/09/24 */
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
 * `__ieee754_cosh()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
/* __ieee754_cosh(x)
 * Method :
 * mathematically cosh(x) if defined to be (exp(x)+exp(-x))/2
 *	1. Replace x by |x| (cosh(x) = cosh(-x)).
 *	2.
 *		                                        [ exp(x) - 1 ]^2
 *	    0        <= x <= ln2/2  :  cosh(x) := 1 + -------------------
 *			       			           2*exp(x)
 *
 *		                                  exp(x) +  1/exp(x)
 *	    ln2/2    <= x <= 22     :  cosh(x) := -------------------
 *			       			          2
 *	    22       <= x <= lnovft :  cosh(x) := exp(x)/2
 *	    lnovft   <= x <= ln2ovft:  cosh(x) := exp(x/2)/2 * exp(x/2)
 *	    ln2ovft  <  x	    :  cosh(x) := huge*huge (overflow)
 *
 * Special cases:
 *	cosh(x) is |x| if x is +INF, -INF, or NaN.
 *	only cosh(0)=1 is exact for finite x.
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH
#include "math_private.h"
#include <libm.h>

static const double one = 1.0, half=0.5, huge = 1.0e300;

/**
 * @funcname `__ieee754_cosh`
 *
 * @description
 * Computes the hyperbolic cosine of x: the IEEE 754 core of cosh().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `half` | `const double` | A constant of the algorithm |
 * | `huge` | `const double` | A constant of the algorithm |
 * | `one` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ix` | `int32_t` | The high word of x |
 * | `lx` | `uint32_t` | The low word of x |
 * | `t` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_HIGH_WORD()` with the arguments `ix` and `x`.
 * 2. Keep only the bits `0x7fffffff` of `ix`.
 * 3. If `ix` is greater than or equal to `0x7ff00000`:
 *    1. Return `x*x`.
 * 4. If `ix` is less than `0x3fd62e43`:
 *    1. Call `expm1()` with the argument `fabs(x)`, and store the result in
 *       `t`.
 *    2. Set `w` to `one+t`.
 *    3. If `ix` is less than `0x3c800000`:
 *       1. Return `w`.
 *    4. Return `one+(t*t)/(w+w)`.
 * 5. If `ix` is less than `0x40360000`:
 *    1. Call `__ieee754_exp()` with the argument `fabs(x)`, and store the
 *       result in `t`.
 *    2. Return `half*t+half/t`.
 * 6. If `ix` is less than `0x40862E42`:
 *    1. Return `half*__ieee754_exp(fabs(x))`.
 * 7. Use the macro `GET_LOW_WORD()` with the arguments `lx` and `x`.
 * 8. If `ix` is less than `0x408633CE` or (`ix` is equal to `0x408633ce` and
 *    `lx` is less than or equal to `(uint32_t)0x8fb9f87d`):
 *    1. Call `__ieee754_exp()` with the argument `half*fabs(x)`, and store the
 *       result in `w`.
 *    2. Set `t` to `half*w`.
 *    3. Return `t*w`.
 * 9. Return `huge*huge`.
 *
 * @calls
 * - `expm1()`
 * - `fabs()`
 * - `__ieee754_exp()`
 */
double
__ieee754_cosh(double x)
{
	double t,w;
	int32_t ix;
	uint32_t lx;

    /* High word of |x|. */
	GET_HIGH_WORD(ix,x);
	ix &= 0x7fffffff;

    /* x is INF or NaN */
	if(ix>=0x7ff00000) return x*x;

    /* |x| in [0,0.5*ln2], return 1+expm1(|x|)^2/(2*exp(|x|)) */
	if(ix<0x3fd62e43) {
	    t = expm1(fabs(x));
	    w = one+t;
	    if (ix<0x3c800000) return w;	/* cosh(tiny) = 1 */
	    return one+(t*t)/(w+w);
	}

    /* |x| in [0.5*ln2,22], return (exp(|x|)+1/exp(|x|)/2; */
	if (ix < 0x40360000) {
		t = __ieee754_exp(fabs(x));
		return half*t+half/t;
	}

    /* |x| in [22, log(maxdouble)] return half*exp(|x|) */
	if (ix < 0x40862E42)  return half*__ieee754_exp(fabs(x));

    /* |x| in [log(maxdouble), overflowthresold] */
	GET_LOW_WORD(lx,x);
	if (ix<0x408633CE ||
	      ((ix==0x408633ce)&&(lx<=(uint32_t)0x8fb9f87d))) {
	    w = __ieee754_exp(half*fabs(x));
	    t = half*w;
	    return t*w;
	}

    /* |x| > overflowthresold, cosh(x) overflow */
	return huge*huge;
}
#endif
