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
 * Created by julien on Sat Jan 31 20:12:07 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-09: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: Doxygen comments for its macros.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/* @(#)k_tan.c 5.1 93/09/24 */
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
 * `__kernel_tan()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

/* __kernel_tan( x, y, k )
 * kernel tan function on [-pi/4, pi/4], pi/4 ~ 0.7854
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * Input k indicates whether tan (if k=1) or
 * -1/tan (if k= -1) is returned.
 *
 * Algorithm
 *	1. Since tan(-x) = -tan(x), we need only to consider positive x.
 *	2. if x < 2^-28 (hx<0x3e300000 0), return x with inexact if x!=0.
 *	3. tan(x) is approximated by a odd polynomial of degree 27 on
 *	   [0,0.67434]
 *		  	         3             27
 *	   	tan(x) ~ x + T1*x + ... + T13*x
 *	   where
 *
 * 	        |tan(x)         2     4            26   |     -59.2
 * 	        |----- - (1+T1*x +T2*x +.... +T13*x    )| <= 2
 * 	        |  x 					|
 *
 *	   Note: tan(x+y) = tan(x) + tan'(x)*y
 *		          ~ tan(x) + (1+x*x)*y
 *	   Therefore, for better accuracy in computing tan(x+y), let
 *		     3      2      2       2       2
 *		r = x *(T2+x *(T3+x *(...+x *(T12+x *T13))))
 *	   then
 *		 		    3    2
 *		tan(x+y) = x + (T1*x + (x *(r+y)+y))
 *
 *      4. For x in [0.67434,pi/4],  let y = pi/4 - x, then
 *		tan(x) = tan(pi/4-y) = (1-tan(y))/(1+tan(y))
 *		       = 1 - 2*(tan(y) - (tan(y)^2)/(1+tan(y)))
 */

#include <libm.h>
#include "math_private.h"

static const double xxx[] = {
		 3.33333333333334091986e-01,	/* 3FD55555, 55555563 */
		 1.33333333333201242699e-01,	/* 3FC11111, 1110FE7A */
		 5.39682539762260521377e-02,	/* 3FABA1BA, 1BB341FE */
		 2.18694882948595424599e-02,	/* 3F9664F4, 8406D637 */
		 8.86323982359930005737e-03,	/* 3F8226E3, E96E8493 */
		 3.59207910759131235356e-03,	/* 3F6D6D22, C9560328 */
		 1.45620945432529025516e-03,	/* 3F57DBC8, FEE08315 */
		 5.88041240820264096874e-04,	/* 3F4344D8, F2F26501 */
		 2.46463134818469906812e-04,	/* 3F3026F7, 1A8D1068 */
		 7.81794442939557092300e-05,	/* 3F147E88, A03792A6 */
		 7.14072491382608190305e-05,	/* 3F12B80F, 32F0A7E9 */
		-1.85586374855275456654e-05,	/* BEF375CB, DB605373 */
		 2.59073051863633712884e-05,	/* 3EFB2A70, 74BF7AD4 */
/* one */	 1.00000000000000000000e+00,	/* 3FF00000, 00000000 */
/* pio4 */	 7.85398163397448278999e-01,	/* 3FE921FB, 54442D18 */
/* pio4lo */	 3.06161699786838301793e-17	/* 3C81A626, 33145C07 */
};
/**
 * @name Constants of __kernel_tan()
 * The elements of the array xxx of k_tan.c: the coefficients of the polynomial
 * and three constants.
 * @{
 */

/**
 * @macroname `one`
 *
 * @description
 * 1.0 (xxx[13]).
 */
#define	one	xxx[13]

/**
 * @macroname `pio4`
 *
 * @description
 * pi/4 (xxx[14]).
 */
#define	pio4	xxx[14]

/**
 * @macroname `pio4lo`
 *
 * @description
 * The low part of pi/4 (xxx[15]).
 */
#define	pio4lo	xxx[15]

/**
 * @macroname `T`
 *
 * @description
 * The coefficients of the polynomial (xxx, T[0] to T[12]).
 */
#define	T	xxx

/** @} */

/**
 * @funcname `__kernel_tan`
 *
 * @description
 * The kernel tangent of x + y on [-pi/4, pi/4] (y is the tail of x), or -1/tan
 * if iy is -1.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `y` | `double` | [in] The tail of x. |
 * | `iy` | `int` | [in] 1 for the tangent, -1 for -1/tangent (or 0 if y is 0). |
 *
 * @return The tangent, or -1/tangent.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `xxx` | `const double[16]` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a` | `double` | A temporary of the computation |
 * | `hx` | `int32_t` | The high word of x |
 * | `ix` | `int32_t` | The high word of x without the sign bit |
 * | `low` | `uint32_t` | The low word of x |
 * | `r` | `double` | A temporary of the computation |
 * | `s` | `double` | A temporary of the computation |
 * | `t` | `double` | A temporary of the computation |
 * | `v` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 * | `z` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_HIGH_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `hx & 0x7fffffff`.
 * 3. If `ix` is less than `0x3e300000`:
 *    1. If `(int) x` is equal to `0`:
 *       1. Use the macro `GET_LOW_WORD()` with the arguments `low` and `x`.
 *       2. If `((ix | low) | (iy + 1))` is equal to `0`:
 *          1. Return `one / fabs(x)`.
 *       3. Otherwise:
 *          1. If `iy` is equal to `1`:
 *             1. Return `x`.
 *          2. Otherwise:
 *             1. Set `z` to `w = x + y`.
 *             2. Use the macro `SET_LOW_WORD()` with the arguments `z` and
 *                `0`.
 *             3. Set `v` to `y - (z - x)`.
 *             4. Set `t` to `a = -one / w`.
 *             5. Use the macro `SET_LOW_WORD()` with the arguments `t` and
 *                `0`.
 *             6. Set `s` to `one + t * z`.
 *             7. Return `t + a * (s + t * v)`.
 * 4. If `ix` is greater than or equal to `0x3FE59428`:
 *    1. If `hx` is less than `0`:
 *       1. Set `x` to `-x`.
 *       2. Set `y` to `-y`.
 *    2. Set `z` to `pio4 - x`.
 *    3. Set `w` to `pio4lo - y`.
 *    4. Set `x` to `z + w`.
 *    5. Set `y` to `0.0`.
 * 5. Set `z` to `x * x`.
 * 6. Set `w` to `z * z`.
 * 7. Set `r` to
 *    `T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))))`.
 * 8. Set `v` to
 *    `z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w *`
 *    `T[12])))))`.
 * 9. Set `s` to `z * x`.
 * 10. Set `r` to `y + z * (s * (r + v) + y)`.
 * 11. Add `T[0] * s` to `r`.
 * 12. Set `w` to `x + r`.
 * 13. If `ix` is greater than or equal to `0x3FE59428`:
 *     1. Set `v` to `(double) iy`.
 *     2. Return
 *        `(double) (1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) -`
 *        `r)))`.
 * 14. If `iy` is equal to `1`:
 *     1. Return `w`.
 * 15. Otherwise:
 *     1. Set `z` to `w`.
 *     2. Use the macro `SET_LOW_WORD()` with the arguments `z` and `0`.
 *     3. Set `v` to `r - (z - x)`.
 *     4. Set `t` to `a = -1.0 / w`.
 *     5. Use the macro `SET_LOW_WORD()` with the arguments `t` and `0`.
 *     6. Set `s` to `1.0 + t * z`.
 *     7. Return `t + a * (s + t * v)`.
 *
 * @calls
 * - `fabs()`
 */
