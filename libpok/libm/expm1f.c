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

/* s_expm1f.c -- float version of s_expm1.c.
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
 * expm1f() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float huge = 1.0e+30, tiny = 1.0e-30;

static const float
one		= 1.0,
o_threshold	= 8.8721679688e+01,/* 0x42b17180 */
ln2_hi		= 6.9313812256e-01,/* 0x3f317180 */
ln2_lo		= 9.0580006145e-06,/* 0x3717f7d1 */
invln2		= 1.4426950216e+00,/* 0x3fb8aa3b */
	/* scaled coefficients related to expm1 */
Q1  =  -3.3333335072e-02, /* 0xbd088889 */
Q2  =   1.5873016091e-03, /* 0x3ad00d01 */
Q3  =  -7.9365076090e-05, /* 0xb8a670cd */
Q4  =   4.0082177293e-06, /* 0x36867e54 */
Q5  =  -2.0109921195e-07; /* 0xb457edbb */

/**
 * @funcname `expm1f`
 *
 * @description
 * Computes exp(x) - 1, accurate near 0, in float (C standard).
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
 * | `Q1` | `const float` | A constant of the algorithm |
 * | `Q2` | `const float` | A constant of the algorithm |
 * | `Q3` | `const float` | A constant of the algorithm |
 * | `Q4` | `const float` | A constant of the algorithm |
 * | `Q5` | `const float` | A constant of the algorithm |
 * | `huge` | `const float` | A constant of the algorithm |
 * | `invln2` | `const float` | A constant of the algorithm |
 * | `ln2_hi` | `const float` | A constant of the algorithm |
 * | `ln2_lo` | `const float` | A constant of the algorithm |
 * | `o_threshold` | `const float` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 * | `tiny` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `float` | A temporary of the computation |
 * | `e` | `float` | A temporary of the computation |
 * | `hfx` | `float` | A temporary of the computation |
 * | `hi` | `float` | A temporary of the computation |
 * | `hx` | `uint32_t` | The bits of x |
 * | `hxs` | `float` | A temporary of the computation |
 * | `i` | `int32_t` | The bits of y |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `lo` | `float` | A temporary of the computation |
 * | `r1` | `float` | A temporary of the computation |
 * | `t` | `float` | A temporary of the computation |
 * | `xsb` | `int32_t` | A temporary of the computation |
 * | `y` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `c` to `0`.
 * 2. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 3. Set `xsb` to `hx&0x80000000`.
 * 4. If `xsb` is equal to `0`:
 *    1. Set `y` to `x`.
 * 5. Otherwise:
 *    1. Set `y` to `-x`.
 * 6. Keep only the bits `0x7fffffff` of `hx`.
 * 7. If `hx` is greater than or equal to `0x4195b844`:
 *    1. If `hx` is greater than or equal to `0x42b17218`:
 *       1. If `hx` is greater than `0x7f800000`:
 *          1. Return `x+x`.
 *       2. If `hx` is equal to `0x7f800000`:
 *          1. Return `x` if `xsb` is equal to `0`, otherwise `-1.0`.
 *       3. If `x` is greater than `o_threshold`:
 *          1. Return `huge*huge`.
 *    2. If `xsb` is not equal to `0`:
 *       1. If `x+tiny` is less than `(float)0.0`:
 *          1. Return `tiny-one`.
 * 8. If `hx` is greater than `0x3eb17218`:
 *    1. If `hx` is less than `0x3F851592`:
 *       1. If `xsb` is equal to `0`:
 *          1. Set `hi` to `x - ln2_hi`.
 *          2. Set `lo` to `ln2_lo`.
 *          3. Set `k` to `1`.
 *       2. Otherwise:
 *          1. Set `hi` to `x + ln2_hi`.
 *          2. Set `lo` to `-ln2_lo`.
 *          3. Set `k` to `-1`.
 *    2. Otherwise:
 *       1. Set `k` to `invln2*x+((xsb==0)?(float)0.5:(float)-0.5)`.
 *       2. Set `t` to `k`.
 *       3. Set `hi` to `x - t*ln2_hi`.
 *       4. Set `lo` to `t*ln2_lo`.
 *    3. Set `x` to `hi - lo`.
 *    4. Set `c` to `(hi-x)-lo`.
 * 9. Otherwise, if `hx` is less than `0x33000000`:
 *    1. Set `t` to `huge+x`.
 *    2. Return `x - (t-(huge+x))`.
 * 10. Otherwise:
 *     1. Set `k` to `0`.
 * 11. Set `hfx` to `(float)0.5*x`.
 * 12. Set `hxs` to `x*hfx`.
 * 13. Set `r1` to `one+hxs*(Q1+hxs*(Q2+hxs*(Q3+hxs*(Q4+hxs*Q5))))`.
 * 14. Set `t` to `(float)3.0-r1*hfx`.
 * 15. Set `e` to `hxs*((r1-t)/((float)6.0 - x*t))`.
 * 16. If `k` is equal to `0`:
 *     1. Return `x - (x*e-hxs)`.
 * 17. Otherwise:
 *     1. Set `e` to `(x*(e-c)-c)`.
 *     2. Subtract `hxs` from `e`.
 *     3. If `k` is equal to `-1`:
 *        1. Return `(float)0.5*(x-e)-(float)0.5`.
 *     4. If `k` is equal to `1`:
 *        1. If `x` is less than `(float)-0.25`:
 *           1. Return `-(float)2.0*(e-(x+(float)0.5))`.
 *        2. Otherwise:
 *           1. Return `one+(float)2.0*(x-e)`.
 *     5. If `k` is less than or equal to `-2` or `k` is greater than `56`:
 *        1. Set `y` to `one-(e-x)`.
 *        2. Use the macro `GET_FLOAT_WORD()` with the arguments `i` and `y`.
 *        3. Use the macro `SET_FLOAT_WORD()` with the arguments `y` and
 *           `i+(k<<23)`.
 *        4. Return `y-one`.
 *     6. Set `t` to `one`.
 *     7. If `k` is less than `23`:
 *        1. Use the macro `SET_FLOAT_WORD()` with the arguments `t` and
 *           `0x3f800000 - (0x1000000>>k)`.
 *        2. Set `y` to `t-(e-x)`.
 *        3. Use the macro `GET_FLOAT_WORD()` with the arguments `i` and `y`.
 *        4. Use the macro `SET_FLOAT_WORD()` with the arguments `y` and
 *           `i+(k<<23)`.
 *     8. Otherwise:
 *        1. Use the macro `SET_FLOAT_WORD()` with the arguments `t` and
 *           `((0x7f-k)<<23)`.
 *        2. Set `y` to `x-(e+t)`.
 *        3. Add `one` to `y`.
 *        4. Use the macro `GET_FLOAT_WORD()` with the arguments `i` and `y`.
 *        5. Use the macro `SET_FLOAT_WORD()` with the arguments `y` and
 *           `i+(k<<23)`.
 * 18. Return `y`.
 *
 * @calls
 * None.
 */
