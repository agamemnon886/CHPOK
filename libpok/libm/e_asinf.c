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

/* e_asinf.c -- float version of e_asin.c.
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
 * `__ieee754_asinf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH
#include <libm.h>
#include "math_private.h"

static const float
one =  1.0000000000e+00, /* 0x3F800000 */
huge =  1.000e+30,
pio2_hi =  1.5707962513e+00, /* 0x3fc90fda */
pio2_lo =  7.5497894159e-08, /* 0x33a22168 */
pio4_hi =  7.8539818525e-01, /* 0x3f490fdb */
	/* coefficient for R(x^2) */
pS0 =  1.6666667163e-01, /* 0x3e2aaaab */
pS1 = -3.2556581497e-01, /* 0xbea6b090 */
pS2 =  2.0121252537e-01, /* 0x3e4e0aa8 */
pS3 = -4.0055535734e-02, /* 0xbd241146 */
pS4 =  7.9153501429e-04, /* 0x3a4f7f04 */
pS5 =  3.4793309169e-05, /* 0x3811ef08 */
qS1 = -2.4033949375e+00, /* 0xc019d139 */
qS2 =  2.0209457874e+00, /* 0x4001572d */
qS3 = -6.8828397989e-01, /* 0xbf303361 */
qS4 =  7.7038154006e-02; /* 0x3d9dc62e */

/**
 * @funcname `__ieee754_asinf`
 *
 * @description
 * Computes the arc sine of x, in float: the IEEE 754 core of asinf().
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
 * | `one` | `const float` | A constant of the algorithm |
 * | `pS0` | `const float` | A constant of the algorithm |
 * | `pS1` | `const float` | A constant of the algorithm |
 * | `pS2` | `const float` | A constant of the algorithm |
 * | `pS3` | `const float` | A constant of the algorithm |
 * | `pS4` | `const float` | A constant of the algorithm |
 * | `pS5` | `const float` | A constant of the algorithm |
 * | `pio2_hi` | `const float` | A constant of the algorithm |
 * | `pio2_lo` | `const float` | A constant of the algorithm |
 * | `pio4_hi` | `const float` | A constant of the algorithm |
 * | `qS1` | `const float` | A constant of the algorithm |
 * | `qS2` | `const float` | A constant of the algorithm |
 * | `qS3` | `const float` | A constant of the algorithm |
 * | `qS4` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `float` | A temporary of the computation |
 * | `hx` | `int32_t` | The bits of x |
 * | `iw` | `int32_t` | The bits of w |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `p` | `float` | A temporary of the computation |
 * | `q` | `float` | A temporary of the computation |
 * | `r` | `float` | A temporary of the computation |
 * | `s` | `float` | A temporary of the computation |
 * | `t` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `t` to `0`.
 * 2. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 3. Set `ix` to `hx&0x7fffffff`.
 * 4. If `ix` is equal to `0x3f800000`:
 *    1. Return `x*pio2_hi+x*pio2_lo`.
 * 5. Otherwise, if `ix` is greater than `0x3f800000`:
 *    1. Return `(x-x)/(x-x)`.
 * 6. Otherwise, if `ix` is less than `0x3f000000`:
 *    1. If `ix` is less than `0x32000000`:
 *       1. If `huge+x` is greater than `one`:
 *          1. Return `x`.
 *    2. Set `t` to `x*x`.
 *    3. Set `p` to `t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5)))))`.
 *    4. Set `q` to `one+t*(qS1+t*(qS2+t*(qS3+t*qS4)))`.
 *    5. Set `w` to `p/q`.
 *    6. Return `x+x*w`.
 * 7. Set `w` to `one-fabsf(x)`.
 * 8. Set `t` to `w*(float)0.5`.
 * 9. Set `p` to `t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5)))))`.
 * 10. Set `q` to `one+t*(qS1+t*(qS2+t*(qS3+t*qS4)))`.
 * 11. Call `__ieee754_sqrtf()` with the argument `t`, and store the result in
 *     `s`.
 * 12. If `ix` is greater than or equal to `0x3F79999A`:
 *     1. Set `w` to `p/q`.
 *     2. Set `t` to `pio2_hi-((float)2.0*(s+s*w)-pio2_lo)`.
 * 13. Otherwise:
 *     1. Set `w` to `s`.
 *     2. Use the macro `GET_FLOAT_WORD()` with the arguments `iw` and `w`.
 *     3. Use the macro `SET_FLOAT_WORD()` with the arguments `w` and
 *        `iw&0xfffff000`.
 *     4. Set `c` to `(t-w*w)/(s+w)`.
 *     5. Set `r` to `p/q`.
 *     6. Set `p` to `(float)2.0*s*r-(pio2_lo-(float)2.0*c)`.
 *     7. Set `q` to `pio4_hi-(float)2.0*w`.
 *     8. Set `t` to `pio4_hi-(p-q)`.
 * 14. If `hx` is greater than `0`:
 *     1. Return `t`.
 * 15. Otherwise:
 *     1. Return `-t`.
 *
 * @calls
 * - `fabsf()`
 * - `__ieee754_sqrtf()`
 */
float
__ieee754_asinf(float x)
{
	float t,w,p,q,c,r,s;
	int32_t hx,ix;

	t = 0;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix==0x3f800000) {
		/* asin(1)=+-pi/2 with inexact */
	    return x*pio2_hi+x*pio2_lo;
	} else if(ix> 0x3f800000) {	/* |x|>= 1 */
	    return (x-x)/(x-x);		/* asin(|x|>1) is NaN */
	} else if (ix<0x3f000000) {	/* |x|<0.5 */
	    if(ix<0x32000000) {		/* if |x| < 2**-27 */
		if(huge+x>one) return x;/* return x with inexact if x!=0*/
	    } 
		t = x*x;
		p = t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5)))));
		q = one+t*(qS1+t*(qS2+t*(qS3+t*qS4)));
		w = p/q;
		return x+x*w;
	}
	/* 1> |x|>= 0.5 */
	w = one-fabsf(x);
	t = w*(float)0.5;
	p = t*(pS0+t*(pS1+t*(pS2+t*(pS3+t*(pS4+t*pS5)))));
	q = one+t*(qS1+t*(qS2+t*(qS3+t*qS4)));
	s = __ieee754_sqrtf(t);
	if(ix>=0x3F79999A) { 	/* if |x| > 0.975 */
	    w = p/q;
	    t = pio2_hi-((float)2.0*(s+s*w)-pio2_lo);
	} else {
	    int32_t iw;
	    w  = s;
	    GET_FLOAT_WORD(iw,w);
	    SET_FLOAT_WORD(w,iw&0xfffff000);
	    c  = (t-w*w)/(s+w);
	    r  = p/q;
	    p  = (float)2.0*s*r-(pio2_lo-(float)2.0*c);
	    q  = pio4_hi-(float)2.0*w;
	    t  = pio4_hi-(p-q);
	}
	if(hx>0) return t; else return -t;
}
#endif
