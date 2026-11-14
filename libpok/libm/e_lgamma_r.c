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

/* @(#)er_lgamma.c 5.1 93/09/24 */
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
 * sin_pi() and `__ieee754_lgamma_r()` of libpok's math library (fdlibm, from
 * Sun Microsystems).
 */
/* __ieee754_lgamma_r(x, signgamp)
 * Reentrant version of the logarithm of the Gamma function
 * with user provide pointer for the sign of Gamma(x).
 *
 * Method:
 *   1. Argument Reduction for 0 < x <= 8
 * 	Since gamma(1+s)=s*gamma(s), for x in [0,8], we may
 * 	reduce x to a number in [1.5,2.5] by
 * 		lgamma(1+s) = log(s) + lgamma(s)
 *	for example,
 *		lgamma(7.3) = log(6.3) + lgamma(6.3)
 *			    = log(6.3*5.3) + lgamma(5.3)
 *			    = log(6.3*5.3*4.3*3.3*2.3) + lgamma(2.3)
 *   2. Polynomial approximation of lgamma around its
 *	minimun ymin=1.461632144968362245 to maintain monotonicity.
 *	On [ymin-0.23, ymin+0.27] (i.e., [1.23164,1.73163]), use
 *		Let z = x-ymin;
 *		lgamma(x) = -1.214862905358496078218 + z^2*poly(z)
 *	where
 *		poly(z) is a 14 degree polynomial.
 *   2. Rational approximation in the primary interval [2,3]
 *	We use the following approximation:
 *		s = x-2.0;
 *		lgamma(x) = 0.5*s + s*P(s)/Q(s)
 *	with accuracy
 *		|P/Q - (lgamma(x)-0.5s)| < 2**-61.71
 *	Our algorithms are based on the following observation
 *
 *                             zeta(2)-1    2    zeta(3)-1    3
 * lgamma(2+s) = s*(1-Euler) + --------- * s  -  --------- * s  + ...
 *                                 2                 3
 *
 *	where Euler = 0.5771... is the Euler constant, which is very
 *	close to 0.5.
 *
 *   3. For x>=8, we have
 *	lgamma(x)~(x-0.5)log(x)-x+0.5*log(2pi)+1/(12x)-1/(360x**3)+....
 *	(better formula:
 *	   lgamma(x)~(x-0.5)*(log(x)-1)-.5*(log(2pi)-1) + ...)
 *	Let z = 1/x, then we approximation
 *		f(z) = lgamma(x) - (x-0.5)(log(x)-1)
 *	by
 *	  			    3       5             11
 *		w = w0 + w1*z + w2*z  + w3*z  + ... + w6*z
 *	where
 *		|w - f(z)| < 2**-58.74
 *
 *   4. For negative x, since (G is gamma function)
 *		-x*G(-x)*G(x) = pi/sin(pi*x),
 * 	we have
 * 		G(x) = pi/(sin(pi*x)*(-x)*G(-x))
 *	since G(-x) is positive, sign(G(x)) = sign(sin(pi*x)) for x<0
 *	Hence, for x<0, signgam = sign(sin(pi*x)) and
 *		lgamma(x) = log(|Gamma(x)|)
 *			  = log(pi/(|x*sin(pi*x)|)) - lgamma(-x);
 *	Note: one should avoid compute pi*(-x) directly in the
 *	      computation of sin(pi*(-x)).
 *
 *   5. Special Cases
 *		lgamma(2+s) ~ s*(1-Euler) for tiny s
 *		lgamma(1)=lgamma(2)=0
 *		lgamma(x) ~ -log(x) for tiny x
 *		lgamma(0) = lgamma(inf) = inf
 *	 	lgamma(-integer) = +-inf
 *
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const double
two52=  4.50359962737049600000e+15, /* 0x43300000, 0x00000000 */
half=  5.00000000000000000000e-01, /* 0x3FE00000, 0x00000000 */
one =  1.00000000000000000000e+00, /* 0x3FF00000, 0x00000000 */
pi  =  3.14159265358979311600e+00, /* 0x400921FB, 0x54442D18 */
a0  =  7.72156649015328655494e-02, /* 0x3FB3C467, 0xE37DB0C8 */
a1  =  3.22467033424113591611e-01, /* 0x3FD4A34C, 0xC4A60FAD */
a2  =  6.73523010531292681824e-02, /* 0x3FB13E00, 0x1A5562A7 */
a3  =  2.05808084325167332806e-02, /* 0x3F951322, 0xAC92547B */
a4  =  7.38555086081402883957e-03, /* 0x3F7E404F, 0xB68FEFE8 */
a5  =  2.89051383673415629091e-03, /* 0x3F67ADD8, 0xCCB7926B */
a6  =  1.19270763183362067845e-03, /* 0x3F538A94, 0x116F3F5D */
a7  =  5.10069792153511336608e-04, /* 0x3F40B6C6, 0x89B99C00 */
a8  =  2.20862790713908385557e-04, /* 0x3F2CF2EC, 0xED10E54D */
a9  =  1.08011567247583939954e-04, /* 0x3F1C5088, 0x987DFB07 */
a10 =  2.52144565451257326939e-05, /* 0x3EFA7074, 0x428CFA52 */
a11 =  4.48640949618915160150e-05, /* 0x3F07858E, 0x90A45837 */
tc  =  1.46163214496836224576e+00, /* 0x3FF762D8, 0x6356BE3F */
tf  = -1.21486290535849611461e-01, /* 0xBFBF19B9, 0xBCC38A42 */
/* tt = -(tail of tf) */
tt  = -3.63867699703950536541e-18, /* 0xBC50C7CA, 0xA48A971F */
t0  =  4.83836122723810047042e-01, /* 0x3FDEF72B, 0xC8EE38A2 */
t1  = -1.47587722994593911752e-01, /* 0xBFC2E427, 0x8DC6C509 */
t2  =  6.46249402391333854778e-02, /* 0x3FB08B42, 0x94D5419B */
t3  = -3.27885410759859649565e-02, /* 0xBFA0C9A8, 0xDF35B713 */
t4  =  1.79706750811820387126e-02, /* 0x3F9266E7, 0x970AF9EC */
t5  = -1.03142241298341437450e-02, /* 0xBF851F9F, 0xBA91EC6A */
t6  =  6.10053870246291332635e-03, /* 0x3F78FCE0, 0xE370E344 */
t7  = -3.68452016781138256760e-03, /* 0xBF6E2EFF, 0xB3E914D7 */
t8  =  2.25964780900612472250e-03, /* 0x3F6282D3, 0x2E15C915 */
t9  = -1.40346469989232843813e-03, /* 0xBF56FE8E, 0xBF2D1AF1 */
t10 =  8.81081882437654011382e-04, /* 0x3F4CDF0C, 0xEF61A8E9 */
t11 = -5.38595305356740546715e-04, /* 0xBF41A610, 0x9C73E0EC */
t12 =  3.15632070903625950361e-04, /* 0x3F34AF6D, 0x6C0EBBF7 */
t13 = -3.12754168375120860518e-04, /* 0xBF347F24, 0xECC38C38 */
t14 =  3.35529192635519073543e-04, /* 0x3F35FD3E, 0xE8C2D3F4 */
u0  = -7.72156649015328655494e-02, /* 0xBFB3C467, 0xE37DB0C8 */
u1  =  6.32827064025093366517e-01, /* 0x3FE4401E, 0x8B005DFF */
u2  =  1.45492250137234768737e+00, /* 0x3FF7475C, 0xD119BD6F */
u3  =  9.77717527963372745603e-01, /* 0x3FEF4976, 0x44EA8450 */
u4  =  2.28963728064692451092e-01, /* 0x3FCD4EAE, 0xF6010924 */
u5  =  1.33810918536787660377e-02, /* 0x3F8B678B, 0xBF2BAB09 */
v1  =  2.45597793713041134822e+00, /* 0x4003A5D7, 0xC2BD619C */
v2  =  2.12848976379893395361e+00, /* 0x40010725, 0xA42B18F5 */
v3  =  7.69285150456672783825e-01, /* 0x3FE89DFB, 0xE45050AF */
v4  =  1.04222645593369134254e-01, /* 0x3FBAAE55, 0xD6537C88 */
v5  =  3.21709242282423911810e-03, /* 0x3F6A5ABB, 0x57D0CF61 */
s0  = -7.72156649015328655494e-02, /* 0xBFB3C467, 0xE37DB0C8 */
s1  =  2.14982415960608852501e-01, /* 0x3FCB848B, 0x36E20878 */
s2  =  3.25778796408930981787e-01, /* 0x3FD4D98F, 0x4F139F59 */
s3  =  1.46350472652464452805e-01, /* 0x3FC2BB9C, 0xBEE5F2F7 */
s4  =  2.66422703033638609560e-02, /* 0x3F9B481C, 0x7E939961 */
s5  =  1.84028451407337715652e-03, /* 0x3F5E26B6, 0x7368F239 */
s6  =  3.19475326584100867617e-05, /* 0x3F00BFEC, 0xDD17E945 */
r1  =  1.39200533467621045958e+00, /* 0x3FF645A7, 0x62C4AB74 */
r2  =  7.21935547567138069525e-01, /* 0x3FE71A18, 0x93D3DCDC */
r3  =  1.71933865632803078993e-01, /* 0x3FC601ED, 0xCCFBDF27 */
r4  =  1.86459191715652901344e-02, /* 0x3F9317EA, 0x742ED475 */
r5  =  7.77942496381893596434e-04, /* 0x3F497DDA, 0xCA41A95B */
r6  =  7.32668430744625636189e-06, /* 0x3EDEBAF7, 0xA5B38140 */
w0  =  4.18938533204672725052e-01, /* 0x3FDACFE3, 0x90C97D69 */
w1  =  8.33333333333329678849e-02, /* 0x3FB55555, 0x5555553B */
w2  = -2.77777777728775536470e-03, /* 0xBF66C16C, 0x16B02E5C */
w3  =  7.93650558643019558500e-04, /* 0x3F4A019F, 0x98CF38B6 */
w4  = -5.95187557450339963135e-04, /* 0xBF4380CB, 0x8C0FE741 */
w5  =  8.36339918996282139126e-04, /* 0x3F4B67BA, 0x4CDAD5D1 */
w6  = -1.63092934096575273989e-03; /* 0xBF5AB89D, 0x0B9E43E4 */