double
__kernel_tan(double x, double y, int iy)
{
	double z, r, v, w, s;
	int32_t ix, hx;

	GET_HIGH_WORD(hx, x);	/* high word of x */
	ix = hx & 0x7fffffff;			/* high word of |x| */
	if (ix < 0x3e300000) {			/* x < 2**-28 */
		if ((int) x == 0) {		/* generate inexact */
			uint32_t low;
			GET_LOW_WORD(low, x);
			if(((ix | low) | (iy + 1)) == 0)
				return one / fabs(x);
			else {
				if (iy == 1)
					return x;
				else {	/* compute -1 / (x+y) carefully */
					double a, t;

					z = w = x + y;
					SET_LOW_WORD(z, 0);
					v = y - (z - x);
					t = a = -one / w;
					SET_LOW_WORD(t, 0);
					s = one + t * z;
					return t + a * (s + t * v);
				}
			}
		}
	}
	if (ix >= 0x3FE59428) {	/* |x| >= 0.6744 */
		if (hx < 0) {
			x = -x;
			y = -y;
		}
		z = pio4 - x;
		w = pio4lo - y;
		x = z + w;
		y = 0.0;
	}
	z = x * x;
	w = z * z;
	/*
	 * Break x^5*(T[1]+x^2*T[2]+...) into
	 * x^5(T[1]+x^4*T[3]+...+x^20*T[11]) +
	 * x^5(x^2*(T[2]+x^4*T[4]+...+x^22*[T12]))
	 */
	r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] +
		w * T[11]))));
	v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] +
		w * T[12])))));
	s = z * x;
	r = y + z * (s * (r + v) + y);
	r += T[0] * s;
	w = x + r;
	if (ix >= 0x3FE59428) {
		v = (double) iy;
		return (double) (1 - ((hx >> 30) & 2)) *
			(v - 2.0 * (x - (w * w / (w + v) - r)));
	}
	if (iy == 1)
		return w;
	else {
		/*
		 * if allow error up to 2 ulp, simply return
		 * -1.0 / (x+r) here
		 */
		/* compute -1.0 / (x+r) accurately */
		double a, t;
		z = w;
		SET_LOW_WORD(z, 0);
		v = r - (z - x);	/* z+v = r+x */
		t = a = -1.0 / w;	/* a = -1.0/w */
		SET_LOW_WORD(t, 0);
		s = 1.0 + t * z;
		return t + a * (s + t * v);
	}
}

#endif
