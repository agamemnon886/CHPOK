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

/* @(#)e_scalb.c 5.1 93/09/24 */
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
 * `__ieee754_scalb()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
/*
 * __ieee754_scalb(x, fn) is provide for
 * passing various standard test suite. One
 * should use scalbn() instead.
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

#ifdef _SCALB_INT
double
__ieee754_scalb(double x, int fn)
#else
/**
 * @funcname `__ieee754_scalb`
 *
 * @description
 * Computes x times 2 raised to the power fn: the IEEE 754 core of scalb().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `fn` | `double` | [in] The power of 2. |
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
 * 1. If `isnan(x)||isnan(fn)` holds:
 *    1. Return `x*fn`.
 * 2. If `finite(fn)` is false:
 *    1. If `fn` is greater than `0.0`:
 *       1. Return `x*fn`.
 *    2. Otherwise:
 *       1. Return `x/(-fn)`.
 * 3. If `rint(fn)` is not equal to `fn`:
 *    1. Return `(fn-fn)/(fn-fn)`.
 * 4. If `fn` is greater than `65000.0`:
 *    1. Call `scalbn()` with the arguments `x` and `65000`, and return its
 *       result.
 * 5. If `-fn` is greater than `65000.0`:
 *    1. Call `scalbn()` with the arguments `x` and `-65000`, and return its
 *       result.
 * 6. Call `scalbn()` with the arguments `x` and `(int)fn`, and return its
 *    result.
 *
 * @calls
 * - `isnan()`
 * - `finite()`
 * - `rint()`
 * - `scalbn()`
 */
double
__ieee754_scalb(double x, double fn)
#endif
{
#ifdef _SCALB_INT
	return scalbn(x,fn);
#else
	if (isnan(x)||isnan(fn)) return x*fn;
	if (!finite(fn)) {
	    if(fn>0.0) return x*fn;
	    else       return x/(-fn);
	}
	if (rint(fn)!=fn) return (fn-fn)/(fn-fn);
	if ( fn > 65000.0) return scalbn(x, 65000);
	if (-fn > 65000.0) return scalbn(x,-65000);
	return scalbn(x,(int)fn);
#endif
}
#endif