float
expm1f(float x)
{
	float y,hi,lo,c,t,e,hxs,hfx,r1;
	int32_t k,xsb;
	uint32_t hx;

	c = 0;
	GET_FLOAT_WORD(hx,x);
	xsb = hx&0x80000000;		/* sign bit of x */
	if(xsb==0) y=x; else y= -x;	/* y = |x| */
	hx &= 0x7fffffff;		/* high word of |x| */

    /* filter out huge and non-finite argument */
	if(hx >= 0x4195b844) {			/* if |x|>=27*ln2 */
	    if(hx >= 0x42b17218) {		/* if |x|>=88.721... */
                if(hx>0x7f800000)
		    return x+x; 	 /* NaN */
		if(hx==0x7f800000)
		    return (xsb==0)? x:-1.0;/* exp(+-inf)={inf,-1} */
	        if(x > o_threshold) return huge*huge; /* overflow */
	    }
	    if(xsb!=0) { /* x < -27*ln2, return -1.0 with inexact */
		if(x+tiny<(float)0.0)	/* raise inexact */
		return tiny-one;	/* return -1 */
	    }
	}

    /* argument reduction */
	if(hx > 0x3eb17218) {		/* if  |x| > 0.5 ln2 */
	    if(hx < 0x3F851592) {	/* and |x| < 1.5 ln2 */
		if(xsb==0)
		    {hi = x - ln2_hi; lo =  ln2_lo;  k =  1;}
		else
		    {hi = x + ln2_hi; lo = -ln2_lo;  k = -1;}
	    } else {
		k  = invln2*x+((xsb==0)?(float)0.5:(float)-0.5);
		t  = k;
		hi = x - t*ln2_hi;	/* t*ln2_hi is exact here */
		lo = t*ln2_lo;
	    }
	    x  = hi - lo;
	    c  = (hi-x)-lo;
	}
	else if(hx < 0x33000000) {  	/* when |x|<2**-25, return x */
	    t = huge+x;	/* return x with inexact flags when x!=0 */
	    return x - (t-(huge+x));
	}
	else k = 0;

    /* x is now in primary range */
	hfx = (float)0.5*x;
	hxs = x*hfx;
	r1 = one+hxs*(Q1+hxs*(Q2+hxs*(Q3+hxs*(Q4+hxs*Q5))));
	t  = (float)3.0-r1*hfx;
	e  = hxs*((r1-t)/((float)6.0 - x*t));
	if(k==0) return x - (x*e-hxs);		/* c is 0 */
	else {
	    e  = (x*(e-c)-c);
	    e -= hxs;
	    if(k== -1) return (float)0.5*(x-e)-(float)0.5;
	    if(k==1)  {
	       	if(x < (float)-0.25) return -(float)2.0*(e-(x+(float)0.5));
	       	else 	      return  one+(float)2.0*(x-e);
	    }
	    if (k <= -2 || k>56) {   /* suffice to return exp(x)-1 */
	        int32_t i;
	        y = one-(e-x);
		GET_FLOAT_WORD(i,y);
		SET_FLOAT_WORD(y,i+(k<<23));	/* add k to y's exponent */
	        return y-one;
	    }
	    t = one;
	    if(k<23) {
	        int32_t i;
	        SET_FLOAT_WORD(t,0x3f800000 - (0x1000000>>k)); /* t=1-2^-k */
	       	y = t-(e-x);
		GET_FLOAT_WORD(i,y);
		SET_FLOAT_WORD(y,i+(k<<23));	/* add k to y's exponent */
	   } else {
	        int32_t i;
		SET_FLOAT_WORD(t,((0x7f-k)<<23));	/* 2^-k */
	       	y = x-(e+t);
	       	y += one;
		GET_FLOAT_WORD(i,y);
		SET_FLOAT_WORD(y,i+(k<<23));	/* add k to y's exponent */
	    }
	}
	return y;
}

#endif

