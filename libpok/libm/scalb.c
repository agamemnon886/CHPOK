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

/* @(#)w_scalb.c 5.1 93/09/24 */
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
 * scalb() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

/*
 * wrapper scalb(double x, double fn) is provide for
 * passing various standard test suite. One
 * should use scalbn() instead.
 */

#include <libm.h>
#include "math_private.h"

#include <errno.h>

#ifdef _SCALB_INT
double
scalb(double x, int fn)		/* wrapper scalb */
#else
/**
 * @funcname `scalb`
 *
 * @description
 * Computes x times 2 raised to the power fn (C standard): the wrapper of
 * `__ieee754_scalb()`.
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
 * 1. Call `__ieee754_scalb()` with the arguments `x` and `fn`, and return its
 *    result.
 *
 * @calls
 * - `__ieee754_scalb()`
 */
double
scalb(double x, double fn)	/* wrapper scalb */
#endif
{
#ifdef _IEEE_LIBM
	return __ieee754_scalb(x,fn);
#else
	double z;
	z = __ieee754_scalb(x,fn);
	if(_LIB_VERSION == _IEEE_) return z;
	if(!(finite(z)||isnan(z))&&finite(x)) {
	    return __kernel_standard(x,(double)fn,32); /* scalb overflow */
	}
	if(z==0.0&&z!=x) {
	    return __kernel_standard(x,(double)fn,33); /* scalb underflow */
	}
#ifndef _SCALB_INT
	if(!finite(fn)) errno = POK_ERRNO_ERANGE;
#endif
	return z;
#endif
}

#endif