static const double zero=  0.00000000000000000000e+00;

/**
 * @funcname `sin_pi`
 *
 * @description
 * Returns sin(pi*x) for the reflection of lgamma().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 *
 * @return sin(pi*x).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `one` | `const double` | A constant of the algorithm |
 * | `pi` | `const double` | A constant of the algorithm |
 * | `two52` | `const double` | A constant of the algorithm |
 * | `zero` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ix` | `int` | The high word of x |
 * | `n` | `int` | The low word of z |
 * | `y` | `double` | A temporary of the computation |
 * | `z` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_HIGH_WORD()` with the arguments `ix` and `x`.
 * 2. Keep only the bits `0x7fffffff` of `ix`.
 * 3. If `ix` is less than `0x3fd00000`:
 *    1. Call `__kernel_sin()` with the arguments `pi*x`, `zero` and `0`, and
 *       return its result.
 * 4. Set `y` to `-x`.
 * 5. Call `floor()` with the argument `y`, and store the result in `z`.
 * 6. If `z` is not equal to `y`:
 *    1. Multiply `y` by `0.5`.
 *    2. Set `y` to `2.0*(y - floor(y))`.
 *    3. Set `n` to `(int) (y*4.0)`.
 * 7. Otherwise:
 *    1. If `ix` is greater than or equal to `0x43400000`:
 *       1. Set `y` to `zero`.
 *       2. Set `n` to `0`.
 *    2. Otherwise:
 *       1. If `ix` is less than `0x43300000`:
 *          1. Set `z` to `y+two52`.
 *       2. Use the macro `GET_LOW_WORD()` with the arguments `n` and `z`.
 *       3. Keep only the bits `1` of `n`.
 *       4. Set `y` to `n`.
 *       5. Shift `n` left by `2` bits.
 * 8. Depending on `n`:
 *    1. Case `0`:
 *       1. Call `__kernel_sin()` with the arguments `pi*y`, `zero` and `0`,
 *          and store the result in `y`.
 *       2. Leave the switch.
 *    2. Case `1` or `2`:
 *       1. Call `__kernel_cos()` with the arguments `pi*(0.5-y)` and `zero`,
 *          and store the result in `y`.
 *       2. Leave the switch.
 *    3. Case `3` or `4`:
 *       1. Call `__kernel_sin()` with the arguments `pi*(one-y)`, `zero` and
 *          `0`, and store the result in `y`.
 *       2. Leave the switch.
 *    4. Case `5` or `6`:
 *       1. Set `y` to `-__kernel_cos(pi*(y-1.5),zero)`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Call `__kernel_sin()` with the arguments `pi*(y-2.0)`, `zero` and
 *          `0`, and store the result in `y`.
 *       2. Leave the switch.
 * 9. Return `-y`.
 *
 * @calls
 * - `__kernel_sin()`
 * - `floor()`
 * - `__kernel_cos()`
 */
