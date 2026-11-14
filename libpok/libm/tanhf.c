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

/* s_tanhf.c -- float version of s_tanh.c.
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
 * tanhf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float one=1.0, two=2.0, tiny = 1.0e-30;

/**
 * @funcname `tanhf`
 *
 * @description
 * Computes the hyperbolic tangent of x, in float (C standard).
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
 * | `tiny` | `const float` | A constant of the algorithm |
 * | `two` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `jx` | `int32_t` | The bits of x |
 * | `t` | `float` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `jx` and `x`.
 * 2. Set `ix` to `jx&0x7fffffff`.
 * 3. If `ix` is greater than or equal to `0x7f800000`:
 *    1. If `jx` is greater than or equal to `0`:
 *       1. Return `one/x+one`.
 *    2. Otherwise:
 *       1. Return `one/x-one`.
 * 4. If `ix` is less than `0x41b00000`:
 *    1. If `ix` is less than `0x24000000`:
 *       1. Return `x*(one+x)`.
 *    2. If `ix` is greater than or equal to `0x3f800000`:
 *       1. Call `expm1f()` with the argument `two*fabsf(x)`, and store the
 *          result in `t`.
 *       2. Set `z` to `one - two/(t+two)`.
 *    3. Otherwise:
 *       1. Call `expm1f()` with the argument `-two*fabsf(x)`, and store the
 *          result in `t`.
 *       2. Set `z` to `-t/(t+two)`.
 * 5. Otherwise:
 *    1. Set `z` to `one - tiny`.
 * 6. Return `z` if `jx` is greater than or equal to `0`, otherwise `-z`.
 *
 * @calls
 * - `expm1f()`
 * - `fabsf()`
 */
float
tanhf(float x)
{
	float t,z;
	int32_t jx,ix;

	GET_FLOAT_WORD(jx,x);
	ix = jx&0x7fffffff;

    /* x is INF or NaN */
	if(ix>=0x7f800000) {
	    if (jx>=0) return one/x+one;    /* tanh(+-inf)=+-1 */
	    else       return one/x-one;    /* tanh(NaN) = NaN */
	}

    /* |x| < 22 */
	if (ix < 0x41b00000) {		/* |x|<22 */
	    if (ix<0x24000000) 		/* |x|<2**-55 */
		return x*(one+x);    	/* tanh(small) = small */
	    if (ix>=0x3f800000) {	/* |x|>=1  */
		t = expm1f(two*fabsf(x));
		z = one - two/(t+two);
	    } else {
	        t = expm1f(-two*fabsf(x));
	        z= -t/(t+two);
	    }
    /* |x| > 22, return +-1 */
	} else {
	    z = one - tiny;		/* raised inexact flag */
	}
	return (jx>=0)? z: -z;
}

#endif
