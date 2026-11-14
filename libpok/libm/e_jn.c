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

/* @(#)e_jn.c 5.1 93/09/24 */
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
 * `__ieee754_jn()` and `__ieee754_yn()` of libpok's math library (fdlibm, from
 * Sun Microsystems).
 */
/*
 * __ieee754_jn(n, x), __ieee754_yn(n, x)
 * floating point Bessel's function of the 1st and 2nd kind
 * of order n
 *
 * Special cases:
 *	y0(0)=y1(0)=yn(n,0) = -inf with division by zero signal;
 *	y0(-ve)=y1(-ve)=yn(n,-ve) are NaN with invalid signal.
 * Note 2. About jn(n,x), yn(n,x)
 *	For n=0, j0(x) is called,
 *	for n=1, j1(x) is called,
 *	for n<x, forward recursion us used starting
 *	from values of j0(x) and j1(x).
 *	for n>x, a continued fraction approximation to
 *	j(n,x)/j(n-1,x) is evaluated and then backward
 *	recursion is used starting from a supposed value
 *	for j(n,x). The resulting value of j(0,x) is
 *	compared with the actual value to correct the
 *	supposed value of j(n,x).
 *
 *	yn(n,x) is similar in all respects, except
 *	that forward recursion is used for all
 *	values of n>1.
 *
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "namespace.h"
#include "math_private.h"
#include <libm.h>

static const double
invsqrtpi=  5.64189583547756279280e-01, /* 0x3FE20DD7, 0x50429B6D */
two   =  2.00000000000000000000e+00, /* 0x40000000, 0x00000000 */
one   =  1.00000000000000000000e+00; /* 0x3FF00000, 0x00000000 */

static const double zero  =  0.00000000000000000000e+00;

