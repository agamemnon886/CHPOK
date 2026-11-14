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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/* @(#)w_jn.c 5.1 93/09/24 */
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
 * jn() and yn() of libpok's math library (fdlibm, from Sun Microsystems).
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

/*
 * wrapper jn(int n, double x), yn(int n, double x)
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

#include <libm.h>
#include "math_private.h"

/**
 * @funcname `jn`
 *
 * @description
 * Computes the Bessel function of the first kind of order n of x (C standard):
 * the wrapper of `__ieee754_jn()`.
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
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `__ieee754_jn()` with the arguments `n` and `x`, and return its
 *    result.
 *
 * @calls
 * - `__ieee754_jn()`
 */
double
jn(int n, double x)	/* wrapper jn */
{
#ifdef _IEEE_LIBM
	return __ieee754_jn(n,x);
#else
	double z;
	z = __ieee754_jn(n,x);
	if(_LIB_VERSION == _IEEE_ || isnan(x) ) return z;
	if(fabs(x)>X_TLOSS) {
	    return __kernel_standard((double)n,x,38); /* jn(|x|>X_TLOSS,n) */
	} else
	    return z;
#endif
}

/**
 * @funcname `yn`
 *
 * @description
 * Computes the Bessel function of the second kind of order n of x (C standard):
 * the wrapper of `__ieee754_yn()`.
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
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `__ieee754_yn()` with the arguments `n` and `x`, and return its
 *    result.
 *
 * @calls
 * - `__ieee754_yn()`
 */
double
yn(int n, double x)	/* wrapper yn */
{
#ifdef _IEEE_LIBM
	return __ieee754_yn(n,x);
#else
	double z;
	z = __ieee754_yn(n,x);
	if(_LIB_VERSION == _IEEE_ || isnan(x) ) return z;
        if(x <= 0.0){
                if(x==0.0)
                    /* d= -one/(x-x); */
                    return __kernel_standard((double)n,x,12);
                else
                    /* d = zero/(x-x); */
                    return __kernel_standard((double)n,x,13);
        }
	if(x>X_TLOSS) {
	    return __kernel_standard((double)n,x,39); /* yn(x>X_TLOSS,n) */
	} else
	    return z;
#endif
}

#endif
