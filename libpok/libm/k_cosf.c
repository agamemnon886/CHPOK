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
 * Created by julien on Sat Jan 31 20:12:07 2009 
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

/* k_cosf.c -- float version of k_cos.c
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
 * `__kernel_cosf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float
one =  1.0000000000e+00, /* 0x3f800000 */
C1  =  4.1666667908e-02, /* 0x3d2aaaab */
C2  = -1.3888889225e-03, /* 0xbab60b61 */
C3  =  2.4801587642e-05, /* 0x37d00d01 */
C4  = -2.7557314297e-07, /* 0xb493f27c */
C5  =  2.0875723372e-09, /* 0x310f74f6 */
C6  = -1.1359647598e-11; /* 0xad47d74e */

/**
 * @funcname `__kernel_cosf`
 *
 * @description
 * The kernel cosine of x + y on [-pi/4, pi/4] (y is the tail of x).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `y` | `float` | [in] The tail of x. |
 *
 * @return The cosine.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `C1` | `const float` | A constant of the algorithm |
 * | `C2` | `const float` | A constant of the algorithm |
 * | `C3` | `const float` | A constant of the algorithm |
 * | `C4` | `const float` | A constant of the algorithm |
 * | `C5` | `const float` | A constant of the algorithm |
 * | `C6` | `const float` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a` | `float` | A temporary of the computation |
 * | `hz` | `float` | A temporary of the computation |
 * | `ix` | `int32_t` | The bits of x |
 * | `qx` | `float` | A temporary of the computation |
 * | `r` | `float` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 * 2. Keep only the bits `0x7fffffff` of `ix`.
 * 3. If `ix` is less than `0x32000000`:
 *    1. If `((int)x)` is equal to `0`:
 *       1. Return `one`.
 * 4. Set `z` to `x*x`.
 * 5. Set `r` to `z*(C1+z*(C2+z*(C3+z*(C4+z*(C5+z*C6)))))`.
 * 6. If `ix` is less than `0x3e99999a`:
 *    1. Return `one - ((float)0.5*z - (z*r - x*y))`.
 * 7. Otherwise:
 *    1. If `ix` is greater than `0x3f480000`:
 *       1. Set `qx` to `(float)0.28125`.
 *    2. Otherwise:
 *       1. Use the macro `SET_FLOAT_WORD()` with the arguments `qx` and
 *          `ix-0x01000000`.
 *    3. Set `hz` to `(float)0.5*z-qx`.
 *    4. Set `a` to `one-qx`.
 *    5. Return `a - (hz - (z*r-x*y))`.
 *
 * @calls
 * None.
 */
float
__kernel_cosf(float x, float y)
{
	float a,hz,z,r,qx;
	int32_t ix;
	GET_FLOAT_WORD(ix,x);
	ix &= 0x7fffffff;			/* ix = |x|'s high word*/
	if(ix<0x32000000) {			/* if x < 2**27 */
	    if(((int)x)==0) return one;		/* generate inexact */
	}
	z  = x*x;
	r  = z*(C1+z*(C2+z*(C3+z*(C4+z*(C5+z*C6)))));
	if(ix < 0x3e99999a) 			/* if |x| < 0.3 */
	    return one - ((float)0.5*z - (z*r - x*y));
	else {
	    if(ix > 0x3f480000) {		/* x > 0.78125 */
		qx = (float)0.28125;
	    } else {
	        SET_FLOAT_WORD(qx,ix-0x01000000);	/* x/4 */
	    }
	    hz = (float)0.5*z-qx;
	    a  = one-qx;
	    return a - (hz - (z*r-x*y));
	}
}

#endif

