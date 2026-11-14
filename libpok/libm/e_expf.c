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

/* e_expf.c -- float version of e_exp.c.
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
 * `__ieee754_expf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

static const float huge = 1.0e+30;

static const float
one	= 1.0,
halF[2]	= {0.5,-0.5,},
twom100 = 7.8886090522e-31,      /* 2**-100=0x0d800000 */
o_threshold=  8.8721679688e+01,  /* 0x42b17180 */
u_threshold= -1.0397208405e+02,  /* 0xc2cff1b5 */
ln2HI[2]   ={ 6.9313812256e-01,		/* 0x3f317180 */
	     -6.9313812256e-01,},	/* 0xbf317180 */
ln2LO[2]   ={ 9.0580006145e-06,  	/* 0x3717f7d1 */
	     -9.0580006145e-06,},	/* 0xb717f7d1 */
invln2 =  1.4426950216e+00, 		/* 0x3fb8aa3b */
P1   =  1.6666667163e-01, /* 0x3e2aaaab */
P2   = -2.7777778450e-03, /* 0xbb360b61 */
P3   =  6.6137559770e-05, /* 0x388ab355 */
P4   = -1.6533901999e-06, /* 0xb5ddea0e */
P5   =  4.1381369442e-08; /* 0x3331bb4c */

/**
 * @funcname `__ieee754_expf`
 *
 * @description
 * Computes e raised to the power x, in float: the IEEE 754 core of expf().
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
 * | `P1` | `const float` | A constant of the algorithm |
 * | `P2` | `const float` | A constant of the algorithm |
 * | `P3` | `const float` | A constant of the algorithm |
 * | `P4` | `const float` | A constant of the algorithm |
 * | `P5` | `const float` | A constant of the algorithm |
 * | `halF` | `const float[2]` | A constant of the algorithm |
 * | `huge` | `const float` | A constant of the algorithm |
 * | `invln2` | `const float` | A constant of the algorithm |
 * | `ln2HI` | `const float[2]` | A constant of the algorithm |
 * | `ln2LO` | `const float[2]` | A constant of the algorithm |
 * | `o_threshold` | `const float` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 * | `twom100` | `const float` | A constant of the algorithm |
 * | `u_threshold` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `float` | A temporary of the computation |
 * | `hi` | `float` | A temporary of the computation |
 * | `hx` | `uint32_t` | The bits of x |
 * | `hy` | `uint32_t` | The bits of y |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `lo` | `float` | A temporary of the computation |
 * | `t` | `float` | A temporary of the computation |
 * | `xsb` | `int32_t` | A temporary of the computation |
 * | `y` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `hi` to `lo = 0`.
 * 2. Set `k` to `0`.
 * 3. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 4. Set `xsb` to `(hx>>31)&1`.
 * 5. Keep only the bits `0x7fffffff` of `hx`.
 * 6. If `hx` is greater than or equal to `0x42b17218`:
 *    1. If `hx` is greater than `0x7f800000`:
 *       1. Return `x+x`.
 *    2. If `hx` is equal to `0x7f800000`:
 *       1. Return `x` if `xsb` is equal to `0`, otherwise `0.0`.
 *    3. If `x` is greater than `o_threshold`:
 *       1. Return `huge*huge`.
 *    4. If `x` is less than `u_threshold`:
 *       1. Return `twom100*twom100`.
 * 7. If `hx` is greater than `0x3eb17218`:
 *    1. If `hx` is less than `0x3F851592`:
 *       1. Set `hi` to `x-ln2HI[xsb]`.
 *       2. Set `lo` to `ln2LO[xsb]`.
 *       3. Set `k` to `1-xsb-xsb`.
 *    2. Otherwise:
 *       1. Set `k` to `invln2*x+halF[xsb]`.
 *       2. Set `t` to `k`.
 *       3. Set `hi` to `x - t*ln2HI[0]`.
 *       4. Set `lo` to `t*ln2LO[0]`.
 *    3. Set `x` to `hi - lo`.
 * 8. Otherwise, if `hx` is less than `0x31800000`:
 *    1. If `huge+x` is greater than `one`:
 *       1. Return `one+x`.
 * 9. Otherwise:
 *    1. Set `k` to `0`.
 * 10. Set `t` to `x*x`.
 * 11. Set `c` to `x - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))))`.
 * 12. If `k` is equal to `0`:
 *     1. Return `one-((x*c)/(c-(float)2.0)-x)`.
 * 13. Otherwise:
 *     1. Set `y` to `one-((lo-(x*c)/((float)2.0-c))-hi)`.
 * 14. If `k` is greater than or equal to `-125`:
 *     1. Use the macro `GET_FLOAT_WORD()` with the arguments `hy` and `y`.
 *     2. Use the macro `SET_FLOAT_WORD()` with the arguments `y` and
 *        `hy+(k<<23)`.
 *     3. Return `y`.
 * 15. Otherwise:
 *     1. Use the macro `GET_FLOAT_WORD()` with the arguments `hy` and `y`.
 *     2. Use the macro `SET_FLOAT_WORD()` with the arguments `y` and
 *        `hy+((k+100)<<23)`.
 *     3. Return `y*twom100`.
 *
 * @calls
 * None.
 */
float
__ieee754_expf(float x)	/* default IEEE double exp */
{
	float y,hi,lo,c,t;
	int32_t k,xsb;
	uint32_t hx;

	hi = lo = 0;
	k = 0;
	GET_FLOAT_WORD(hx,x);
	xsb = (hx>>31)&1;		/* sign bit of x */
	hx &= 0x7fffffff;		/* high word of |x| */

    /* filter out non-finite argument */
	if(hx >= 0x42b17218) {			/* if |x|>=88.721... */
	    if(hx>0x7f800000)
		 return x+x;	 		/* NaN */
            if(hx==0x7f800000)
		return (xsb==0)? x:0.0;		/* exp(+-inf)={inf,0} */
	    if(x > o_threshold) return huge*huge; /* overflow */
	    if(x < u_threshold) return twom100*twom100; /* underflow */
	}

    /* argument reduction */
	if(hx > 0x3eb17218) {		/* if  |x| > 0.5 ln2 */
	    if(hx < 0x3F851592) {	/* and |x| < 1.5 ln2 */
		hi = x-ln2HI[xsb]; lo=ln2LO[xsb]; k = 1-xsb-xsb;
	    } else {
		k  = invln2*x+halF[xsb];
		t  = k;
		hi = x - t*ln2HI[0];	/* t*ln2HI is exact here */
		lo = t*ln2LO[0];
	    }
	    x  = hi - lo;
	}
	else if(hx < 0x31800000)  {	/* when |x|<2**-28 */
	    if(huge+x>one) return one+x;/* trigger inexact */
	}
	else k = 0;

    /* x is now in primary range */
	t  = x*x;
	c  = x - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
	if(k==0) 	return one-((x*c)/(c-(float)2.0)-x);
	else 		y = one-((lo-(x*c)/((float)2.0-c))-hi);
	if(k >= -125) {
	    uint32_t hy;
	    GET_FLOAT_WORD(hy,y);
	    SET_FLOAT_WORD(y,hy+(k<<23));	/* add k to y's exponent */
	    return y;
	} else {
	    uint32_t hy;
	    GET_FLOAT_WORD(hy,y);
	    SET_FLOAT_WORD(y,hy+((k+100)<<23));	/* add k to y's exponent */
	    return y*twom100;
	}
}
#endif

