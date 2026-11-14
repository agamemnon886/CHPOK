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

/* e_acosf.c -- float version of e_acos.c.
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
 * `__ieee754_acosf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

static const float
one =  1.0000000000e+00, /* 0x3F800000 */
pi =  3.1415925026e+00, /* 0x40490fda */
pio2_hi =  1.5707962513e+00, /* 0x3fc90fda */
pio2_lo =  7.5497894159e-08, /* 0x33a22168 */
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
 * @funcname `__ieee754_acosf`
 *
 * @description
 * Computes the arc cosine of x, in float: the IEEE 754 core of acosf().
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
 * | `pS0` | `const float` | A constant of the algorithm |
 * | `pS1` | `const float` | A constant of the algorithm |
 * | `pS2` | `const float` | A constant of the algorithm |
 * | `pS3` | `const float` | A constant of the algorithm |
 * | `pS4` | `const float` | A constant of the algorithm |
 * | `pS5` | `const float` | A constant of the algorithm |
 * | `pi` | `const float` | A constant of the algorithm |
 * | `pio2_hi` | `const float` | A constant of the algorithm |
 * | `pio2_lo` | `const float` | A constant of the algorithm |
 * | `qS1` | `const float` | A constant of the algorithm |
 * | `qS2` | `const float` | A constant of the algorithm |
 * | `qS3` | `const float` | A constant of the algorithm |
 * | `qS4` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `float` | A temporary of the computation |
 * | `df` | `float` | A temporary of the computation |
 * | `hx` | `int32_t` | The bits of x |
 * | `idf` | `int32_t` | The bits of df |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `p` | `float` | A temporary of the computation |
 * | `q` | `float` | A temporary of the computation |
 * | `r` | `float` | A temporary of the computation |
 * | `s` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `hx&0x7fffffff`.
 * 3. If `ix` is equal to `0x3f800000`:
 *    1. If `hx` is greater than `0`:
 *       1. Return `0.0`.
 *    2. Otherwise:
 *       1. Return `pi+(float)2.0*pio2_lo`.
 * 4. Otherwise, if `ix` is greater than `0x3f800000`:
 *    1. Return `(x-x)/(x-x)`.
 * 5. If `ix` is less than `0x3f000000`:
 *    1. If `ix` is less than or equal to `0x23000000`:
 *       1. Return `pio2_hi+pio2_lo`.
 *    2. Set `z` to `x*x`.
 *    3. Set `p` to `z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))))`.
 *    4. Set `q` to `one+z*(qS1+z*(qS2+z*(qS3+z*qS4)))`.
 *    5. Set `r` to `p/q`.
 *    6. Return `pio2_hi - (x - (pio2_lo-x*r))`.
 * 6. Otherwise, if `hx` is less than `0`:
 *    1. Set `z` to `(one+x)*(float)0.5`.
 *    2. Set `p` to `z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))))`.
 *    3. Set `q` to `one+z*(qS1+z*(qS2+z*(qS3+z*qS4)))`.
 *    4. Call `__ieee754_sqrtf()` with the argument `z`, and store the result
 *       in `s`.
 *    5. Set `r` to `p/q`.
 *    6. Set `w` to `r*s-pio2_lo`.
 *    7. Return `pi - (float)2.0*(s+w)`.
 * 7. Otherwise:
 *    1. Set `z` to `(one-x)*(float)0.5`.
 *    2. Call `__ieee754_sqrtf()` with the argument `z`, and store the result
 *       in `s`.
 *    3. Set `df` to `s`.
 *    4. Use the macro `GET_FLOAT_WORD()` with the arguments `idf` and `df`.
 *    5. Use the macro `SET_FLOAT_WORD()` with the arguments `df` and
 *       `idf&0xfffff000`.
 *    6. Set `c` to `(z-df*df)/(s+df)`.
 *    7. Set `p` to `z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))))`.
 *    8. Set `q` to `one+z*(qS1+z*(qS2+z*(qS3+z*qS4)))`.
 *    9. Set `r` to `p/q`.
 *    10. Set `w` to `r*s+c`.
 *    11. Return `(float)2.0*(df+w)`.
 *
 * @calls
 * - `__ieee754_sqrtf()`
 */
float
__ieee754_acosf(float x)
{
	float z,p,q,r,w,s,c,df;
	int32_t hx,ix;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix==0x3f800000) {		/* |x|==1 */
	    if(hx>0) return 0.0;	/* acos(1) = 0  */
	    else return pi+(float)2.0*pio2_lo;	/* acos(-1)= pi */
	} else if(ix>0x3f800000) {	/* |x| >= 1 */
	    return (x-x)/(x-x);		/* acos(|x|>1) is NaN */
	}
	if(ix<0x3f000000) {	/* |x| < 0.5 */
	    if(ix<=0x23000000) return pio2_hi+pio2_lo;/*if|x|<2**-57*/
	    z = x*x;
	    p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))));
	    q = one+z*(qS1+z*(qS2+z*(qS3+z*qS4)));
	    r = p/q;
	    return pio2_hi - (x - (pio2_lo-x*r));
	} else  if (hx<0) {		/* x < -0.5 */
	    z = (one+x)*(float)0.5;
	    p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))));
	    q = one+z*(qS1+z*(qS2+z*(qS3+z*qS4)));
	    s = __ieee754_sqrtf(z);
	    r = p/q;
	    w = r*s-pio2_lo;
	    return pi - (float)2.0*(s+w);
	} else {			/* x > 0.5 */
	    int32_t idf;
	    z = (one-x)*(float)0.5;
	    s = __ieee754_sqrtf(z);
	    df = s;
	    GET_FLOAT_WORD(idf,df);
	    SET_FLOAT_WORD(df,idf&0xfffff000);
	    c  = (z-df*df)/(s+df);
	    p = z*(pS0+z*(pS1+z*(pS2+z*(pS3+z*(pS4+z*pS5)))));
	    q = one+z*(qS1+z*(qS2+z*(qS3+z*qS4)));
	    r = p/q;
	    w = r*s+c;
	    return (float)2.0*(df+w);
	}
}
#endif

