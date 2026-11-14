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

/* @(#)k_cos.c 5.1 93/09/24 */
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
 * `__kernel_cos()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

/*
 * __kernel_cos( x,  y )
 * kernel cos function on [-pi/4, pi/4], pi/4 ~ 0.785398164
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 *
 * Algorithm
 *	1. Since cos(-x) = cos(x), we need only to consider positive x.
 *	2. if x < 2^-27 (hx<0x3e400000 0), return 1 with inexact if x!=0.
 *	3. cos(x) is approximated by a polynomial of degree 14 on
 *	   [0,pi/4]
 *		  	                 4            14
 *	   	cos(x) ~ 1 - x*x/2 + C1*x + ... + C6*x
 *	   where the remez error is
 *
 * 	|              2     4     6     8     10    12     14 |     -58
 * 	|cos(x)-(1-.5*x +C1*x +C2*x +C3*x +C4*x +C5*x  +C6*x  )| <= 2
 * 	|    					               |
 *
 * 	               4     6     8     10    12     14
 *	4. let r = C1*x +C2*x +C3*x +C4*x +C5*x  +C6*x  , then
 *	       cos(x) = 1 - x*x/2 + r
 *	   since cos(x+y) ~ cos(x) - sin(x)*y
 *			  ~ cos(x) - x*y,
 *	   a correction term is necessary in cos(x) and hence
 *		cos(x+y) = 1 - (x*x/2 - (r - x*y))
 *	   For better accuracy when x > 0.3, let qx = |x|/4 with
 *	   the last 32 bits mask off, and if x > 0.78125, let qx = 0.28125.
 *	   Then
 *		cos(x+y) = (1-qx) - ((x*x/2-qx) - (r-x*y)).
 *	   Note that 1-qx and (x*x/2-qx) is EXACT here, and the
 *	   magnitude of the latter is at least a quarter of x*x/2,
 *	   thus, reducing the rounding error in the subtraction.
 */

#include <libm.h>
#include "math_private.h"

static const double
one =  1.00000000000000000000e+00, /* 0x3FF00000, 0x00000000 */
C1  =  4.16666666666666019037e-02, /* 0x3FA55555, 0x5555554C */
C2  = -1.38888888888741095749e-03, /* 0xBF56C16C, 0x16C15177 */
C3  =  2.48015872894767294178e-05, /* 0x3EFA01A0, 0x19CB1590 */
C4  = -2.75573143513906633035e-07, /* 0xBE927E4F, 0x809C52AD */
C5  =  2.08757232129817482790e-09, /* 0x3E21EE9E, 0xBDB4B1C4 */
C6  = -1.13596475577881948265e-11; /* 0xBDA8FAE9, 0xBE8838D4 */

/**
 * @funcname `__kernel_cos`
 *
 * @description
 * The kernel cosine of x + y on [-pi/4, pi/4] (y is the tail of x).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `y` | `double` | [in] The tail of x. |
 *
 * @return The cosine.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `C1` | `const double` | A constant of the algorithm |
 * | `C2` | `const double` | A constant of the algorithm |
 * | `C3` | `const double` | A constant of the algorithm |
 * | `C4` | `const double` | A constant of the algorithm |
 * | `C5` | `const double` | A constant of the algorithm |
 * | `C6` | `const double` | A constant of the algorithm |
 * | `one` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a` | `double` | A temporary of the computation |
 * | `hz` | `double` | A temporary of the computation |
 * | `ix` | `int32_t` | The high word of x |
 * | `qx` | `double` | A temporary of the computation |
 * | `r` | `double` | A temporary of the computation |
 * | `z` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_HIGH_WORD()` with the arguments `ix` and `x`.
 * 2. Keep only the bits `0x7fffffff` of `ix`.
 * 3. If `ix` is less than `0x3e400000`:
 *    1. If `((int)x)` is equal to `0`:
 *       1. Return `one`.
 * 4. Set `z` to `x*x`.
 * 5. Set `r` to `z*(C1+z*(C2+z*(C3+z*(C4+z*(C5+z*C6)))))`.
 * 6. If `ix` is less than `0x3FD33333`:
 *    1. Return `one - (0.5*z - (z*r - x*y))`.
 * 7. Otherwise:
 *    1. If `ix` is greater than `0x3fe90000`:
 *       1. Set `qx` to `0.28125`.
 *    2. Otherwise:
 *       1. Use the macro `INSERT_WORDS()` with the arguments `qx`,
 *          `ix-0x00200000` and `0`.
 *    3. Set `hz` to `0.5*z-qx`.
 *    4. Set `a` to `one-qx`.
 *    5. Return `a - (hz - (z*r-x*y))`.
 *
 * @calls
 * None.
 */
double
__kernel_cos(double x, double y)
{
	double a,hz,z,r,qx;
	int32_t ix;
	GET_HIGH_WORD(ix,x);
	ix &= 0x7fffffff;			/* ix = |x|'s high word*/
	if(ix<0x3e400000) {			/* if x < 2**27 */
	    if(((int)x)==0) return one;		/* generate inexact */
	}
	z  = x*x;
	r  = z*(C1+z*(C2+z*(C3+z*(C4+z*(C5+z*C6)))));
	if(ix < 0x3FD33333) 			/* if |x| < 0.3 */
	    return one - (0.5*z - (z*r - x*y));
	else {
	    if(ix > 0x3fe90000) {		/* x > 0.78125 */
		qx = 0.28125;
	    } else {
	        INSERT_WORDS(qx,ix-0x00200000,0);	/* x/4 */
	    }
	    hz = 0.5*z-qx;
	    a  = one-qx;
	    return a - (hz - (z*r-x*y));
	}
}

#endif
