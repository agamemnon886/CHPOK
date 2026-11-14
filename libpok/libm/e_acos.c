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

/* @(#)e_acos.c 5.1 93/09/24 */
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
 * `__ieee754_acos()` of libpok's math library (fdlibm, from Sun Microsystems).
 */

/* __ieee754_acos(x)
 * Method :
 *	acos(x)  = pi/2 - asin(x)
 *	acos(-x) = pi/2 + asin(x)
 * For |x|<=0.5
 *	acos(x) = pi/2 - (x + x*x^2*R(x^2))	(see asin.c)
 * For x>0.5
 * 	acos(x) = pi/2 - (pi/2 - 2asin(sqrt((1-x)/2)))
 *		= 2asin(sqrt((1-x)/2))
 *		= 2s + 2s*z*R(z) 	...z=(1-x)/2, s=sqrt(z)
 *		= 2f + (2c + 2s*z*R(z))
 *     where f=hi part of s, and c = (z-f*f)/(s+f) is the correction term
 *     for f so that f+c ~ sqrt(z).
 * For x<-0.5
 *	acos(x) = pi - 2asin(sqrt((1-|x|)/2))
 *		= pi - 0.5*(s+s*z*R(z)), where z=(1-|x|)/2,s=sqrt(z)
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN with invalid signal.
 *
 * Function needed: __ieee754_sqrt
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

static const double
one=  1.00000000000000000000e+00, /* 0x3FF00000, 0x00000000 */
pi =  3.14159265358979311600e+00, /* 0x400921FB, 0x54442D18 */
pio2_hi =  1.57079632679489655800e+00, /* 0x3FF921FB, 0x54442D18 */
pio2_lo =  6.12323399573676603587e-17, /* 0x3C91A626, 0x33145C07 */
pS0 =  1.66666666666666657415e-01, /* 0x3FC55555, 0x55555555 */
pS1 = -3.25565818622400915405e-01, /* 0xBFD4D612, 0x03EB6F7D */
pS2 =  2.01212532134862925881e-01, /* 0x3FC9C155, 0x0E884455 */
pS3 = -4.00555345006794114027e-02, /* 0xBFA48228, 0xB5688F3B */
pS4 =  7.91534994289814532176e-04, /* 0x3F49EFE0, 0x7501B288 */
pS5 =  3.47933107596021167570e-05, /* 0x3F023DE1, 0x0DFDF709 */
qS1 = -2.40339491173441421878e+00, /* 0xC0033A27, 0x1C8A2D4B */
qS2 =  2.02094576023350569471e+00, /* 0x40002AE5, 0x9C598AC8 */
qS3 = -6.88283971605453293030e-01, /* 0xBFE6066C, 0x1B8D0159 */
qS4 =  7.70381505559019352791e-02; /* 0x3FB3B8C5, 0xB12E9282 */