static
double sin_pi(double x)
{
	double y,z;
	int n,ix;

	GET_HIGH_WORD(ix,x);
	ix &= 0x7fffffff;

	if(ix<0x3fd00000) return __kernel_sin(pi*x,zero,0);
	y = -x;		/* x is assume negative */

    /*
     * argument reduction, make sure inexact flag not raised if input
     * is an integer
     */
	z = floor(y);
	if(z!=y) {				/* inexact anyway */
	    y  *= 0.5;
	    y   = 2.0*(y - floor(y));		/* y = |x| mod 2.0 */
	    n   = (int) (y*4.0);
	} else {
            if(ix>=0x43400000) {
                y = zero; n = 0;                 /* y must be even */
            } else {
                if(ix<0x43300000) z = y+two52;	/* exact */
		GET_LOW_WORD(n,z);
		n &= 1;
                y  = n;
                n<<= 2;
            }
        }
	switch (n) {
	    case 0:   y =  __kernel_sin(pi*y,zero,0); break;
	    case 1:
	    case 2:   y =  __kernel_cos(pi*(0.5-y),zero); break;
	    case 3:
	    case 4:   y =  __kernel_sin(pi*(one-y),zero,0); break;
	    case 5:
	    case 6:   y = -__kernel_cos(pi*(y-1.5),zero); break;
	    default:  y =  __kernel_sin(pi*(y-2.0),zero,0); break;
	    }
	return -y;
}


