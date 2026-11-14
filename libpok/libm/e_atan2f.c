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

/* e_atan2f.c -- float version of e_atan2.c.
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
 * `__ieee754_atan2f()` of libpok's math library (fdlibm, from Sun
 * Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH
#include <libm.h>
#include "math_private.h"

static const float
tiny  = 1.0e-30,
zero  = 0.0,
pi_o_4  = 7.8539818525e-01, /* 0x3f490fdb */
pi_o_2  = 1.5707963705e+00, /* 0x3fc90fdb */
pi      = 3.1415925026e+00, /* 0x40490fda */
pi_lo   = 1.5099578832e-07; /* 0x34222168 */

/**
 * @funcname `__ieee754_atan2f`
 *
 * @description
 * Computes the arc tangent of y/x, in the quadrant of (x, y), in float: the
 * IEEE 754 core of atan2f().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `y` | `float` | [in] The second argument. |
 * | `x` | `float` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pi` | `const float` | A constant of the algorithm |
 * | `pi_lo` | `const float` | A constant of the algorithm |
 * | `pi_o_2` | `const float` | A constant of the algorithm |
 * | `pi_o_4` | `const float` | A constant of the algorithm |
 * | `tiny` | `const float` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The bits of x |
 * | `hy` | `int32_t` | The bits of y |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `iy` | `int32_t` | The bits of y without the sign bit |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `m` | `int32_t` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 * | `zh` | `uint32_t` | The bits of z |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `hx&0x7fffffff`.
 * 3. Use the macro `GET_FLOAT_WORD()` with the arguments `hy` and `y`.
 * 4. Set `iy` to `hy&0x7fffffff`.
 * 5. If `ix` is greater than `0x7f800000` or `iy` is greater than
 *    `0x7f800000`:
 *    1. Return `x+y`.
 * 6. If `hx` is equal to `0x3f800000`:
 *    1. Call `atanf()` with the argument `y`, and return its result.
 * 7. Set `m` to `((hy>>31)&1)|((hx>>30)&2)`.
 * 8. If `iy` is equal to `0`:
 *    1. Depending on `m`:
 *       1. Case `0` or `1`:
 *          1. Return `y`.
 *       2. Case `2`:
 *          1. Return `pi+tiny`.
 *       3. Case `3`:
 *          1. Return `-pi-tiny`.
 * 9. If `ix` is equal to `0`:
 *    1. Return `-pi_o_2-tiny` if `hy` is less than `0`, otherwise
 *       `pi_o_2+tiny`.
 * 10. If `ix` is equal to `0x7f800000`:
 *     1. If `iy` is equal to `0x7f800000`:
 *        1. Depending on `m`:
 *           1. Case `0`:
 *              1. Return `pi_o_4+tiny`.
 *           2. Case `1`:
 *              1. Return `-pi_o_4-tiny`.
 *           3. Case `2`:
 *              1. Return `(float)3.0*pi_o_4+tiny`.
 *           4. Case `3`:
 *              1. Return `(float)-3.0*pi_o_4-tiny`.
 *     2. Otherwise:
 *        1. Depending on `m`:
 *           1. Case `0`:
 *              1. Return `zero`.
 *           2. Case `1`:
 *              1. Return `-zero`.
 *           3. Case `2`:
 *              1. Return `pi+tiny`.
 *           4. Case `3`:
 *              1. Return `-pi-tiny`.
 * 11. If `iy` is equal to `0x7f800000`:
 *     1. Return `-pi_o_2-tiny` if `hy` is less than `0`, otherwise
 *        `pi_o_2+tiny`.
 * 12. Set `k` to `(iy-ix)>>23`.
 * 13. If `k` is greater than `60`:
 *     1. Set `z` to `pi_o_2+(float)0.5*pi_lo`.
 * 14. Otherwise, if `hx` is less than `0` and `k` is less than `-60`:
 *     1. Set `z` to `0.0`.
 * 15. Otherwise:
 *     1. Call `atanf()` with the argument `fabsf(y/x)`, and store the result in
 *        `z`.
 * 16. Depending on `m`:
 *     1. Case `0`:
 *        1. Return `z`.
 *     2. Case `1`:
 *        1. Use the macro `GET_FLOAT_WORD()` with the arguments `zh` and `z`.
 *        2. Use the macro `SET_FLOAT_WORD()` with the arguments `z` and
 *           `zh ^ 0x80000000`.
 *        3. Return `z`.
 *     3. Case `2`:
 *        1. Return `pi-(z-pi_lo)`.
 *     4. Otherwise (default):
 *        1. Return `(z-pi_lo)-pi`.
 *
 * @calls
 * - `atanf()`
 * - `fabsf()`
 */
float
__ieee754_atan2f(float y, float x)
{
	float z;
	int32_t k,m,hx,hy,ix,iy;

	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	GET_FLOAT_WORD(hy,y);
	iy = hy&0x7fffffff;
	if((ix>0x7f800000)||
	   (iy>0x7f800000))	/* x or y is NaN */
	   return x+y;
	if(hx==0x3f800000) return atanf(y);   /* x=1.0 */
	m = ((hy>>31)&1)|((hx>>30)&2);	/* 2*sign(x)+sign(y) */

    /* when y = 0 */
	if(iy==0) {
	    switch(m) {
		case 0:
		case 1: return y; 	/* atan(+-0,+anything)=+-0 */
		case 2: return  pi+tiny;/* atan(+0,-anything) = pi */
		case 3: return -pi-tiny;/* atan(-0,-anything) =-pi */
	    }
	}
    /* when x = 0 */
	if(ix==0) return (hy<0)?  -pi_o_2-tiny: pi_o_2+tiny;

    /* when x is INF */
	if(ix==0x7f800000) {
	    if(iy==0x7f800000) {
		switch(m) {
		    case 0: return  pi_o_4+tiny;/* atan(+INF,+INF) */
		    case 1: return -pi_o_4-tiny;/* atan(-INF,+INF) */
		    case 2: return  (float)3.0*pi_o_4+tiny;/*atan(+INF,-INF)*/
		    case 3: return (float)-3.0*pi_o_4-tiny;/*atan(-INF,-INF)*/
		}
	    } else {
		switch(m) {
		    case 0: return  zero  ;	/* atan(+...,+INF) */
		    case 1: return -zero  ;	/* atan(-...,+INF) */
		    case 2: return  pi+tiny  ;	/* atan(+...,-INF) */
		    case 3: return -pi-tiny  ;	/* atan(-...,-INF) */
		}
	    }
	}
    /* when y is INF */
	if(iy==0x7f800000) return (hy<0)? -pi_o_2-tiny: pi_o_2+tiny;

    /* compute y/x */
	k = (iy-ix)>>23;
	if(k > 60) z=pi_o_2+(float)0.5*pi_lo; 	/* |y/x| >  2**60 */
	else if(hx<0&&k<-60) z=0.0; 	/* |y|/x < -2**60 */
	else z=atanf(fabsf(y/x));	/* safe to do y/x */
	switch (m) {
	    case 0: return       z  ;	/* atan(+,+) */
	    case 1: {
	    	      uint32_t zh;
		      GET_FLOAT_WORD(zh,z);
		      SET_FLOAT_WORD(z,zh ^ 0x80000000);
		    }
		    return       z  ;	/* atan(-,+) */
	    case 2: return  pi-(z-pi_lo);/* atan(+,-) */
	    default: /* case 3 */
	    	    return  (z-pi_lo)-pi;/* atan(-,-) */
	}
}
#endif
