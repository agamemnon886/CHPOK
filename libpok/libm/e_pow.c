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

/* @(#)e_pow.c 5.1 93/09/24 */
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
 * `__ieee754_pow()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
/* __ieee754_pow(x,y) return x**y
 *
 *		      n
 * Method:  Let x =  2   * (1+f)
 *	1. Compute and return log2(x) in two pieces:
 *		log2(x) = w1 + w2,
 *	   where w1 has 53-24 = 29 bit trailing zeros.
 *	2. Perform y*log2(x) = n+y' by simulating multi-precision
 *	   arithmetic, where |y'|<=0.5.
 *	3. Return x**y = 2**n*exp(y'*log2)
 *
 * Special cases:
 *	1.  (anything) ** 0  is 1
 *	2.  (anything) ** 1  is itself
 *	3.  (anything) ** NAN is NAN
 *	4.  NAN ** (anything except 0) is NAN
 *	5.  +-(|x| > 1) **  +INF is +INF
 *	6.  +-(|x| > 1) **  -INF is +0
 *	7.  +-(|x| < 1) **  +INF is +0
 *	8.  +-(|x| < 1) **  -INF is +INF
 *	9.  +-1         ** +-INF is NAN
 *	10. +0 ** (+anything except 0, NAN)               is +0
 *	11. -0 ** (+anything except 0, NAN, odd integer)  is +0
 *	12. +0 ** (-anything except 0, NAN)               is +INF
 *	13. -0 ** (-anything except 0, NAN, odd integer)  is +INF
 *	14. -0 ** (odd integer) = -( +0 ** (odd integer) )
 *	15. +INF ** (+anything except 0,NAN) is +INF
 *	16. +INF ** (-anything except 0,NAN) is +0
 *	17. -INF ** (anything)  = -0 ** (-anything)
 *	18. (-anything) ** (integer) is (-1)**(integer)*(+anything**integer)
 *	19. (-anything except 0 and inf) ** (non-integer) is NAN
 *
 * Accuracy:
 *	pow(x,y) returns x**y nearly rounded. In particular
 *			pow(integer,integer)
 *	always returns the correct integer provided it is
 *	representable.
 *
 * Constants :
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const double
bp[] = {1.0, 1.5,},
dp_h[] = { 0.0, 5.84962487220764160156e-01,}, /* 0x3FE2B803, 0x40000000 */
dp_l[] = { 0.0, 1.35003920212974897128e-08,}, /* 0x3E4CFDEB, 0x43CFD006 */
zero    =  0.0,
one	=  1.0,
two	=  2.0,
two53	=  9007199254740992.0,	/* 0x43400000, 0x00000000 */
huge	=  1.0e300,
tiny    =  1.0e-300,
	/* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
L1  =  5.99999999999994648725e-01, /* 0x3FE33333, 0x33333303 */
L2  =  4.28571428578550184252e-01, /* 0x3FDB6DB6, 0xDB6FABFF */
L3  =  3.33333329818377432918e-01, /* 0x3FD55555, 0x518F264D */
L4  =  2.72728123808534006489e-01, /* 0x3FD17460, 0xA91D4101 */
L5  =  2.30660745775561754067e-01, /* 0x3FCD864A, 0x93C9DB65 */
L6  =  2.06975017800338417784e-01, /* 0x3FCA7E28, 0x4A454EEF */
P1   =  1.66666666666666019037e-01, /* 0x3FC55555, 0x5555553E */
P2   = -2.77777777770155933842e-03, /* 0xBF66C16C, 0x16BEBD93 */
P3   =  6.61375632143793436117e-05, /* 0x3F11566A, 0xAF25DE2C */
P4   = -1.65339022054652515390e-06, /* 0xBEBBBD41, 0xC5D26BF1 */
P5   =  4.13813679705723846039e-08, /* 0x3E663769, 0x72BEA4D0 */
lg2  =  6.93147180559945286227e-01, /* 0x3FE62E42, 0xFEFA39EF */
lg2_h  =  6.93147182464599609375e-01, /* 0x3FE62E43, 0x00000000 */
lg2_l  = -1.90465429995776804525e-09, /* 0xBE205C61, 0x0CA86C39 */
ovt =  8.0085662595372944372e-0017, /* -(1024-log2(ovfl+.5ulp)) */
cp    =  9.61796693925975554329e-01, /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
cp_h  =  9.61796700954437255859e-01, /* 0x3FEEC709, 0xE0000000 =(float)cp */
cp_l  = -7.02846165095275826516e-09, /* 0xBE3E2FE0, 0x145B01F5 =tail of cp_h*/
ivln2    =  1.44269504088896338700e+00, /* 0x3FF71547, 0x652B82FE =1/ln2 */
ivln2_h  =  1.44269502162933349609e+00, /* 0x3FF71547, 0x60000000 =24b 1/ln2*/
ivln2_l  =  1.92596299112661746887e-08; /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail*/

/**
 * @funcname `__ieee754_pow`
 *
 * @description
 * Computes x raised to the power y: the IEEE 754 core of pow().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `y` | `double` | [in] The second argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `L1` | `const double` | A constant of the algorithm |
 * | `L2` | `const double` | A constant of the algorithm |
 * | `L3` | `const double` | A constant of the algorithm |
 * | `L4` | `const double` | A constant of the algorithm |
 * | `L5` | `const double` | A constant of the algorithm |
 * | `L6` | `const double` | A constant of the algorithm |
 * | `P1` | `const double` | A constant of the algorithm |
 * | `P2` | `const double` | A constant of the algorithm |
 * | `P3` | `const double` | A constant of the algorithm |
 * | `P4` | `const double` | A constant of the algorithm |
 * | `P5` | `const double` | A constant of the algorithm |
 * | `bp` | `const double[2]` | A constant of the algorithm |
 * | `cp` | `const double` | A constant of the algorithm |
 * | `cp_h` | `const double` | A constant of the algorithm |
 * | `cp_l` | `const double` | A constant of the algorithm |
 * | `dp_h` | `const double[2]` | A constant of the algorithm |
 * | `dp_l` | `const double[2]` | A constant of the algorithm |
 * | `huge` | `const double` | A constant of the algorithm |
 * | `ivln2` | `const double` | A constant of the algorithm |
 * | `ivln2_h` | `const double` | A constant of the algorithm |
 * | `ivln2_l` | `const double` | A constant of the algorithm |
 * | `lg2` | `const double` | A constant of the algorithm |
 * | `lg2_h` | `const double` | A constant of the algorithm |
 * | `lg2_l` | `const double` | A constant of the algorithm |
 * | `one` | `const double` | A constant of the algorithm |
 * | `ovt` | `const double` | A constant of the algorithm |
 * | `tiny` | `const double` | A constant of the algorithm |
 * | `two` | `const double` | A constant of the algorithm |
 * | `two53` | `const double` | A constant of the algorithm |
 * | `zero` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ax` | `double` | A temporary of the computation |
 * | `hx` | `int32_t` | The high word of x |
 * | `hy` | `int32_t` | The high word of y |
 * | `i` | `int32_t` | The low word of z |
 * | `ix` | `int32_t` | The high word of ax |
 * | `iy` | `int32_t` | The high word of y without the sign bit |
 * | `j` | `int32_t` | The high word of z |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `lx` | `uint32_t` | The low word of x |
 * | `ly` | `uint32_t` | The low word of y |
 * | `n` | `int32_t` | A temporary of the computation |
 * | `p_h` | `double` | A temporary of the computation |
 * | `p_l` | `double` | A temporary of the computation |
 * | `r` | `double` | A temporary of the computation |
 * | `s` | `double` | A temporary of the computation |
 * | `s2` | `double` | A temporary of the computation |
 * | `s_h` | `double` | A temporary of the computation |
 * | `s_l` | `double` | A temporary of the computation |
 * | `ss` | `double` | A temporary of the computation |
 * | `t` | `double` | A temporary of the computation |
 * | `t1` | `double` | A temporary of the computation |
 * | `t2` | `double` | A temporary of the computation |
 * | `t_h` | `double` | A temporary of the computation |
 * | `t_l` | `double` | A temporary of the computation |
 * | `u` | `double` | A temporary of the computation |
 * | `v` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 * | `yisint` | `int32_t` | A temporary of the computation |
 * | `yy1` | `double` | A temporary of the computation |
 * | `z` | `double` | A temporary of the computation |
 * | `z_h` | `double` | A temporary of the computation |
 * | `z_l` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 2. Use the macro `EXTRACT_WORDS()` with the arguments `hy`, `ly` and `y`.
 * 3. Set `ix` to `hx&0x7fffffff`.
 * 4. Set `iy` to `hy&0x7fffffff`.
 * 5. If `(iy|ly)` is equal to `0`:
 *    1. Return `one`.
 * 6. If `ix` is greater than `0x7ff00000` or (`ix` is equal to `0x7ff00000`
 *    and `lx` is not equal to `0`) or `iy` is greater than `0x7ff00000` or
 *    (`iy` is equal to `0x7ff00000` and `ly` is not equal to `0`):
 *    1. Return `x+y`.
 * 7. Set `yisint` to `0`.
 * 8. If `hx` is less than `0`:
 *    1. If `iy` is greater than or equal to `0x43400000`:
 *       1. Set `yisint` to `2`.
 *    2. Otherwise, if `iy` is greater than or equal to `0x3ff00000`:
 *       1. Set `k` to `(iy>>20)-0x3ff`.
 *       2. If `k` is greater than `20`:
 *          1. Set `j` to `ly>>(52-k)`.
 *          2. If `(uint32_t)(j<<(52-k))` is equal to `ly`:
 *             1. Set `yisint` to `2-(j&1)`.
 *       3. Otherwise, if `ly` is equal to `0`:
 *          1. Set `j` to `iy>>(20-k)`.
 *          2. If `(j<<(20-k))` is equal to `iy`:
 *             1. Set `yisint` to `2-(j&1)`.
 * 9. If `ly` is equal to `0`:
 *    1. If `iy` is equal to `0x7ff00000`:
 *       1. If `((ix-0x3ff00000)|lx)` is equal to `0`:
 *          1. Return `y - y`.
 *       2. Otherwise, if `ix` is greater than or equal to `0x3ff00000`:
 *          1. Return `y` if `hy` is greater than or equal to `0`, otherwise
 *             `zero`.
 *       3. Otherwise:
 *          1. Return `-y` if `hy` is less than `0`, otherwise `zero`.
 *    2. If `iy` is equal to `0x3ff00000`:
 *       1. If `hy` is less than `0`:
 *          1. Return `one/x`.
 *       2. Otherwise:
 *          1. Return `x`.
 *    3. If `hy` is equal to `0x40000000`:
 *       1. Return `x*x`.
 *    4. If `hy` is equal to `0x3fe00000`:
 *       1. If `hx` is greater than or equal to `0`:
 *          1. Call `__ieee754_sqrt()` with the argument `x`, and return its
 *             result.
 * 10. Call `fabs()` with the argument `x`, and store the result in `ax`.
 * 11. If `lx` is equal to `0`:
 *     1. If `ix` is equal to `0x7ff00000` or `ix` is equal to `0` or `ix` is
 *        equal to `0x3ff00000`:
 *        1. Set `z` to `ax`.
 *        2. If `hy` is less than `0`:
 *           1. Set `z` to `one/z`.
 *        3. If `hx` is less than `0`:
 *           1. If `((ix-0x3ff00000)|yisint)` is equal to `0`:
 *              1. Set `z` to `(z-z)/(z-z)`.
 *           2. Otherwise, if `yisint` is equal to `1`:
 *              1. Set `z` to `-z`.
 *        4. Return `z`.
 * 12. Set `n` to `(hx>>31)+1`.
 * 13. If `(n|yisint)` is equal to `0`:
 *     1. Return `(x-x)/(x-x)`.
 * 14. Set `s` to `one`.
 * 15. If `(n|(yisint-1))` is equal to `0`:
 *     1. Set `s` to `-one`.
 * 16. If `iy` is greater than `0x41e00000`:
 *     1. If `iy` is greater than `0x43f00000`:
 *        1. If `ix` is less than or equal to `0x3fefffff`:
 *           1. Return `huge*huge` if `hy` is less than `0`, otherwise
 *              `tiny*tiny`.
 *        2. If `ix` is greater than or equal to `0x3ff00000`:
 *           1. Return `huge*huge` if `hy` is greater than `0`, otherwise
 *              `tiny*tiny`.
 *     2. If `ix` is less than `0x3fefffff`:
 *        1. Return `s*huge*huge` if `hy` is less than `0`, otherwise
 *           `s*tiny*tiny`.
 *     3. If `ix` is greater than `0x3ff00000`:
 *        1. Return `s*huge*huge` if `hy` is greater than `0`, otherwise
 *           `s*tiny*tiny`.
 *     4. Set `t` to `ax-one`.
 *     5. Set `w` to `(t*t)*(0.5-t*(0.3333333333333333333333-t*0.25))`.
 *     6. Set `u` to `ivln2_h*t`.
 *     7. Set `v` to `t*ivln2_l-w*ivln2`.
 *     8. Set `t1` to `u+v`.
 *     9. Use the macro `SET_LOW_WORD()` with the arguments `t1` and `0`.
 *     10. Set `t2` to `v-(t1-u)`.
 * 17. Otherwise:
 *     1. Set `n` to `0`.
 *     2. If `ix` is less than `0x00100000`:
 *        1. Multiply `ax` by `two53`.
 *        2. Subtract `53` from `n`.
 *        3. Use the macro `GET_HIGH_WORD()` with the arguments `ix` and `ax`.
 *     3. Add `((ix)>>20)-0x3ff` to `n`.
 *     4. Set `j` to `ix&0x000fffff`.
 *     5. Set `ix` to `j|0x3ff00000`.
 *     6. If `j` is less than or equal to `0x3988E`:
 *        1. Set `k` to `0`.
 *     7. Otherwise, if `j` is less than `0xBB67A`:
 *        1. Set `k` to `1`.
 *     8. Otherwise:
 *        1. Set `k` to `0`.
 *        2. Add `1` to `n`.
 *        3. Subtract `0x00100000` from `ix`.
 *     9. Use the macro `SET_HIGH_WORD()` with the arguments `ax` and `ix`.
 *     10. Set `u` to `ax-bp[k]`.
 *     11. Set `v` to `one/(ax+bp[k])`.
 *     12. Set `ss` to `u*v`.
 *     13. Set `s_h` to `ss`.
 *     14. Use the macro `SET_LOW_WORD()` with the arguments `s_h` and `0`.
 *     15. Set `t_h` to `zero`.
 *     16. Use the macro `SET_HIGH_WORD()` with the arguments `t_h` and
 *         `((ix>>1)|0x20000000)+0x00080000+(k<<18)`.
 *     17. Set `t_l` to `ax - (t_h-bp[k])`.
 *     18. Set `s_l` to `v*((u-s_h*t_h)-s_h*t_l)`.
 *     19. Set `s2` to `ss*ss`.
 *     20. Set `r` to `s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))))`.
 *     21. Add `s_l*(s_h+ss)` to `r`.
 *     22. Set `s2` to `s_h*s_h`.
 *     23. Set `t_h` to `3.0+s2+r`.
 *     24. Use the macro `SET_LOW_WORD()` with the arguments `t_h` and `0`.
 *     25. Set `t_l` to `r-((t_h-3.0)-s2)`.
 *     26. Set `u` to `s_h*t_h`.
 *     27. Set `v` to `s_l*t_h+t_l*ss`.
 *     28. Set `p_h` to `u+v`.
 *     29. Use the macro `SET_LOW_WORD()` with the arguments `p_h` and `0`.
 *     30. Set `p_l` to `v-(p_h-u)`.
 *     31. Set `z_h` to `cp_h*p_h`.
 *     32. Set `z_l` to `cp_l*p_h+p_l*cp+dp_l[k]`.
 *     33. Set `t` to `(double)n`.
 *     34. Set `t1` to `(((z_h+z_l)+dp_h[k])+t)`.
 *     35. Use the macro `SET_LOW_WORD()` with the arguments `t1` and `0`.
 *     36. Set `t2` to `z_l-(((t1-t)-dp_h[k])-z_h)`.
 * 18. Set `yy1` to `y`.
 * 19. Use the macro `SET_LOW_WORD()` with the arguments `yy1` and `0`.
 * 20. Set `p_l` to `(y-yy1)*t1+y*t2`.
 * 21. Set `p_h` to `yy1*t1`.
 * 22. Set `z` to `p_l+p_h`.
 * 23. Use the macro `EXTRACT_WORDS()` with the arguments `j`, `i` and `z`.
 * 24. If `j` is greater than or equal to `0x40900000`:
 *     1. If `((j-0x40900000)|i)` is not equal to `0`:
 *        1. Return `s*huge*huge`.
 *     2. Otherwise:
 *        1. If `p_l+ovt` is greater than `z-p_h`:
 *           1. Return `s*huge*huge`.
 * 25. Otherwise, if `(j&0x7fffffff)` is greater than or equal to `0x4090cc00`:
 *     1. If `((j-0xc090cc00)|i)` is not equal to `0`:
 *        1. Return `s*tiny*tiny`.
 *     2. Otherwise:
 *        1. If `p_l` is less than or equal to `z-p_h`:
 *           1. Return `s*tiny*tiny`.
 * 26. Set `i` to `j&0x7fffffff`.
 * 27. Set `k` to `(i>>20)-0x3ff`.
 * 28. Set `n` to `0`.
 * 29. If `i` is greater than `0x3fe00000`:
 *     1. Set `n` to `j+(0x00100000>>(k+1))`.
 *     2. Set `k` to `((n&0x7fffffff)>>20)-0x3ff`.
 *     3. Set `t` to `zero`.
 *     4. Use the macro `SET_HIGH_WORD()` with the arguments `t` and
 *        `n&~(0x000fffff>>k)`.
 *     5. Set `n` to `((n&0x000fffff)|0x00100000)>>(20-k)`.
 *     6. If `j` is less than `0`:
 *        1. Set `n` to `-n`.
 *     7. Subtract `t` from `p_h`.
 * 30. Set `t` to `p_l+p_h`.
 * 31. Use the macro `SET_LOW_WORD()` with the arguments `t` and `0`.
 * 32. Set `u` to `t*lg2_h`.
 * 33. Set `v` to `(p_l-(t-p_h))*lg2+t*lg2_l`.
 * 34. Set `z` to `u+v`.
 * 35. Set `w` to `v-(z-u)`.
 * 36. Set `t` to `z*z`.
 * 37. Set `t1` to `z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))))`.
 * 38. Set `r` to `(z*t1)/(t1-two)-(w+z*w)`.
 * 39. Set `z` to `one-(r-z)`.
 * 40. Use the macro `GET_HIGH_WORD()` with the arguments `j` and `z`.
 * 41. Add `(n<<20)` to `j`.
 * 42. If `(j>>20)` is less than or equal to `0`:
 *     1. Call `scalbn()` with the arguments `z` and `n`, and store the result
 *        in `z`.
 * 43. Otherwise:
 *     1. Use the macro `SET_HIGH_WORD()` with the arguments `z` and `j`.
 * 44. Return `s*z`.
 *
 * @calls
 * - `__ieee754_sqrt()`
 * - `fabs()`
 * - `scalbn()`
 */
