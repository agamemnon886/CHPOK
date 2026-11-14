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

/* e_jnf.c -- float version of e_jn.c.
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
 * `__ieee754_jnf()` and `__ieee754_ynf()` of libpok's math library (fdlibm,
 * from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"
#include <libm.h>

static const float
#if 0
invsqrtpi=  5.6418961287e-01, /* 0x3f106ebb */
#endif
two   =  2.0000000000e+00, /* 0x40000000 */
one   =  1.0000000000e+00; /* 0x3F800000 */

static const float zero  =  0.0000000000e+00;

/**
 * @funcname `__ieee754_jnf`
 *
 * @description
 * Computes the Bessel function of the first kind of order n of x, in float: the
 * IEEE 754 core of jnf().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `int` | [in] The order. |
 * | `x` | `float` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `one` | `const float` | A constant of the algorithm |
 * | `two` | `const float` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a` | `float` | A temporary of the computation |
 * | `b` | `float` | A temporary of the computation |
 * | `di` | `float` | A temporary of the computation |
 * | `h` | `float` | A temporary of the computation |
 * | `hx` | `int32_t` | The bits of x |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ix` | `int32_t` | A temporary of the computation |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `m` | `int32_t` | A temporary of the computation |
 * | `q0` | `float` | A temporary of the computation |
 * | `q1` | `float` | A temporary of the computation |
 * | `sgn` | `int32_t` | A temporary of the computation |
 * | `t` | `float` | A temporary of the computation |
 * | `temp` | `float` | A temporary of the computation |
 * | `tmp` | `float` | A temporary of the computation |
 * | `v` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `0x7fffffff&hx`.
 * 3. If `ix` is greater than `0x7f800000`:
 *    1. Return `x+x`.
 * 4. If `n` is less than `0`:
 *    1. Set `n` to `-n`.
 *    2. Set `x` to `-x`.
 *    3. Toggle the bits `0x80000000` in `hx`.
 * 5. If `n` is equal to `0`:
 *    1. Return `(__ieee754_j0f(x))`.
 * 6. If `n` is equal to `1`:
 *    1. Return `(__ieee754_j1f(x))`.
 * 7. Set `sgn` to `(n&1)&(hx>>31)`.
 * 8. Call `fabsf()` with the argument `x`, and store the result in `x`.
 * 9. If `ix` is equal to `0` or `ix` is greater than or equal to `0x7f800000`:
 *    1. Set `b` to `zero`.
 * 10. Otherwise, if `(float)n` is less than or equal to `x`:
 *     1. Call `__ieee754_j0f()` with the argument `x`, and store the result in
 *        `a`.
 *     2. Call `__ieee754_j1f()` with the argument `x`, and store the result in
 *        `b`.
 *     3. Set `i` to `1`; then while `i` is less than `n`, repeat (after each
 *        pass: increment `i`):
 *        1. Set `temp` to `b`.
 *        2. Set `b` to `b*((float)(i+i)/x) - a`.
 *        3. Set `a` to `temp`.
 * 11. Otherwise:
 *     1. If `ix` is less than `0x30800000`:
 *        1. If `n` is greater than `33`:
 *           1. Set `b` to `zero`.
 *        2. Otherwise:
 *           1. Set `temp` to `x*(float)0.5`.
 *           2. Set `b` to `temp`.
 *           3. Set `a` to `one`. Set `i` to `2`; then while `i` is less than or
 *              equal to `n`, repeat (after each pass: increment `i`):
 *              1. Multiply `a` by `(float)i`.
 *              2. Multiply `b` by `temp`.
 *           4. Set `b` to `b/a`.
 *     2. Otherwise:
 *        1. Set `w` to `(n+n)/(float)x`.
 *        2. Set `h` to `(float)2.0/(float)x`.
 *        3. Set `q0` to `w`.
 *        4. Set `z` to `w+h`.
 *        5. Set `q1` to `w*z - (float)1.0`.
 *        6. Set `k` to `1`.
 *        7. While `q1` is less than `(float)1.0e9`:
 *           1. Add `1` to `k`.
 *           2. Add `h` to `z`.
 *           3. Set `tmp` to `z*q1 - q0`.
 *           4. Set `q0` to `q1`.
 *           5. Set `q1` to `tmp`.
 *        8. Set `m` to `n+n`.
 *        9. Set `t` to `zero`. Set `i` to `2*(n+k)`; then while `i` is greater
 *           than or equal to `m`, repeat (after each pass: subtract `2` from
 *           `i`):
 *           1. Set `t` to `one/(i/x-t)`.
 *        10. Set `a` to `t`.
 *        11. Set `b` to `one`.
 *        12. Set `tmp` to `n`.
 *        13. Set `v` to `two/x`.
 *        14. Set `tmp` to `tmp*__ieee754_logf(fabsf(v*tmp))`.
 *        15. If `tmp` is less than `(float)8.8721679688e+01`:
 *            1. Set `i` to `n-1`. Set `di` to `(float)(i+i)`; then while `i` is
 *               greater than `0`, repeat (after each pass: decrement `i`):
 *               1. Set `temp` to `b`.
 *               2. Multiply `b` by `di`.
 *               3. Set `b` to `b/x - a`.
 *               4. Set `a` to `temp`.
 *               5. Subtract `two` from `di`.
 *        16. Otherwise:
 *            1. Set `i` to `n-1`. Set `di` to `(float)(i+i)`; then while `i` is
 *               greater than `0`, repeat (after each pass: decrement `i`):
 *               1. Set `temp` to `b`.
 *               2. Multiply `b` by `di`.
 *               3. Set `b` to `b/x - a`.
 *               4. Set `a` to `temp`.
 *               5. Subtract `two` from `di`.
 *               6. If `b` is greater than `(float)1e10`:
 *                  1. Divide `a` by `b`.
 *                  2. Divide `t` by `b`.
 *                  3. Set `b` to `one`.
 *        17. Set `b` to `(t*__ieee754_j0f(x)/b)`.
 * 12. If `sgn` is equal to `1`:
 *     1. Return `-b`.
 * 13. Otherwise:
 *     1. Return `b`.
 *
 * @calls
 * - `__ieee754_j0f()`
 * - `__ieee754_j1f()`
 * - `fabsf()`
 * - `__ieee754_logf()`
 */
