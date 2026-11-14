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

/* @(#)e_exp.c 5.1 93/09/24 */
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
 * `__ieee754_exp()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
/* __ieee754_exp(x)
 * Returns the exponential of x.
 *
 * Method
 *   1. Argument reduction:
 *      Reduce x to an r so that |r| <= 0.5*ln2 ~ 0.34658.
 *	Given x, find r and integer k such that
 *
 *               x = k*ln2 + r,  |r| <= 0.5*ln2.
 *
 *      Here r will be represented as r = hi-lo for better
 *	accuracy.
 *
 *   2. Approximation of exp(r) by a special rational function on
 *	the interval [0,0.34658]:
 *	Write
 *	    R(r**2) = r*(exp(r)+1)/(exp(r)-1) = 2 + r*r/6 - r**4/360 + ...
 *      We use a special Reme algorithm on [0,0.34658] to generate
 * 	a polynomial of degree 5 to approximate R. The maximum error
 *	of this polynomial approximation is bounded by 2**-59. In
 *	other words,
 *	    R(z) ~ 2.0 + P1*z + P2*z**2 + P3*z**3 + P4*z**4 + P5*z**5
 *  	(where z=r*r, and the values of P1 to P5 are listed below)
 *	and
 *	    |                  5          |     -59
 *	    | 2.0+P1*z+...+P5*z   -  R(z) | <= 2
 *	    |                             |
 *	The computation of exp(r) thus becomes
 *                             2*r
 *		exp(r) = 1 + -------
 *		              R - r
 *                                 r*R1(r)
 *		       = 1 + r + ----------- (for better accuracy)
 *		                  2 - R1(r)
 *	where
 *			         2       4             10
 *		R1(r) = r - (P1*r  + P2*r  + ... + P5*r   ).
 *
 *   3. Scale back to obtain exp(x):
 *	From step 1, we have
 *	   exp(x) = 2^k * exp(r)
 *
 * Special cases:
 *	exp(INF) is INF, exp(NaN) is NaN;
 *	exp(-INF) is 0, and
 *	for finite argument, only exp(0)=1 is exact.
 *
 * Accuracy:
 *	according to an error analysis, the error is always less than
 *	1 ulp (unit in the last place).
 *
 * Misc. info.
 *	For IEEE double
 *	    if x >  7.09782712893383973096e+02 then exp(x) overflow
 *	    if x < -7.45133219101941108420e+02 then exp(x) underflow
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

static const double
one	= 1.0,
halF[2]	= {0.5,-0.5,},
huge	= 1.0e+300,
twom1000= 9.33263618503218878990e-302,     /* 2**-1000=0x01700000,0*/
o_threshold=  7.09782712893383973096e+02,  /* 0x40862E42, 0xFEFA39EF */
u_threshold= -7.45133219101941108420e+02,  /* 0xc0874910, 0xD52D3051 */
ln2HI[2]   ={ 6.93147180369123816490e-01,  /* 0x3fe62e42, 0xfee00000 */
	     -6.93147180369123816490e-01,},/* 0xbfe62e42, 0xfee00000 */
ln2LO[2]   ={ 1.90821492927058770002e-10,  /* 0x3dea39ef, 0x35793c76 */
	     -1.90821492927058770002e-10,},/* 0xbdea39ef, 0x35793c76 */
invln2 =  1.44269504088896338700e+00, /* 0x3ff71547, 0x652b82fe */
P1   =  1.66666666666666019037e-01, /* 0x3FC55555, 0x5555553E */
P2   = -2.77777777770155933842e-03, /* 0xBF66C16C, 0x16BEBD93 */
P3   =  6.61375632143793436117e-05, /* 0x3F11566A, 0xAF25DE2C */
P4   = -1.65339022054652515390e-06, /* 0xBEBBBD41, 0xC5D26BF1 */
P5   =  4.13813679705723846039e-08; /* 0x3E663769, 0x72BEA4D0 */


/**
 * @funcname `__ieee754_exp`
 *
 * @description
 * Computes e raised to the power x: the IEEE 754 core of exp().
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
 * | `P1` | `const double` | A constant of the algorithm |
 * | `P2` | `const double` | A constant of the algorithm |
 * | `P3` | `const double` | A constant of the algorithm |
 * | `P4` | `const double` | A constant of the algorithm |
 * | `P5` | `const double` | A constant of the algorithm |
 * | `halF` | `const double[2]` | A constant of the algorithm |
 * | `huge` | `const double` | A constant of the algorithm |
 * | `invln2` | `const double` | A constant of the algorithm |
 * | `ln2HI` | `const double[2]` | A constant of the algorithm |
 * | `ln2LO` | `const double[2]` | A constant of the algorithm |
 * | `o_threshold` | `const double` | A constant of the algorithm |
 * | `one` | `const double` | A constant of the algorithm |
 * | `twom1000` | `const double` | A constant of the algorithm |
 * | `u_threshold` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `double` | A temporary of the computation |
 * | `hi` | `double` | A temporary of the computation |
 * | `hx` | `uint32_t` | The high word of x |
 * | `hy` | `uint32_t` | The high word of y |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `lo` | `double` | A temporary of the computation |
 * | `lx` | `uint32_t` | The low word of x |
 * | `t` | `double` | A temporary of the computation |
 * | `xsb` | `int32_t` | A temporary of the computation |
 * | `y` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `hi` to `lo = 0`.
 * 2. Set `k` to `0`.
 * 3. Use the macro `GET_HIGH_WORD()` with the arguments `hx` and `x`.
 * 4. Set `xsb` to `(hx>>31)&1`.
 * 5. Keep only the bits `0x7fffffff` of `hx`.
 * 6. If `hx` is greater than or equal to `0x40862E42`:
 *    1. If `hx` is greater than or equal to `0x7ff00000`:
 *       1. Use the macro `GET_LOW_WORD()` with the arguments `lx` and `x`.
 *       2. If `((hx&0xfffff)|lx)` is not equal to `0`:
 *          1. Return `x+x`.
 *       3. Otherwise:
 *          1. Return `x` if `xsb` is equal to `0`, otherwise `0.0`.
 *    2. If `x` is greater than `o_threshold`:
 *       1. Return `huge*huge`.
 *    3. If `x` is less than `u_threshold`:
 *       1. Return `twom1000*twom1000`.
 * 7. If `hx` is greater than `0x3fd62e42`:
 *    1. If `hx` is less than `0x3FF0A2B2`:
 *       1. Set `hi` to `x-ln2HI[xsb]`.
 *       2. Set `lo` to `ln2LO[xsb]`.
 *       3. Set `k` to `1-xsb-xsb`.
 *    2. Otherwise:
 *       1. Set `k` to `invln2*x+halF[xsb]`.
 *       2. Set `t` to `k`.
 *       3. Set `hi` to `x - t*ln2HI[0]`.
 *       4. Set `lo` to `t*ln2LO[0]`.
 *    3. Set `x` to `hi - lo`.
 * 8. Otherwise, if `hx` is less than `0x3e300000`:
 *    1. If `huge+x` is greater than `one`:
 *       1. Return `one+x`.
 * 9. Otherwise:
 *    1. Set `k` to `0`.
 * 10. Set `t` to `x*x`.
 * 11. Set `c` to `x - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))))`.
 * 12. If `k` is equal to `0`:
 *     1. Return `one-((x*c)/(c-2.0)-x)`.
 * 13. Otherwise:
 *     1. Set `y` to `one-((lo-(x*c)/(2.0-c))-hi)`.
 * 14. If `k` is greater than or equal to `-1021`:
 *     1. Use the macro `GET_HIGH_WORD()` with the arguments `hy` and `y`.
 *     2. Use the macro `SET_HIGH_WORD()` with the arguments `y` and
 *        `hy+(k<<20)`.
 *     3. Return `y`.
 * 15. Otherwise:
 *     1. Use the macro `GET_HIGH_WORD()` with the arguments `hy` and `y`.
 *     2. Use the macro `SET_HIGH_WORD()` with the arguments `y` and
 *        `hy+((k+1000)<<20)`.
 *     3. Return `y*twom1000`.
 *
 * @calls
 * None.
 */
double
__ieee754_exp(double x)	/* default IEEE double exp */
{
	double y,hi,lo,c,t;
	int32_t k,xsb;
	uint32_t hx;

	hi = lo = 0;
	k = 0;
	GET_HIGH_WORD(hx,x);
	xsb = (hx>>31)&1;		/* sign bit of x */
	hx &= 0x7fffffff;		/* high word of |x| */

    /* filter out non-finite argument */
	if(hx >= 0x40862E42) {			/* if |x|>=709.78... */
            if(hx>=0x7ff00000) {
	        uint32_t lx;
		GET_LOW_WORD(lx,x);
		if(((hx&0xfffff)|lx)!=0)
		     return x+x; 		/* NaN */
		else return (xsb==0)? x:0.0;	/* exp(+-inf)={inf,0} */
	    }
	    if(x > o_threshold) return huge*huge; /* overflow */
	    if(x < u_threshold) return twom1000*twom1000; /* underflow */
	}

    /* argument reduction */
	if(hx > 0x3fd62e42) {		/* if  |x| > 0.5 ln2 */
	    if(hx < 0x3FF0A2B2) {	/* and |x| < 1.5 ln2 */
		hi = x-ln2HI[xsb]; lo=ln2LO[xsb]; k = 1-xsb-xsb;
	    } else {
		k  = invln2*x+halF[xsb];
		t  = k;
		hi = x - t*ln2HI[0];	/* t*ln2HI is exact here */
		lo = t*ln2LO[0];
	    }
	    x  = hi - lo;
	}
	else if(hx < 0x3e300000)  {	/* when |x|<2**-28 */
	    if(huge+x>one) return one+x;/* trigger inexact */
	}
	else k = 0;

    /* x is now in primary range */
	t  = x*x;
	c  = x - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
	if(k==0) 	return one-((x*c)/(c-2.0)-x);
	else 		y = one-((lo-(x*c)/(2.0-c))-hi);
	if(k >= -1021) {
	    uint32_t hy;
	    GET_HIGH_WORD(hy,y);
	    SET_HIGH_WORD(y,hy+(k<<20));	/* add k to y's exponent */
	    return y;
	} else {
	    uint32_t hy;
	    GET_HIGH_WORD(hy,y);
	    SET_HIGH_WORD(y,hy+((k+1000)<<20));	/* add k to y's exponent */
	    return y*twom1000;
	}
}
#endif