double
__ieee754_pow(double x, double y)
{
	double z,ax,z_h,z_l,p_h,p_l;
	double yy1,t1,t2,r,s,t,u,v,w;
	int32_t i,j,k,yisint,n;
	int32_t hx,hy,ix,iy;
	uint32_t lx,ly;

	EXTRACT_WORDS(hx,lx,x);
	EXTRACT_WORDS(hy,ly,y);
	ix = hx&0x7fffffff;  iy = hy&0x7fffffff;

    /* y==zero: x**0 = 1 */
	if((iy|ly)==0) return one;

    /* +-NaN return x+y */
	if(ix > 0x7ff00000 || ((ix==0x7ff00000)&&(lx!=0)) ||
	   iy > 0x7ff00000 || ((iy==0x7ff00000)&&(ly!=0)))
		return x+y;

    /* determine if y is an odd int when x < 0
     * yisint = 0	... y is not an integer
     * yisint = 1	... y is an odd int
     * yisint = 2	... y is an even int
     */
	yisint  = 0;
	if(hx<0) {
	    if(iy>=0x43400000) yisint = 2; /* even integer y */
	    else if(iy>=0x3ff00000) {
		k = (iy>>20)-0x3ff;	   /* exponent */
		if(k>20) {
		    j = ly>>(52-k);
		    if((uint32_t)(j<<(52-k))==ly) yisint = 2-(j&1);
		} else if(ly==0) {
		    j = iy>>(20-k);
		    if((j<<(20-k))==iy) yisint = 2-(j&1);
		}
	    }
	}

    /* special value of y */
	if(ly==0) {
	    if (iy==0x7ff00000) {	/* y is +-inf */
	        if(((ix-0x3ff00000)|lx)==0)
		    return  y - y;	/* inf**+-1 is NaN */
	        else if (ix >= 0x3ff00000)/* (|x|>1)**+-inf = inf,0 */
		    return (hy>=0)? y: zero;
	        else			/* (|x|<1)**-,+inf = inf,0 */
		    return (hy<0)?-y: zero;
	    }
	    if(iy==0x3ff00000) {	/* y is  +-1 */
		if(hy<0) return one/x; else return x;
	    }
	    if(hy==0x40000000) return x*x; /* y is  2 */
	    if(hy==0x3fe00000) {	/* y is  0.5 */
		if(hx>=0)	/* x >= +0 */
		return __ieee754_sqrt(x);
	    }
	}

	ax   = fabs(x);
    /* special value of x */
	if(lx==0) {
	    if(ix==0x7ff00000||ix==0||ix==0x3ff00000){
		z = ax;			/*x is +-0,+-inf,+-1*/
		if(hy<0) z = one/z;	/* z = (1/|x|) */
		if(hx<0) {
		    if(((ix-0x3ff00000)|yisint)==0) {
			z = (z-z)/(z-z); /* (-1)**non-int is NaN */
		    } else if(yisint==1)
			z = -z;		/* (x<0)**odd = -(|x|**odd) */
		}
		return z;
	    }
	}

	n = (hx>>31)+1;

    /* (x<0)**(non-int) is NaN */
	if((n|yisint)==0) return (x-x)/(x-x);

	s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
	if((n|(yisint-1))==0) s = -one;/* (-ve)**(odd int) */

    /* |y| is huge */
	if(iy>0x41e00000) { /* if |y| > 2**31 */
	    if(iy>0x43f00000){	/* if |y| > 2**64, must o/uflow */
		if(ix<=0x3fefffff) return (hy<0)? huge*huge:tiny*tiny;
		if(ix>=0x3ff00000) return (hy>0)? huge*huge:tiny*tiny;
	    }
	/* over/underflow if x is not close to one */
	    if(ix<0x3fefffff) return (hy<0)? s*huge*huge:s*tiny*tiny;
	    if(ix>0x3ff00000) return (hy>0)? s*huge*huge:s*tiny*tiny;
	/* now |1-x| is tiny <= 2**-20, suffice to compute
	   log(x) by x-x^2/2+x^3/3-x^4/4 */
	    t = ax-one;		/* t has 20 trailing zeros */
	    w = (t*t)*(0.5-t*(0.3333333333333333333333-t*0.25));
	    u = ivln2_h*t;	/* ivln2_h has 21 sig. bits */
	    v = t*ivln2_l-w*ivln2;
	    t1 = u+v;
	    SET_LOW_WORD(t1,0);
	    t2 = v-(t1-u);
	} else {
	    double ss,s2,s_h,s_l,t_h,t_l;
	    n = 0;
	/* take care subnormal number */
	    if(ix<0x00100000)
		{ax *= two53; n -= 53; GET_HIGH_WORD(ix,ax); }
	    n  += ((ix)>>20)-0x3ff;
	    j  = ix&0x000fffff;
	/* determine interval */
	    ix = j|0x3ff00000;		/* normalize ix */
	    if(j<=0x3988E) k=0;		/* |x|<sqrt(3/2) */
	    else if(j<0xBB67A) k=1;	/* |x|<sqrt(3)   */
	    else {k=0;n+=1;ix -= 0x00100000;}
	    SET_HIGH_WORD(ax,ix);

	/* compute ss = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
	    u = ax-bp[k];		/* bp[0]=1.0, bp[1]=1.5 */
	    v = one/(ax+bp[k]);
	    ss = u*v;
	    s_h = ss;
	    SET_LOW_WORD(s_h,0);
	/* t_h=ax+bp[k] High */
	    t_h = zero;
	    SET_HIGH_WORD(t_h,((ix>>1)|0x20000000)+0x00080000+(k<<18));
	    t_l = ax - (t_h-bp[k]);
	    s_l = v*((u-s_h*t_h)-s_h*t_l);
	/* compute log(ax) */
	    s2 = ss*ss;
	    r = s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))));
	    r += s_l*(s_h+ss);
	    s2  = s_h*s_h;
	    t_h = 3.0+s2+r;
	    SET_LOW_WORD(t_h,0);
	    t_l = r-((t_h-3.0)-s2);
	/* u+v = ss*(1+...) */
	    u = s_h*t_h;
	    v = s_l*t_h+t_l*ss;
	/* 2/(3log2)*(ss+...) */
	    p_h = u+v;
	    SET_LOW_WORD(p_h,0);
	    p_l = v-(p_h-u);
	    z_h = cp_h*p_h;		/* cp_h+cp_l = 2/(3*log2) */
	    z_l = cp_l*p_h+p_l*cp+dp_l[k];
	/* log2(ax) = (ss+..)*2/(3*log2) = n + dp_h + z_h + z_l */
	    t = (double)n;
	    t1 = (((z_h+z_l)+dp_h[k])+t);
	    SET_LOW_WORD(t1,0);
	    t2 = z_l-(((t1-t)-dp_h[k])-z_h);
	}

    /* split up y into yy1+y2 and compute (yy1+y2)*(t1+t2) */
	yy1  = y;
	SET_LOW_WORD(yy1,0);
	p_l = (y-yy1)*t1+y*t2;
	p_h = yy1*t1;
	z = p_l+p_h;
	EXTRACT_WORDS(j,i,z);
	if (j>=0x40900000) {				/* z >= 1024 */
	    if(((j-0x40900000)|i)!=0)			/* if z > 1024 */
		return s*huge*huge;			/* overflow */
	    else {
		if(p_l+ovt>z-p_h) return s*huge*huge;	/* overflow */
	    }
	} else if((j&0x7fffffff)>=0x4090cc00 ) {	/* z <= -1075 */
	    if(((j-0xc090cc00)|i)!=0) 		/* z < -1075 */
		return s*tiny*tiny;		/* underflow */
	    else {
		if(p_l<=z-p_h) return s*tiny*tiny;	/* underflow */
	    }
	}
    /*
     * compute 2**(p_h+p_l)
     */
	i = j&0x7fffffff;
	k = (i>>20)-0x3ff;
	n = 0;
	if(i>0x3fe00000) {		/* if |z| > 0.5, set n = [z+0.5] */
	    n = j+(0x00100000>>(k+1));
	    k = ((n&0x7fffffff)>>20)-0x3ff;	/* new k for n */
	    t = zero;
	    SET_HIGH_WORD(t,n&~(0x000fffff>>k));
	    n = ((n&0x000fffff)|0x00100000)>>(20-k);
	    if(j<0) n = -n;
	    p_h -= t;
	}
	t = p_l+p_h;
	SET_LOW_WORD(t,0);
	u = t*lg2_h;
	v = (p_l-(t-p_h))*lg2+t*lg2_l;
	z = u+v;
	w = v-(z-u);
	t  = z*z;
	t1  = z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
	r  = (z*t1)/(t1-two)-(w+z*w);
	z  = one-(r-z);
	GET_HIGH_WORD(j,z);
	j += (n<<20);
	if((j>>20)<=0) z = scalbn(z,n);	/* subnormal output */
	else SET_HIGH_WORD(z,j);
	return s*z;
}

#endif