float
__ieee754_jnf(int n, float x)
{
	int32_t i,hx,ix, sgn;
	float a, b, temp, di;
	float z, w;

    /* J(-n,x) = (-1)^n * J(n, x), J(n, -x) = (-1)^n * J(n, x)
     * Thus, J(-n,x) = J(n,-x)
     */
	GET_FLOAT_WORD(hx,x);
	ix = 0x7fffffff&hx;
    /* if J(n,NaN) is NaN */
	if(ix>0x7f800000) return x+x;
	if(n<0){
		n = -n;
		x = -x;
		hx ^= 0x80000000;
	}
	if(n==0) return(__ieee754_j0f(x));
	if(n==1) return(__ieee754_j1f(x));
	sgn = (n&1)&(hx>>31);	/* even n -- 0, odd n -- sign(x) */
	x = fabsf(x);
	if(ix==0||ix>=0x7f800000) 	/* if x is 0 or inf */
	    b = zero;
	else if((float)n<=x) {
		/* Safe to use J(n+1,x)=2n/x *J(n,x)-J(n-1,x) */
	    a = __ieee754_j0f(x);
	    b = __ieee754_j1f(x);
	    for(i=1;i<n;i++){
		temp = b;
		b = b*((float)(i+i)/x) - a; /* avoid underflow */
		a = temp;
	    }
	} else {
	    if(ix<0x30800000) {	/* x < 2**-29 */
    /* x is tiny, return the first Taylor expansion of J(n,x)
     * J(n,x) = 1/n!*(x/2)^n  - ...
     */
		if(n>33)	/* underflow */
		    b = zero;
		else {
		    temp = x*(float)0.5; b = temp;
		    for (a=one,i=2;i<=n;i++) {
			a *= (float)i;		/* a = n! */
			b *= temp;		/* b = (x/2)^n */
		    }
		    b = b/a;
		}
	    } else {
		/* use backward recurrence */
		/* 			x      x^2      x^2
		 *  J(n,x)/J(n-1,x) =  ----   ------   ------   .....
		 *			2n  - 2(n+1) - 2(n+2)
		 *
		 * 			1      1        1
		 *  (for large x)   =  ----  ------   ------   .....
		 *			2n   2(n+1)   2(n+2)
		 *			-- - ------ - ------ -
		 *			 x     x         x
		 *
		 * Let w = 2n/x and h=2/x, then the above quotient
		 * is equal to the continued fraction:
		 *		    1
		 *	= -----------------------
		 *		       1
		 *	   w - -----------------
		 *			  1
		 * 	        w+h - ---------
		 *		       w+2h - ...
		 *
		 * To determine how many terms needed, let
		 * Q(0) = w, Q(1) = w(w+h) - 1,
		 * Q(k) = (w+k*h)*Q(k-1) - Q(k-2),
		 * When Q(k) > 1e4	good for single
		 * When Q(k) > 1e9	good for double
		 * When Q(k) > 1e17	good for quadruple
		 */
	    /* determine k */
		float t,v;
		float q0,q1,h,tmp; int32_t k,m;
		w  = (n+n)/(float)x; h = (float)2.0/(float)x;
		q0 = w;  z = w+h; q1 = w*z - (float)1.0; k=1;
		while(q1<(float)1.0e9) {
			k += 1; z += h;
			tmp = z*q1 - q0;
			q0 = q1;
			q1 = tmp;
		}
		m = n+n;
		for(t=zero, i = 2*(n+k); i>=m; i -= 2) t = one/(i/x-t);
		a = t;
		b = one;
		/*  estimate log((2/x)^n*n!) = n*log(2/x)+n*ln(n)
		 *  Hence, if n*(log(2n/x)) > ...
		 *  single 8.8722839355e+01
		 *  double 7.09782712893383973096e+02
		 *  long double 1.1356523406294143949491931077970765006170e+04
		 *  then recurrent value may overflow and the result is
		 *  likely underflow to zero
		 */
		tmp = n;
		v = two/x;
		tmp = tmp*__ieee754_logf(fabsf(v*tmp));
		if(tmp<(float)8.8721679688e+01) {
	    	    for(i=n-1,di=(float)(i+i);i>0;i--){
		        temp = b;
			b *= di;
			b  = b/x - a;
		        a = temp;
			di -= two;
	     	    }
		} else {
	    	    for(i=n-1,di=(float)(i+i);i>0;i--){
		        temp = b;
			b *= di;
			b  = b/x - a;
		        a = temp;
			di -= two;
		    /* scale b to avoid spurious overflow */
			if(b>(float)1e10) {
			    a /= b;
			    t /= b;
			    b  = one;
			}
	     	    }
		}
	    	b = (t*__ieee754_j0f(x)/b);
	    }
	}
	if(sgn==1) return -b; else return b;
}

