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

/* @(#)s_logb.c 5.1 93/09/24 */
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
 * logb() of libpok's math library (fdlibm, from Sun Microsystems).
 */

/*
 * double logb(x)
 * IEEE 754 logb. Included to pass IEEE test suite. Not recommend.
 * Use ilogb instead.
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

/**
 * @funcname `logb`
 *
 * @description
 * Computes the binary exponent of x, as a floating value (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ix` | `int32_t` | The high word of x |
 * | `lx` | `int32_t` | The low word of x |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `ix`, `lx` and `x`.
 * 2. Keep only the bits `0x7fffffff` of `ix`.
 * 3. If `(ix|lx)` is equal to `0`:
 *    1. Return `-1.0/fabs(x)`.
 * 4. If `ix` is greater than or equal to `0x7ff00000`:
 *    1. Return `x*x`.
 * 5. If `(ix>>=20)` is equal to `0`:
 *    1. Return `-1022.0`.
 * 6. Otherwise:
 *    1. Return `(double) (ix-1023)`.
 *
 * @calls
 * - `fabs()`
 */
double
logb(double x)
{
	int32_t lx,ix;
	EXTRACT_WORDS(ix,lx,x);
	ix &= 0x7fffffff;			/* high |x| */
	if((ix|lx)==0) return -1.0/fabs(x);
	if(ix>=0x7ff00000) return x*x;
	if((ix>>=20)==0) 			/* IEEE 754 logb */
		return -1022.0;
	else
		return (double) (ix-1023);
}

#endif
