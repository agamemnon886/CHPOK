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

/* @(#)e_atanh.c 5.1 93/09/24 */
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
 * `__ieee754_atanh()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
/* __ieee754_atanh(x)
 * Method :
 *    1.Reduced x to positive by atanh(-x) = -atanh(x)
 *    2.For x>=0.5
 *                  1              2x                          x
 *	atanh(x) = --- * log(1 + -------) = 0.5 * log1p(2 * --------)
 *                  2             1 - x                      1 - x
 *
 * 	For x<0.5
 *	atanh(x) = 0.5*log1p(2x+2x*x/(1-x))
 *
 * Special cases:
 *	atanh(x) is NaN if |x| > 1 with signal;
 *	atanh(NaN) is that NaN with no signal;
 *	atanh(+-1) is +-INF with signal.
 *
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const double one = 1.0, huge = 1e300;

static const double zero = 0.0;

/**
 * @funcname `__ieee754_atanh`
 *
 * @description
 * Computes the inverse hyperbolic tangent of x: the IEEE 754 core of atanh().
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
 * | `huge` | `const double` | A constant of the algorithm |
 * | `one` | `const double` | A constant of the algorithm |
 * | `zero` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The high word of x |
 * | `ix` | `int32_t` | The high word of x without the sign bit |
 * | `lx` | `uint32_t` | The low word of x |
 * | `t` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 2. Set `ix` to `hx&0x7fffffff`.
 * 3. If `(ix|((lx|(-lx))>>31))` is greater than `0x3ff00000`:
 *    1. Return `(x-x)/(x-x)`.
 * 4. If `ix` is equal to `0x3ff00000`:
 *    1. Return `x/zero`.
 * 5. If `ix` is less than `0x3e300000` and `(huge+x)` is greater than `zero`:
 *    1. Return `x`.
 * 6. Use the macro `SET_HIGH_WORD()` with the arguments `x` and `ix`.
 * 7. If `ix` is less than `0x3fe00000`:
 *    1. Set `t` to `x+x`.
 *    2. Set `t` to `0.5*log1p(t+t*x/(one-x))`.
 * 8. Otherwise:
 *    1. Set `t` to `0.5*log1p((x+x)/(one-x))`.
 * 9. If `hx` is greater than or equal to `0`:
 *    1. Return `t`.
 * 10. Otherwise:
 *     1. Return `-t`.
 *
 * @calls
 * - `log1p()`
 */
double
__ieee754_atanh(double x)
{
	double t;
	int32_t hx,ix;
	uint32_t lx;
	EXTRACT_WORDS(hx,lx,x);
	ix = hx&0x7fffffff;
	if ((ix|((lx|(-lx))>>31))>0x3ff00000) /* |x|>1 */
	    return (x-x)/(x-x);
	if(ix==0x3ff00000)
	    return x/zero;
	if(ix<0x3e300000&&(huge+x)>zero) return x;	/* x<2**-28 */
	SET_HIGH_WORD(x,ix);
	if(ix<0x3fe00000) {		/* x < 0.5 */
	    t = x+x;
	    t = 0.5*log1p(t+t*x/(one-x));
	} else
	    t = 0.5*log1p((x+x)/(one-x));
	if(hx>=0) return t; else return -t;
}
#endif