/**
 * @funcname `__ieee754_ynf`
 *
 * @description
 * Computes the Bessel function of the second kind of order n of x, in float:
 * the IEEE 754 core of ynf().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `int` | [in] The order. |
 * | `x` | `float` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `one` | `const float` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a` | `float` | A temporary of the computation |
 * | `b` | `float` | A temporary of the computation |
 * | `hx` | `int32_t` | The bits of x |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ib` | `int32_t` | The bits of b |
 * | `ix` | `int32_t` | A temporary of the computation |
 * | `sign` | `int32_t` | A temporary of the computation |
 * | `temp` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `0x7fffffff&hx`.
 * 3. If `ix` is greater than `0x7f800000`:
 *    1. Return `x+x`.
 * 4. If `ix` is equal to `0`:
 *    1. Return `-one/zero`.
 * 5. If `hx` is less than `0`:
 *    1. Return `zero/zero`.
 * 6. Set `sign` to `1`.
 * 7. If `n` is less than `0`:
 *    1. Set `n` to `-n`.
 *    2. Set `sign` to `1 - ((n&1)<<1)`.
 * 8. If `n` is equal to `0`:
 *    1. Return `(__ieee754_y0f(x))`.
 * 9. If `n` is equal to `1`:
 *    1. Return `(sign*__ieee754_y1f(x))`.
 * 10. If `ix` is equal to `0x7f800000`:
 *     1. Return `zero`.
 * 11. Call `__ieee754_y0f()` with the argument `x`, and store the result in
 *     `a`.
 * 12. Call `__ieee754_y1f()` with the argument `x`, and store the result in
 *     `b`.
 * 13. Use the macro `GET_FLOAT_WORD()` with the arguments `ib` and `b`.
 * 14. Set `i` to `1`; then while `i` is less than `n` and `(uint32_t)ib` is not
 *     equal to `0xff800000`, repeat (after each pass: increment `i`):
 *     1. Set `temp` to `b`.
 *     2. Set `b` to `((float)(i+i)/x)*b - a`.
 *     3. Use the macro `GET_FLOAT_WORD()` with the arguments `ib` and `b`.
 *     4. Set `a` to `temp`.
 * 15. If `sign` is greater than `0`:
 *     1. Return `b`.
 * 16. Otherwise:
 *     1. Return `-b`.
 *
 * @calls
 * - `__ieee754_y0f()`
 * - `__ieee754_y1f()`
 */
float
__ieee754_ynf(int n, float x)
{
	int32_t i,hx,ix,ib;
	int32_t sign;
	float a, b, temp;

	GET_FLOAT_WORD(hx,x);
	ix = 0x7fffffff&hx;
    /* if Y(n,NaN) is NaN */
	if(ix>0x7f800000) return x+x;
	if(ix==0) return -one/zero;
	if(hx<0) return zero/zero;
	sign = 1;
	if(n<0){
		n = -n;
		sign = 1 - ((n&1)<<1);
	}
	if(n==0) return(__ieee754_y0f(x));
	if(n==1) return(sign*__ieee754_y1f(x));
	if(ix==0x7f800000) return zero;

	a = __ieee754_y0f(x);
	b = __ieee754_y1f(x);
	/* quit if b is -inf */
	GET_FLOAT_WORD(ib,b);
	for(i=1;i<n&&(uint32_t)ib!=0xff800000;i++){
	    temp = b;
	    b = ((float)(i+i)/x)*b - a;
	    GET_FLOAT_WORD(ib,b);
	    a = temp;
	}
	if(sign>0) return b; else return -b;
}

#endif

