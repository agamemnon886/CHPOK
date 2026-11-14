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

/* e_scalbf.c -- float version of e_scalb.c.
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
 * `__ieee754_scalbf()` of libpok's math library (fdlibm, from Sun
 * Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

#ifdef _SCALB_INT
float
__ieee754_scalbf(float x, int fn)
#else
/**
 * @funcname `__ieee754_scalbf`
 *
 * @description
 * Computes x times 2 raised to the power fn, in float: the IEEE 754 core of
 * scalbf().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `fn` | `float` | [in] The power of 2. |
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
 * 1. If `isnanf(x)||isnanf(fn)` holds:
 *    1. Return `x*fn`.
 * 2. If `finitef(fn)` is false:
 *    1. If `fn` is greater than `(float)0.0`:
 *       1. Return `x*fn`.
 *    2. Otherwise:
 *       1. Return `x/(-fn)`.
 * 3. If `rintf(fn)` is not equal to `fn`:
 *    1. Return `(fn-fn)/(fn-fn)`.
 * 4. If `fn` is greater than `(float)65000.0`:
 *    1. Call `scalbnf()` with the arguments `x` and `65000`, and return its
 *       result.
 * 5. If `-fn` is greater than `(float)65000.0`:
 *    1. Call `scalbnf()` with the arguments `x` and `-65000`, and return its
 *       result.
 * 6. Call `scalbnf()` with the arguments `x` and `(int)fn`, and return its
 *    result.
 *
 * @calls
 * - `isnanf()`
 * - `finitef()`
 * - `rintf()`
 * - `scalbnf()`
 */
float
__ieee754_scalbf(float x, float fn)
#endif
{
#ifdef _SCALB_INT
	return scalbnf(x,fn);
#else
	if (isnanf(x)||isnanf(fn)) return x*fn;
	if (!finitef(fn)) {
	    if(fn>(float)0.0) return x*fn;
	    else       return x/(-fn);
	}
	if (rintf(fn)!=fn) return (fn-fn)/(fn-fn);
	if ( fn > (float)65000.0) return scalbnf(x, 65000);
	if (-fn > (float)65000.0) return scalbnf(x,-65000);
	return scalbnf(x,(int)fn);
#endif
}
#endif