/**
 * @funcname `__ieee754_lgamma_r`
 *
 * @description
 * Computes the logarithm of the absolute value of the gamma function of x: the
 * IEEE 754 core of lgamma_r().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `signgamp` | `int *` | [out] Where the sign of gamma(x) is stored. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a0` | `const double` | A constant of the algorithm |
 * | `a1` | `const double` | A constant of the algorithm |
 * | `a10` | `const double` | A constant of the algorithm |
 * | `a11` | `const double` | A constant of the algorithm |
 * | `a2` | `const double` | A constant of the algorithm |
 * | `a3` | `const double` | A constant of the algorithm |
 * | `a4` | `const double` | A constant of the algorithm |
 * | `a5` | `const double` | A constant of the algorithm |
 * | `a6` | `const double` | A constant of the algorithm |
 * | `a7` | `const double` | A constant of the algorithm |
 * | `a8` | `const double` | A constant of the algorithm |
 * | `a9` | `const double` | A constant of the algorithm |
 * | `half` | `const double` | A constant of the algorithm |
 * | `one` | `const double` | A constant of the algorithm |
 * | `pi` | `const double` | A constant of the algorithm |
 * | `r1` | `const double` | A constant of the algorithm |
 * | `r2` | `const double` | A constant of the algorithm |
 * | `r3` | `const double` | A constant of the algorithm |
 * | `r4` | `const double` | A constant of the algorithm |
 * | `r5` | `const double` | A constant of the algorithm |
 * | `r6` | `const double` | A constant of the algorithm |
 * | `s0` | `const double` | A constant of the algorithm |
 * | `s1` | `const double` | A constant of the algorithm |
 * | `s2` | `const double` | A constant of the algorithm |
 * | `s3` | `const double` | A constant of the algorithm |
 * | `s4` | `const double` | A constant of the algorithm |
 * | `s5` | `const double` | A constant of the algorithm |
 * | `s6` | `const double` | A constant of the algorithm |
 * | `t0` | `const double` | A constant of the algorithm |
 * | `t1` | `const double` | A constant of the algorithm |
 * | `t10` | `const double` | A constant of the algorithm |
 * | `t11` | `const double` | A constant of the algorithm |
 * | `t12` | `const double` | A constant of the algorithm |
 * | `t13` | `const double` | A constant of the algorithm |
 * | `t14` | `const double` | A constant of the algorithm |
 * | `t2` | `const double` | A constant of the algorithm |
 * | `t3` | `const double` | A constant of the algorithm |
 * | `t4` | `const double` | A constant of the algorithm |
 * | `t5` | `const double` | A constant of the algorithm |
 * | `t6` | `const double` | A constant of the algorithm |
 * | `t7` | `const double` | A constant of the algorithm |
 * | `t8` | `const double` | A constant of the algorithm |
 * | `t9` | `const double` | A constant of the algorithm |
 * | `tc` | `const double` | A constant of the algorithm |
 * | `tf` | `const double` | A constant of the algorithm |
 * | `tt` | `const double` | A constant of the algorithm |
 * | `u0` | `const double` | A constant of the algorithm |
 * | `u1` | `const double` | A constant of the algorithm |
 * | `u2` | `const double` | A constant of the algorithm |
 * | `u3` | `const double` | A constant of the algorithm |
 * | `u4` | `const double` | A constant of the algorithm |
 * | `u5` | `const double` | A constant of the algorithm |
 * | `v1` | `const double` | A constant of the algorithm |
 * | `v2` | `const double` | A constant of the algorithm |
 * | `v3` | `const double` | A constant of the algorithm |
 * | `v4` | `const double` | A constant of the algorithm |
 * | `v5` | `const double` | A constant of the algorithm |
 * | `w0` | `const double` | A constant of the algorithm |
 * | `w1` | `const double` | A constant of the algorithm |
 * | `w2` | `const double` | A constant of the algorithm |
 * | `w3` | `const double` | A constant of the algorithm |
 * | `w4` | `const double` | A constant of the algorithm |
 * | `w5` | `const double` | A constant of the algorithm |
 * | `w6` | `const double` | A constant of the algorithm |
 * | `zero` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int` | The high word of x |
 * | `i` | `int` | A temporary of the computation |
 * | `ix` | `int` | The high word of x without the sign bit |
 * | `lx` | `int` | The low word of x |
 * | `nadj` | `double` | A temporary of the computation |
 * | `p` | `double` | A temporary of the computation |
 * | `p1` | `double` | A temporary of the computation |
 * | `p2` | `double` | A temporary of the computation |
 * | `p3` | `double` | A temporary of the computation |
 * | `q` | `double` | A temporary of the computation |
 * | `r` | `double` | A temporary of the computation |
 * | `t` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 * | `y` | `double` | A temporary of the computation |
 * | `z` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `nadj` to `0`.
 * 2. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 3. Set `*signgamp` to `1`.
 * 4. Set `ix` to `hx&0x7fffffff`.
 * 5. If `ix` is greater than or equal to `0x7ff00000`:
 *    1. Return `x*x`.
 * 6. If `(ix|lx)` is equal to `0`:
 *    1. Return `one/zero`.
 * 7. If `ix` is less than `0x3b900000`:
 *    1. If `hx` is less than `0`:
 *       1. Set `*signgamp` to `-1`.
 *       2. Return `-__ieee754_log(-x)`.
 *    2. Otherwise:
 *       1. Return `-__ieee754_log(x)`.
 * 8. If `hx` is less than `0`:
 *    1. If `ix` is greater than or equal to `0x43300000`:
 *       1. Return `one/zero`.
 *    2. Call `sin_pi()` with the argument `x`, and store the result in `t`.
 *    3. If `t` is equal to `zero`:
 *       1. Return `one/zero`.
 *    4. Call `__ieee754_log()` with the argument `pi/fabs(t*x)`, and store the
 *       result in `nadj`.
 *    5. If `t` is less than `zero`:
 *       1. Set `*signgamp` to `-1`.
 *    6. Set `x` to `-x`.
 * 9. If `((ix-0x3ff00000)|lx)` is equal to `0` or `((ix-0x40000000)|lx)` is
 *    equal to `0`:
 *    1. Set `r` to `0`.
 * 10. Otherwise, if `ix` is less than `0x40000000`:
 *     1. If `ix` is less than or equal to `0x3feccccc`:
 *        1. Set `r` to `-__ieee754_log(x)`.
 *        2. If `ix` is greater than or equal to `0x3FE76944`:
 *           1. Set `y` to `one-x`.
 *           2. Set `i` to `0`.
 *        3. Otherwise, if `ix` is greater than or equal to `0x3FCDA661`:
 *           1. Set `y` to `x-(tc-one)`.
 *           2. Set `i` to `1`.
 *        4. Otherwise:
 *           1. Set `y` to `x`.
 *           2. Set `i` to `2`.
 *     2. Otherwise:
 *        1. Set `r` to `zero`.
 *        2. If `ix` is greater than or equal to `0x3FFBB4C3`:
 *           1. Set `y` to `2.0-x`.
 *           2. Set `i` to `0`.
 *        3. Otherwise, if `ix` is greater than or equal to `0x3FF3B4C4`:
 *           1. Set `y` to `x-tc`.
 *           2. Set `i` to `1`.
 *        4. Otherwise:
 *           1. Set `y` to `x-one`.
 *           2. Set `i` to `2`.
 *     3. Depending on `i`:
 *        1. Case `0`:
 *           1. Set `z` to `y*y`.
 *           2. Set `p1` to `a0+z*(a2+z*(a4+z*(a6+z*(a8+z*a10))))`.
 *           3. Set `p2` to `z*(a1+z*(a3+z*(a5+z*(a7+z*(a9+z*a11)))))`.
 *           4. Set `p` to `y*p1+p2`.
 *           5. Add `(p-0.5*y)` to `r`.
 *           6. Leave the switch.
 *        2. Case `1`:
 *           1. Set `z` to `y*y`.
 *           2. Set `w` to `z*y`.
 *           3. Set `p1` to `t0+w*(t3+w*(t6+w*(t9 +w*t12)))`.
 *           4. Set `p2` to `t1+w*(t4+w*(t7+w*(t10+w*t13)))`.
 *           5. Set `p3` to `t2+w*(t5+w*(t8+w*(t11+w*t14)))`.
 *           6. Set `p` to `z*p1-(tt-w*(p2+y*p3))`.
 *           7. Add `(tf + p)` to `r`.
 *           8. Leave the switch.
 *        3. Case `2`:
 *           1. Set `p1` to `y*(u0+y*(u1+y*(u2+y*(u3+y*(u4+y*u5)))))`.
 *           2. Set `p2` to `one+y*(v1+y*(v2+y*(v3+y*(v4+y*v5))))`.
 *           3. Add `(-0.5*y + p1/p2)` to `r`.
 * 11. Otherwise, if `ix` is less than `0x40200000`:
 *     1. Set `i` to `(int)x`.
 *     2. Set `t` to `zero`.
 *     3. Set `y` to `x-(double)i`.
 *     4. Set `p` to `y*(s0+y*(s1+y*(s2+y*(s3+y*(s4+y*(s5+y*s6))))))`.
 *     5. Set `q` to `one+y*(r1+y*(r2+y*(r3+y*(r4+y*(r5+y*r6)))))`.
 *     6. Set `r` to `half*y+p/q`.
 *     7. Set `z` to `one`.
 *     8. Depending on `i`:
 *        1. Case `7`:
 *           1. Multiply `z` by `(y+6.0)`.
 *        2. Case `6`:
 *           1. Multiply `z` by `(y+5.0)`.
 *        3. Case `5`:
 *           1. Multiply `z` by `(y+4.0)`.
 *        4. Case `4`:
 *           1. Multiply `z` by `(y+3.0)`.
 *        5. Case `3`:
 *           1. Multiply `z` by `(y+2.0)`.
 *           2. Add `__ieee754_log(z)` to `r`.
 *           3. Leave the switch.
 * 12. Otherwise, if `ix` is less than `0x43900000`:
 *     1. Call `__ieee754_log()` with the argument `x`, and store the result in
 *        `t`.
 *     2. Set `z` to `one/x`.
 *     3. Set `y` to `z*z`.
 *     4. Set `w` to `w0+z*(w1+y*(w2+y*(w3+y*(w4+y*(w5+y*w6)))))`.
 *     5. Set `r` to `(x-half)*(t-one)+w`.
 * 13. Otherwise:
 *     1. Set `r` to `x*(__ieee754_log(x)-one)`.
 * 14. If `hx` is less than `0`:
 *     1. Set `r` to `nadj - r`.
 * 15. Return `r`.
 *
 * @calls
 * - `__ieee754_log()`
 * - `sin_pi()`
 * - `fabs()`
 */