/**
 * @funcname `__ieee754_acos`
 *
 * @description
 * Computes the arc cosine of x: the IEEE 754 core of acos().
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
 * | `pS0` | `const double` | A constant of the algorithm |
 * | `pS1` | `const double` | A constant of the algorithm |
 * | `pS2` | `const double` | A constant of the algorithm |
 * | `pS3` | `const double` | A constant of the algorithm |
 * | `pS4` | `const double` | A constant of the algorithm |
 * | `pS5` | `const double` | A constant of the algorithm |
 * | `pi` | `const double` | A constant of the algorithm |
 * | `pio2_hi` | `const double` | A constant of the algorithm |
 * | `pio2_lo` | `const double` | A constant of the algorithm |
 * | `qS1` | `const double` | A constant of the algorithm |
 * | `qS2` | `const double` | A constant of the algorithm |
 * | `qS3` | `const double` | A constant of the algorithm |
 * | `qS4` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `double` | A temporary of the computation |
 * | `df` | `double` | A temporary of the computation |
 * | `hx` | `int32_t` | The high word of x |
 * | `ix` | `int32_t` | The high word of x without the sign bit |
 * | `lx` | `uint32_t` | The low word of x |
 * | `p` | `double` | A temporary of the computation |
 * | `q` | `double` | A temporary of the computation |
 * | `r` | `double` | A temporary of the computation |
 * | `s` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 * | `z` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_HIGH_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `hx&0x7fffffff`.
 * 3. If `ix` is greater than or equal to `0x3ff00000`:
 *    1. Use the macro `GET_LOW_WORD()` with the arguments `lx` and `x`.
 *    2. If `((ix-0x3ff00000)|lx)` is equal to `0`:
 *       1. If `hx` is greater than `0`:
 *          1. Return `0.0`.
 *       2. Otherwise:
 *          1. Return `pi+2.0*pio2_lo`.
 *    3. Return `(x-x)/(x-x)`.
 * 4. If `ix` is less than `0x3fe00000`:
 *    1. If `ix` is less than or equal to `0x3c600000`:
 *       1. Return `pio2_hi+pio2_lo`.
 *    2. Set `z` to `x*x`.
 *    3. Set `p` to `z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))))`.
 *    4. Set `q` to `one+z*(qS1+z*(qS2+z*(qS3+z*qS4)))`.
 *    5. Set `r` to `p/q`.
 *    6. Return `pio2_hi - (x - (pio2_lo-x*r))`.
 * 5. Otherwise, if `hx` is less than `0`:
 *    1. Set `z` to `(one+x)*0.5`.
 *    2. Set `p` to `z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))))`.
 *    3. Set `q` to `one+z*(qS1+z*(qS2+z*(qS3+z*qS4)))`.
 *    4. Call `__ieee754_sqrt()` with the argument `z`, and store the result in
 *       `s`.
 *    5. Set `r` to `p/q`.
 *    6. Set `w` to `r*s-pio2_lo`.
 *    7. Return `pi - 2.0*(s+w)`.
 * 6. Otherwise:
 *    1. Set `z` to `(one-x)*0.5`.
 *    2. Call `__ieee754_sqrt()` with the argument `z`, and store the result in
 *       `s`.
 *    3. Set `df` to `s`.
 *    4. Use the macro `SET_LOW_WORD()` with the arguments `df` and `0`.
 *    5. Set `c` to `(z-df*df)/(s+df)`.
 *    6. Set `p` to `z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))))`.
 *    7. Set `q` to `one+z*(qS1+z*(qS2+z*(qS3+z*qS4)))`.
 *    8. Set `r` to `p/q`.
 *    9. Set `w` to `r*s+c`.
 *    10. Return `2.0*(df+w)`.
 *
 * @calls
 * - `__ieee754_sqrt()`
 */
double
__ieee754_acos(double x)
{
	double z,p,q,r,w,s,c,df;
	int32_t hx,ix;
	GET_HIGH_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix>=0x3ff00000) {	/* |x| >= 1 */
	    uint32_t lx;
	    GET_LOW_WORD(lx,x);
	    if(((ix-0x3ff00000)|lx)==0) {	/* |x|==1 */
		if(hx>0) return 0.0;		/* acos(1) = 0  */
		else return pi+2.0*pio2_lo;	/* acos(-1)= pi */
	    }
	    return (x-x)/(x-x);		/* acos(|x|>1) is NaN */
	}
	if(ix<0x3fe00000) {	/* |x| < 0.5 */
	    if(ix<=0x3c600000) return pio2_hi+pio2_lo;/*if|x|<2**-57*/
	    z = x*x;
	    p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))));
	    q = one+z*(qS1+z*(qS2+z*(qS3+z*qS4)));
	    r = p/q;
	    return pio2_hi - (x - (pio2_lo-x*r));
	} else  if (hx<0) {		/* x < -0.5 */
	    z = (one+x)*0.5;
	    p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))));
	    q = one+z*(qS1+z*(qS2+z*(qS3+z*qS4)));
	    s = __ieee754_sqrt(z);
	    r = p/q;
	    w = r*s-pio2_lo;
	    return pi - 2.0*(s+w);
	} else {			/* x > 0.5 */
	    z = (one-x)*0.5;
	    s = __ieee754_sqrt(z);
	    df = s;
	    SET_LOW_WORD(df,0);
	    c  = (z-df*df)/(s+df);
	    p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))));
	    q = one+z*(qS1+z*(qS2+z*(qS3+z*qS4)));
	    r = p/q;
	    w = r*s+c;
	    return 2.0*(df+w);
	}
}
#endif

