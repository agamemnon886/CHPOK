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

/* @(#)e_asin.c 5.1 93/09/24 */
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
 * `__ieee754_asin()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
/* __ieee754_asin(x)
 * Method :
 *	Since  asin(x) = x + x^3/6 + x^5*3/40 + x^7*15/336 + ...
 *	we approximate asin(x) on [0,0.5] by
 *		asin(x) = x + x*x^2*R(x^2)
 *	where
 *		R(x^2) is a rational approximation of (asin(x)-x)/x^3
 *	and its remez error is bounded by
 *		|(asin(x)-x)/x^3 - R(x^2)| < 2^(-58.75)
 *
 *	For x in [0.5,1]
 *		asin(x) = pi/2-2*asin(sqrt((1-x)/2))
 *	Let y = (1-x), z = y/2, s := sqrt(z), and pio2_hi+pio2_lo=pi/2;
 *	then for x>0.98
 *		asin(x) = pi/2 - 2*(s+s*z*R(z))
 *			= pio2_hi - (2*(s+s*z*R(z)) - pio2_lo)
 *	For x<=0.98, let pio4_hi = pio2_hi/2, then
 *		f = hi part of s;
 *		c = sqrt(z) - f = (z-f*f)/(s+f) 	...f+c=sqrt(z)
 *	and
 *		asin(x) = pi/2 - 2*(s+s*z*R(z))
 *			= pio4_hi+(pio4-2s)-(2s*z*R(z)-pio2_lo)
 *			= pio4_hi+(pio4-2f)-(2s*z*R(z)-(pio2_lo+2c))
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN with invalid signal.
 *
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH


#include <libm.h>
#include "math_private.h"

static const double
one =  1.00000000000000000000e+00, /* 0x3FF00000, 0x00000000 */
huge =  1.000e+300,
pio2_hi =  1.57079632679489655800e+00, /* 0x3FF921FB, 0x54442D18 */
pio2_lo =  6.12323399573676603587e-17, /* 0x3C91A626, 0x33145C07 */
pio4_hi =  7.85398163397448278999e-01, /* 0x3FE921FB, 0x54442D18 */
	/* coefficient for R(x^2) */
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
 * @funcname `__ieee754_asin`
 *
 * @description
 * Computes the arc sine of x: the IEEE 754 core of asin().
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
 * | `pS0` | `const double` | A constant of the algorithm |
 * | `pS1` | `const double` | A constant of the algorithm |
 * | `pS2` | `const double` | A constant of the algorithm |
 * | `pS3` | `const double` | A constant of the algorithm |
 * | `pS4` | `const double` | A constant of the algorithm |
 * | `pS5` | `const double` | A constant of the algorithm |
 * | `pio2_hi` | `const double` | A constant of the algorithm |
 * | `pio2_lo` | `const double` | A constant of the algorithm |
 * | `pio4_hi` | `const double` | A constant of the algorithm |
 * | `qS1` | `const double` | A constant of the algorithm |
 * | `qS2` | `const double` | A constant of the algorithm |
 * | `qS3` | `const double` | A constant of the algorithm |
 * | `qS4` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `double` | A temporary of the computation |
 * | `hx` | `int32_t` | The high word of x |
 * | `ix` | `int32_t` | The high word of x without the sign bit |
 * | `lx` | `uint32_t` | The low word of x |
 * | `p` | `double` | A temporary of the computation |
 * | `q` | `double` | A temporary of the computation |
 * | `r` | `double` | A temporary of the computation |
 * | `s` | `double` | A temporary of the computation |
 * | `t` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `t` to `0`.
 * 2. Use the macro `GET_HIGH_WORD()` with the arguments `hx` and `x`.
 * 3. Set `ix` to `hx&0x7fffffff`.
 * 4. If `ix` is greater than or equal to `0x3ff00000`:
 *    1. Use the macro `GET_LOW_WORD()` with the arguments `lx` and `x`.
 *    2. If `((ix-0x3ff00000)|lx)` is equal to `0`:
 *       1. Return `x*pio2_hi+x*pio2_lo`.
 *    3. Return `(x-x)/(x-x)`.
 * 5. Otherwise, if `ix` is less than `0x3fe00000`:
 *    1. If `ix` is less than `0x3e400000`:
 *       1. If `huge+x` is greater than `one`:
 *          1. Return `x`.
 *    2. Set `t` to `x*x`.
 *    3. Set `p` to `t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5)))))`.
 *    4. Set `q` to `one+t*(qS1+t*(qS2+t*(qS3+t*qS4)))`.
 *    5. Set `w` to `p/q`.
 *    6. Return `x+x*w`.
 * 6. Set `w` to `one-fabs(x)`.
 * 7. Set `t` to `w*0.5`.
 * 8. Set `p` to `t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5)))))`.
 * 9. Set `q` to `one+t*(qS1+t*(qS2+t*(qS3+t*qS4)))`.
 * 10. Call `__ieee754_sqrt()` with the argument `t`, and store the result in
 *     `s`.
 * 11. If `ix` is greater than or equal to `0x3FEF3333`:
 *     1. Set `w` to `p/q`.
 *     2. Set `t` to `pio2_hi-(2.0*(s+s*w)-pio2_lo)`.
 * 12. Otherwise:
 *     1. Set `w` to `s`.
 *     2. Use the macro `SET_LOW_WORD()` with the arguments `w` and `0`.
 *     3. Set `c` to `(t-w*w)/(s+w)`.
 *     4. Set `r` to `p/q`.
 *     5. Set `p` to `2.0*s*r-(pio2_lo-2.0*c)`.
 *     6. Set `q` to `pio4_hi-2.0*w`.
 *     7. Set `t` to `pio4_hi-(p-q)`.
 * 13. If `hx` is greater than `0`:
 *     1. Return `t`.
 * 14. Otherwise:
 *     1. Return `-t`.
 *
 * @calls
 * - `fabs()`
 * - `__ieee754_sqrt()`
 */
double
__ieee754_asin(double x)
{
	double t,w,p,q,c,r,s;
	int32_t hx,ix;

	t = 0;
	GET_HIGH_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix>= 0x3ff00000) {		/* |x|>= 1 */
	    uint32_t lx;
	    GET_LOW_WORD(lx,x);
	    if(((ix-0x3ff00000)|lx)==0)
		    /* asin(1)=+-pi/2 with inexact */
		return x*pio2_hi+x*pio2_lo;
	    return (x-x)/(x-x);		/* asin(|x|>1) is NaN */
	} else if (ix<0x3fe00000) {	/* |x|<0.5 */
	    if(ix<0x3e400000) {		/* if |x| < 2**-27 */
			if(huge+x>one) return x;/* return x with inexact if x!=0*/
	    }
		t = x*x;
		p = t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5)))));
		q = one+t*(qS1+t*(qS2+t*(qS3+t*qS4)));
		w = p/q;
		return x+x*w;
	}
	/* 1> |x|>= 0.5 */
	w = one-fabs(x);
	t = w*0.5;
	p = t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5)))));
	q = one+t*(qS1+t*(qS2+t*(qS3+t*qS4)));
	s = __ieee754_sqrt(t);
	if(ix>=0x3FEF3333) { 	/* if |x| > 0.975 */
	    w = p/q;
	    t = pio2_hi-(2.0*(s+s*w)-pio2_lo);
	} else {
	    w  = s;
	    SET_LOW_WORD(w,0);
	    c  = (t-w*w)/(s+w);
	    r  = p/q;
	    p  = 2.0*s*r-(pio2_lo-2.0*c);
	    q  = pio4_hi-2.0*w;
	    t  = pio4_hi-(p-q);
	}
	if(hx>0) return t; else return -t;
}

#endif