double
__ieee754_lgamma_r(double x, int *signgamp)
{
	double t,y,z,nadj,p,p1,p2,p3,q,r,w;
	int i,hx,lx,ix;

	nadj = 0;
	EXTRACT_WORDS(hx,lx,x);

    /* purge off +-inf, NaN, +-0, and negative arguments */
	*signgamp = 1;
	ix = hx&0x7fffffff;
	if(ix>=0x7ff00000) return x*x;
	if((ix|lx)==0) return one/zero;
	if(ix<0x3b900000) {	/* |x|<2**-70, return -log(|x|) */
	    if(hx<0) {
	        *signgamp = -1;
	        return -__ieee754_log(-x);
	    } else return -__ieee754_log(x);
	}
	if(hx<0) {
	    if(ix>=0x43300000) 	/* |x|>=2**52, must be -integer */
		return one/zero;
	    t = sin_pi(x);
	    if(t==zero) return one/zero; /* -integer */
	    nadj = __ieee754_log(pi/fabs(t*x));
	    if(t<zero) *signgamp = -1;
	    x = -x;
	}

    /* purge off 1 and 2 */
	if((((ix-0x3ff00000)|lx)==0)||(((ix-0x40000000)|lx)==0)) r = 0;
    /* for x < 2.0 */
	else if(ix<0x40000000) {
	    if(ix<=0x3feccccc) { 	/* lgamma(x) = lgamma(x+1)-log(x) */
		r = -__ieee754_log(x);
		if(ix>=0x3FE76944) {y = one-x; i= 0;}
		else if(ix>=0x3FCDA661) {y= x-(tc-one); i=1;}
	  	else {y = x; i=2;}
	    } else {
	  	r = zero;
	        if(ix>=0x3FFBB4C3) {y=2.0-x;i=0;} /* [1.7316,2] */
	        else if(ix>=0x3FF3B4C4) {y=x-tc;i=1;} /* [1.23,1.73] */
		else {y=x-one;i=2;}
	    }
	    switch(i) {
	      case 0:
		z = y*y;
		p1 = a0+z*(a2+z*(a4+z*(a6+z*(a8+z*a10))));
		p2 = z*(a1+z*(a3+z*(a5+z*(a7+z*(a9+z*a11)))));
		p  = y*p1+p2;
		r  += (p-0.5*y); break;
	      case 1:
		z = y*y;
		w = z*y;
		p1 = t0+w*(t3+w*(t6+w*(t9 +w*t12)));	/* parallel comp */
		p2 = t1+w*(t4+w*(t7+w*(t10+w*t13)));
		p3 = t2+w*(t5+w*(t8+w*(t11+w*t14)));
		p  = z*p1-(tt-w*(p2+y*p3));
		r += (tf + p); break;
	      case 2:
		p1 = y*(u0+y*(u1+y*(u2+y*(u3+y*(u4+y*u5)))));
		p2 = one+y*(v1+y*(v2+y*(v3+y*(v4+y*v5))));
		r += (-0.5*y + p1/p2);
	    }
	}
	else if(ix<0x40200000) { 			/* x < 8.0 */
	    i = (int)x;
	    t = zero;
	    y = x-(double)i;
	    p = y*(s0+y*(s1+y*(s2+y*(s3+y*(s4+y*(s5+y*s6))))));
	    q = one+y*(r1+y*(r2+y*(r3+y*(r4+y*(r5+y*r6)))));
	    r = half*y+p/q;
	    z = one;	/* lgamma(1+s) = log(s) + lgamma(s) */
	    switch(i) {
	    case 7: z *= (y+6.0);	/* FALLTHRU */
	    case 6: z *= (y+5.0);	/* FALLTHRU */
	    case 5: z *= (y+4.0);	/* FALLTHRU */
	    case 4: z *= (y+3.0);	/* FALLTHRU */
	    case 3: z *= (y+2.0);	/* FALLTHRU */
		    r += __ieee754_log(z); break;
	    }
    /* 8.0 <= x < 2**58 */
	} else if (ix < 0x43900000) {
	    t = __ieee754_log(x);
	    z = one/x;
	    y = z*z;
	    w = w0+z*(w1+y*(w2+y*(w3+y*(w4+y*(w5+y*w6)))));
	    r = (x-half)*(t-one)+w;
	} else
    /* 2**58 <= x <= inf */
	    r =  x*(__ieee754_log(x)-one);
	if(hx<0) r = nadj - r;
	return r;
}
#endif

