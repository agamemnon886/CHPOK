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

/* @(#)e_sinh.c 5.1 93/09/24 */
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
 * `__ieee754_sinh()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
/* __ieee754_sinh(x)
 * Method :
 * mathematically sinh(x) if defined to be (exp(x)-exp(-x))/2
 *	1. Replace x by |x| (sinh(-x) = -sinh(x)).
 *	2.
 *		                                    E + E/(E+1)
 *	    0        <= x <= 22     :  sinh(x) := --------------, E=expm1(x)
 *			       			        2
 *
 *	    22       <= x <= lnovft :  sinh(x) := exp(x)/2
 *	    lnovft   <= x <= ln2ovft:  sinh(x) := exp(x/2)/2 * exp(x/2)
 *	    ln2ovft  <  x	    :  sinh(x) := x*shuge (overflow)
 *
 * Special cases:
 *	sinh(x) is |x| if x is +INF, -INF, or NaN.
 *	only sinh(0)=0 is exact for finite x.
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const double one = 1.0, shuge = 1.0e307;

/**
 * @funcname `__ieee754_sinh`
 *
 * @description
 * Computes the hyperbolic sine of x: the IEEE 754 core of sinh().
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
 * | `one` | `const double` | A constant of the algorithm |
 * | `shuge` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `h` | `double` | A temporary of the computation |
 * | `ix` | `int32_t` | The high word of x without the sign bit |
 * | `jx` | `int32_t` | The high word of x |
 * | `lx` | `uint32_t` | The low word of x |
 * | `t` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_HIGH_WORD()` with the arguments `jx` and `x`.
 * 2. Set `ix` to `jx&0x7fffffff`.
 * 3. If `ix` is greater than or equal to `0x7ff00000`:
 *    1. Return `x+x`.
 * 4. Set `h` to `0.5`.
 * 5. If `jx` is less than `0`:
 *    1. Set `h` to `-h`.
 * 6. If `ix` is less than `0x40360000`:
 *    1. If `ix` is less than `0x3e300000`:
 *       1. If `shuge+x` is greater than `one`:
 *          1. Return `x`.
 *    2. Call `expm1()` with the argument `fabs(x)`, and store the result in
 *       `t`.
 *    3. If `ix` is less than `0x3ff00000`:
 *       1. Return `h*(2.0*t-t*t/(t+one))`.
 *    4. Return `h*(t+t/(t+one))`.
 * 7. If `ix` is less than `0x40862E42`:
 *    1. Return `h*__ieee754_exp(fabs(x))`.
 * 8. Use the macro `GET_LOW_WORD()` with the arguments `lx` and `x`.
 * 9. If `ix` is less than `0x408633CE` or (`ix` is equal to `0x408633ce` and
 *    `lx` is less than or equal to `(uint32_t)0x8fb9f87d`):
 *    1. Call `__ieee754_exp()` with the argument `0.5*fabs(x)`, and store the
 *       result in `w`.
 *    2. Set `t` to `h*w`.
 *    3. Return `t*w`.
 * 10. Return `x*shuge`.
 *
 * @calls
 * - `expm1()`
 * - `fabs()`
 * - `__ieee754_exp()`
 */
double
__ieee754_sinh(double x)
{
	double t,w,h;
	int32_t ix,jx;
	uint32_t lx;

    /* High word of |x|. */
	GET_HIGH_WORD(jx,x);
	ix = jx&0x7fffffff;

    /* x is INF or NaN */
	if(ix>=0x7ff00000) return x+x;

	h = 0.5;
	if (jx<0) h = -h;
    /* |x| in [0,22], return sign(x)*0.5*(E+E/(E+1))) */
	if (ix < 0x40360000) {		/* |x|<22 */
	    if (ix<0x3e300000) 		/* |x|<2**-28 */
		if(shuge+x>one) return x;/* sinh(tiny) = tiny with inexact */
	    t = expm1(fabs(x));
	    if(ix<0x3ff00000) return h*(2.0*t-t*t/(t+one));
	    return h*(t+t/(t+one));
	}

    /* |x| in [22, log(maxdouble)] return 0.5*exp(|x|) */
	if (ix < 0x40862E42)  return h*__ieee754_exp(fabs(x));

    /* |x| in [log(maxdouble), overflowthresold] */
	GET_LOW_WORD(lx,x);
	if (ix<0x408633CE || ((ix==0x408633ce)&&(lx<=(uint32_t)0x8fb9f87d))) {
	    w = __ieee754_exp(0.5*fabs(x));
	    t = h*w;
	    return t*w;
	}

    /* |x| > overflowthresold, sinh(x) overflow */
	return x*shuge;
}
#endif