/**
 * @funcname `__ieee754_jn`
 *
 * @description
 * Computes the Bessel function of the first kind of order n of x: the IEEE 754
 * core of jn().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `int` | [in] The order. |
 * | `x` | `double` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `invsqrtpi` | `const double` | A constant of the algorithm |
 * | `one` | `const double` | A constant of the algorithm |
 * | `two` | `const double` | A constant of the algorithm |
 * | `zero` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a` | `double` | A temporary of the computation |
 * | `b` | `double` | A temporary of the computation |
 * | `di` | `double` | A temporary of the computation |
 * | `h` | `double` | A temporary of the computation |
 * | `hx` | `int32_t` | The high word of x |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ix` | `int32_t` | A temporary of the computation |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `lx` | `int32_t` | The low word of x |
 * | `m` | `int32_t` | A temporary of the computation |
 * | `q0` | `double` | A temporary of the computation |
 * | `q1` | `double` | A temporary of the computation |
 * | `sgn` | `int32_t` | A temporary of the computation |
 * | `t` | `double` | A temporary of the computation |
 * | `temp` | `double` | A temporary of the computation |
 * | `tmp` | `double` | A temporary of the computation |
 * | `v` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 * | `z` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `temp` to `0`.
 * 2. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 3. Set `ix` to `0x7fffffff&hx`.
 * 4. If `(ix|((uint32_t)(lx|-lx))>>31)` is greater than `0x7ff00000`:
 *    1. Return `x+x`.
 * 5. If `n` is less than `0`:
 *    1. Set `n` to `-n`.
 *    2. Set `x` to `-x`.
 *    3. Toggle the bits `0x80000000` in `hx`.
 * 6. If `n` is equal to `0`:
 *    1. Return `(__ieee754_j0(x))`.
 * 7. If `n` is equal to `1`:
 *    1. Return `(__ieee754_j1(x))`.
 * 8. Set `sgn` to `(n&1)&(hx>>31)`.
 * 9. Call `fabs()` with the argument `x`, and store the result in `x`.
 * 10. If `(ix|lx)` is equal to `0` or `ix` is greater than or equal to
 *     `0x7ff00000`:
 *     1. Set `b` to `zero`.
 * 11. Otherwise, if `(double)n` is less than or equal to `x`:
 *     1. If `ix` is greater than or equal to `0x52D00000`:
 *        1. Depending on `n&3`:
 *           1. Case `0`:
 *              1. Set `temp` to `cos(x)+sin(x)`.
 *              2. Leave the switch.
 *           2. Case `1`:
 *              1. Set `temp` to `-cos(x)+sin(x)`.
 *              2. Leave the switch.
 *           3. Case `2`:
 *              1. Set `temp` to `-cos(x)-sin(x)`.
 *              2. Leave the switch.
 *           4. Case `3`:
 *              1. Set `temp` to `cos(x)-sin(x)`.
 *              2. Leave the switch.
 *        2. Set `b` to `invsqrtpi*temp/sqrt(x)`.
 *     2. Otherwise:
 *        1. Call `__ieee754_j0()` with the argument `x`, and store the result
 *           in `a`.
 *        2. Call `__ieee754_j1()` with the argument `x`, and store the result
 *           in `b`.
 *        3. Set `i` to `1`; then while `i` is less than `n`, repeat (after each
 *           pass: increment `i`):
 *           1. Set `temp` to `b`.
 *           2. Set `b` to `b*((double)(i+i)/x) - a`.
 *           3. Set `a` to `temp`.
 * 12. Otherwise:
 *     1. If `ix` is less than `0x3e100000`:
 *        1. If `n` is greater than `33`:
 *           1. Set `b` to `zero`.
 *        2. Otherwise:
 *           1. Set `temp` to `x*0.5`.
 *           2. Set `b` to `temp`.
 *           3. Set `a` to `one`. Set `i` to `2`; then while `i` is less than or
 *              equal to `n`, repeat (after each pass: increment `i`):
 *              1. Multiply `a` by `(double)i`.
 *              2. Multiply `b` by `temp`.
 *           4. Set `b` to `b/a`.
 *     2. Otherwise:
 *        1. Set `w` to `(n+n)/(double)x`.
 *        2. Set `h` to `2.0/(double)x`.
 *        3. Set `q0` to `w`.
 *        4. Set `z` to `w+h`.
 *        5. Set `q1` to `w*z - 1.0`.
 *        6. Set `k` to `1`.
 *        7. While `q1` is less than `1.0e9`:
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
 *        14. Set `tmp` to `tmp*__ieee754_log(fabs(v*tmp))`.
 *        15. If `tmp` is less than `7.09782712893383973096e+02`:
 *            1. Set `i` to `n-1`. Set `di` to `(double)(i+i)`; then while `i`
 *               is greater than `0`, repeat (after each pass: decrement `i`):
 *               1. Set `temp` to `b`.
 *               2. Multiply `b` by `di`.
 *               3. Set `b` to `b/x - a`.
 *               4. Set `a` to `temp`.
 *               5. Subtract `two` from `di`.
 *        16. Otherwise:
 *            1. Set `i` to `n-1`. Set `di` to `(double)(i+i)`; then while `i`
 *               is greater than `0`, repeat (after each pass: decrement `i`):
 *               1. Set `temp` to `b`.
 *               2. Multiply `b` by `di`.
 *               3. Set `b` to `b/x - a`.
 *               4. Set `a` to `temp`.
 *               5. Subtract `two` from `di`.
 *               6. If `b` is greater than `1e100`:
 *                  1. Divide `a` by `b`.
 *                  2. Divide `t` by `b`.
 *                  3. Set `b` to `one`.
 *        17. Set `b` to `(t*__ieee754_j0(x)/b)`.
 * 13. If `sgn` is equal to `1`:
 *     1. Return `-b`.
 * 14. Otherwise:
 *     1. Return `b`.
 *
 * @calls
 * - `__ieee754_j0()`
 * - `__ieee754_j1()`
 * - `fabs()`
 * - `cos()`
 * - `sin()`
 * - `sqrt()`
 * - `__ieee754_log()`
 */
