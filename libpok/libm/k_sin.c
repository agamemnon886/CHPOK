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

/* @(#)k_sin.c 5.1 93/09/24 */
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
 * `__kernel_sin()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

/* __kernel_sin( x, y, iy)
 * kernel sin function on [-pi/4, pi/4], pi/4 ~ 0.7854
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * Input iy indicates whether y is 0. (if iy=0, y assume to be 0).
 *
 * Algorithm
 *	1. Since sin(-x) = -sin(x), we need only to consider positive x.
 *	2. if x < 2^-27 (hx<0x3e400000 0), return x with inexact if x!=0.
 *	3. sin(x) is approximated by a polynomial of degree 13 on
 *	   [0,pi/4]
 *		  	         3            13
 *	   	sin(x) ~ x + S1*x + ... + S6*x
 *	   where
 *
 * 	|sin(x)         2     4     6     8     10     12  |     -58
 * 	|----- - (1+S1*x +S2*x +S3*x +S4*x +S5*x  +S6*x   )| <= 2
 * 	|  x 					           |
 *
 *	4. sin(x+y) = sin(x) + sin'(x')*y
 *		    ~ sin(x) + (1-x*x/2)*y
 *	   For better accuracy, let
 *		     3      2      2      2      2
 *		r = x *(S2+x *(S3+x *(S4+x *(S5+x *S6))))
 *	   then                   3    2
 *		sin(x) = x + (S1*x + (x *(r-y/2)+y))
 */

#include <libm.h> 
#include "math_private.h"

static const double
half =  5.00000000000000000000e-01, /* 0x3FE00000, 0x00000000 */
S1  = -1.66666666666666324348e-01, /* 0xBFC55555, 0x55555549 */
S2  =  8.33333333332248946124e-03, /* 0x3F811111, 0x1110F8A6 */
S3  = -1.98412698298579493134e-04, /* 0xBF2A01A0, 0x19C161D5 */
S4  =  2.75573137070700676789e-06, /* 0x3EC71DE3, 0x57B1FE7D */
S5  = -2.50507602534068634195e-08, /* 0xBE5AE5E6, 0x8A2B9CEB */
S6  =  1.58969099521155010221e-10; /* 0x3DE5D93A, 0x5ACFD57C */

/**
 * @funcname `__kernel_sin`
 *
 * @description
 * The kernel sine of x + y on [-pi/4, pi/4] (y is the tail of x).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `y` | `double` | [in] The tail of x. |
 * | `iy` | `int` | [in] 0 if y is 0. |
 *
 * @return The sine.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `S1` | `const double` | A constant of the algorithm |
 * | `S2` | `const double` | A constant of the algorithm |
 * | `S3` | `const double` | A constant of the algorithm |
 * | `S4` | `const double` | A constant of the algorithm |
 * | `S5` | `const double` | A constant of the algorithm |
 * | `S6` | `const double` | A constant of the algorithm |
 * | `half` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ix` | `int32_t` | The high word of x |
 * | `r` | `double` | A temporary of the computation |
 * | `v` | `double` | A temporary of the computation |
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
 *    1. If `(int)x` is equal to `0`:
 *       1. Return `x`.
 * 4. Set `z` to `x*x`.
 * 5. Set `v` to `z*x`.
 * 6. Set `r` to `S2+z*(S3+z*(S4+z*(S5+z*S6)))`.
 * 7. If `iy` is equal to `0`:
 *    1. Return `x+v*(S1+z*r)`.
 * 8. Otherwise:
 *    1. Return `x-((z*(half*y-v*r)-y)-v*S1)`.
 *
 * @calls
 * None.
 */
double
__kernel_sin(double x, double y, int iy)
{
	double z,r,v;
	int32_t ix;
	GET_HIGH_WORD(ix,x);
	ix &= 0x7fffffff;			/* high word of x */
	if(ix<0x3e400000)			/* |x| < 2**-27 */
	   {if((int)x==0) return x;}		/* generate inexact */
	z	=  x*x;
	v	=  z*x;
	r	=  S2+z*(S3+z*(S4+z*(S5+z*S6)));
	if(iy==0) return x+v*(S1+z*r);
	else      return x-((z*(half*y-v*r)-y)-v*S1);
}

#endif