double
__ieee754_jn(int n, double x)
{
	int32_t i,hx,ix,lx, sgn;
	double a, b, temp, di;
	double z, w;

	temp = 0;
    /* J(-n,x) = (-1)^n * J(n, x), J(n, -x) = (-1)^n * J(n, x)
     * Thus, J(-n,x) = J(n,-x)
     */
	EXTRACT_WORDS(hx,lx,x);
	ix = 0x7fffffff&hx;
    /* if J(n,NaN) is NaN */
	if((ix|((uint32_t)(lx|-lx))>>31)>0x7ff00000) return x+x;
	if(n<0){
		n = -n;
		x = -x;
		hx ^= 0x80000000;
	}
	if(n==0) return(__ieee754_j0(x));
	if(n==1) return(__ieee754_j1(x));
	sgn = (n&1)&(hx>>31);	/* even n -- 0, odd n -- sign(x) */
	x = fabs(x);
	if((ix|lx)==0||ix>=0x7ff00000) 	/* if x is 0 or inf */
	    b = zero;
	else if((double)n<=x) {
		/* Safe to use J(n+1,x)=2n/x *J(n,x)-J(n-1,x) */
	    if(ix>=0x52D00000) { /* x > 2**302 */
    /* (x >> n**2)
     *	    Jn(x) = cos(x-(2n+1)*pi/4)*sqrt(2/x*pi)
     *	    Yn(x) = sin(x-(2n+1)*pi/4)*sqrt(2/x*pi)
     *	    Let s=sin(x), c=cos(x),
     *		xn=x-(2n+1)*pi/4, sqt2 = sqrt(2),then
     *
     *		   n	sin(xn)*sqt2	cos(xn)*sqt2
     *		----------------------------------
     *		   0	 s-c		 c+s
     *		   1	-s-c 		-c+s
     *		   2	-s+c		-c-s
     *		   3	 s+c		 c-s
     */
		switch(n&3) {
		    case 0: temp =  cos(x)+sin(x); break;
		    case 1: temp = -cos(x)+sin(x); break;
		    case 2: temp = -cos(x)-sin(x); break;
		    case 3: temp =  cos(x)-sin(x); break;
		}
		b = invsqrtpi*temp/sqrt(x);
	    } else {
	        a = __ieee754_j0(x);
	        b = __ieee754_j1(x);
	        for(i=1;i<n;i++){
		    temp = b;
		    b = b*((double)(i+i)/x) - a; /* avoid underflow */
		    a = temp;
	        }
	    }
	} else {
	    if(ix<0x3e100000) {	/* x < 2**-29 */
    /* x is tiny, return the first Taylor expansion of J(n,x)
     * J(n,x) = 1/n!*(x/2)^n  - ...
     */
		if(n>33)	/* underflow */
		    b = zero;
		else {
		    temp = x*0.5; b = temp;
		    for (a=one,i=2;i<=n;i++) {
			a *= (double)i;		/* a = n! */
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
		double t,v;
		double q0,q1,h,tmp; int32_t k,m;
		w  = (n+n)/(double)x; h = 2.0/(double)x;
		q0 = w;  z = w+h; q1 = w*z - 1.0; k=1;
		while(q1<1.0e9) {
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
		tmp = tmp*__ieee754_log(fabs(v*tmp));
		if(tmp<7.09782712893383973096e+02) {
	    	    for(i=n-1,di=(double)(i+i);i>0;i--){
		        temp = b;
			b *= di;
			b  = b/x - a;
		        a = temp;
			di -= two;
	     	    }
		} else {
	    	    for(i=n-1,di=(double)(i+i);i>0;i--){
		        temp = b;
			b *= di;
			b  = b/x - a;
		        a = temp;
			di -= two;
		    /* scale b to avoid spurious overflow */
			if(b>1e100) {
			    a /= b;
			    t /= b;
			    b  = one;
			}
	     	    }
		}
	    	b = (t*__ieee754_j0(x)/b);
	    }
	}
	if(sgn==1) return -b; else return b;
}

/**
 * @funcname `__ieee754_yn`
 *
 * @description
 * Computes the Bessel function of the second kind of order n of x: the IEEE 754
 * core of yn().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `int` | [in] The order. |
 * | `x` | `double` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `invsqrtpi` | `const double` | A constant of the algorithm |
 * | `one` | `const double` | A constant of the algorithm |
 * | `zero` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a` | `double` | A temporary of the computation |
 * | `b` | `double` | A temporary of the computation |
 * | `high` | `uint32_t` | The high word of b |
 * | `hx` | `int32_t` | The high word of x |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ix` | `int32_t` | A temporary of the computation |
 * | `lx` | `int32_t` | The low word of x |
 * | `sign` | `int32_t` | A temporary of the computation |
 * | `temp` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `temp` to `0`.
 * 2. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 3. Set `ix` to `0x7fffffff&hx`.
 * 4. If `(ix|((uint32_t)(lx|-lx))>>31)` is greater than `0x7ff00000`:
 *    1. Return `x+x`.
 * 5. If `(ix|lx)` is equal to `0`:
 *    1. Return `-one/zero`.
 * 6. If `hx` is less than `0`:
 *    1. Return `zero/zero`.
 * 7. Set `sign` to `1`.
 * 8. If `n` is less than `0`:
 *    1. Set `n` to `-n`.
 *    2. Set `sign` to `1 - ((n&1)<<1)`.
 * 9. If `n` is equal to `0`:
 *    1. Return `(__ieee754_y0(x))`.
 * 10. If `n` is equal to `1`:
 *     1. Return `(sign*__ieee754_y1(x))`.
 * 11. If `ix` is equal to `0x7ff00000`:
 *     1. Return `zero`.
 * 12. If `ix` is greater than or equal to `0x52D00000`:
 *     1. Depending on `n&3`:
 *        1. Case `0`:
 *           1. Set `temp` to `sin(x)-cos(x)`.
 *           2. Leave the switch.
 *        2. Case `1`:
 *           1. Set `temp` to `-sin(x)-cos(x)`.
 *           2. Leave the switch.
 *        3. Case `2`:
 *           1. Set `temp` to `-sin(x)+cos(x)`.
 *           2. Leave the switch.
 *        4. Case `3`:
 *           1. Set `temp` to `sin(x)+cos(x)`.
 *           2. Leave the switch.
 *     2. Set `b` to `invsqrtpi*temp/sqrt(x)`.
 * 13. Otherwise:
 *     1. Call `__ieee754_y0()` with the argument `x`, and store the result in
 *        `a`.
 *     2. Call `__ieee754_y1()` with the argument `x`, and store the result in
 *        `b`.
 *     3. Use the macro `GET_HIGH_WORD()` with the arguments `high` and `b`.
 *     4. Set `i` to `1`; then while `i` is less than `n` and `high` is not
 *        equal to `0xfff00000`, repeat (after each pass: increment `i`):
 *        1. Set `temp` to `b`.
 *        2. Set `b` to `((double)(i+i)/x)*b - a`.
 *        3. Use the macro `GET_HIGH_WORD()` with the arguments `high` and `b`.
 *        4. Set `a` to `temp`.
 * 14. If `sign` is greater than `0`:
 *     1. Return `b`.
 * 15. Otherwise:
 *     1. Return `-b`.
 *
 * @calls
 * - `__ieee754_y0()`
 * - `__ieee754_y1()`
 * - `sin()`
 * - `cos()`
 * - `sqrt()`
 */
double
__ieee754_yn(int n, double x)
{
	int32_t i,hx,ix,lx;
	int32_t sign;
	double a, b, temp;

	temp = 0;
	EXTRACT_WORDS(hx,lx,x);
	ix = 0x7fffffff&hx;
    /* if Y(n,NaN) is NaN */
	if((ix|((uint32_t)(lx|-lx))>>31)>0x7ff00000) return x+x;
	if((ix|lx)==0) return -one/zero;
	if(hx<0) return zero/zero;
	sign = 1;
	if(n<0){
		n = -n;
		sign = 1 - ((n&1)<<1);
	}
	if(n==0) return(__ieee754_y0(x));
	if(n==1) return(sign*__ieee754_y1(x));
	if(ix==0x7ff00000) return zero;
	if(ix>=0x52D00000) { /* x > 2**302 */
    /* (x >> n**2)
     *	    Jn(x) = cos(x-(2n+1)*pi/4)*sqrt(2/x*pi)
     *	    Yn(x) = sin(x-(2n+1)*pi/4)*sqrt(2/x*pi)
     *	    Let s=sin(x), c=cos(x),
     *		xn=x-(2n+1)*pi/4, sqt2 = sqrt(2),then
     *
     *		   n	sin(xn)*sqt2	cos(xn)*sqt2
     *		----------------------------------
     *		   0	 s-c		 c+s
     *		   1	-s-c 		-c+s
     *		   2	-s+c		-c-s
     *		   3	 s+c		 c-s
     */
		switch(n&3) {
		    case 0: temp =  sin(x)-cos(x); break;
		    case 1: temp = -sin(x)-cos(x); break;
		    case 2: temp = -sin(x)+cos(x); break;
		    case 3: temp =  sin(x)+cos(x); break;
		}
		b = invsqrtpi*temp/sqrt(x);
	} else {
	    uint32_t high;
	    a = __ieee754_y0(x);
	    b = __ieee754_y1(x);
	/* quit if b is -inf */
	    GET_HIGH_WORD(high,b);
	    for(i=1;i<n&&high!=0xfff00000;i++){
		temp = b;
		b = ((double)(i+i)/x)*b - a;
		GET_HIGH_WORD(high,b);
		a = temp;
	    }
	}
	if(sign>0) return b; else return -b;
}
#endif

