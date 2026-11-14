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

/* e_log10f.c -- float version of e_log10.c.
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
 * `__ieee754_log10f()` of libpok's math library (fdlibm, from Sun
 * Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

static const float
two25      =  3.3554432000e+07, /* 0x4c000000 */
ivln10     =  4.3429449201e-01, /* 0x3ede5bd9 */
log10_2hi  =  3.0102920532e-01, /* 0x3e9a2080 */
log10_2lo  =  7.9034151668e-07; /* 0x355427db */

static const float zero   =  0.0;

/**
 * @funcname `__ieee754_log10f`
 *
 * @description
 * Computes the base 10 logarithm of x, in float: the IEEE 754 core of log10f().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ivln10` | `const float` | A constant of the algorithm |
 * | `log10_2hi` | `const float` | A constant of the algorithm |
 * | `log10_2lo` | `const float` | A constant of the algorithm |
 * | `two25` | `const float` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The bits of x |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `y` | `float` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Set `k` to `0`.
 * 3. If `hx` is less than `0x00800000`:
 *    1. If `(hx&0x7fffffff)` is equal to `0`:
 *       1. Return `-two25/zero`.
 *    2. If `hx` is less than `0`:
 *       1. Return `(x-x)/zero`.
 *    3. Subtract `25` from `k`.
 *    4. Multiply `x` by `two25`.
 *    5. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 4. If `hx` is greater than or equal to `0x7f800000`:
 *    1. Return `x+x`.
 * 5. Add `(hx>>23)-127` to `k`.
 * 6. Set `i` to `((uint32_t)k&0x80000000)>>31`.
 * 7. Set `hx` to `(hx&0x007fffff)|((0x7f-i)<<23)`.
 * 8. Set `y` to `(float)(k+i)`.
 * 9. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `hx`.
 * 10. Set `z` to `y*log10_2lo + ivln10*__ieee754_logf(x)`.
 * 11. Return `z+y*log10_2hi`.
 *
 * @calls
 * - `__ieee754_logf()`
 */
float
__ieee754_log10f(float x)
{
	float y,z;
	int32_t i,k,hx;

	GET_FLOAT_WORD(hx,x);

        k=0;
        if (hx < 0x00800000) {                  /* x < 2**-126  */
            if ((hx&0x7fffffff)==0)
                return -two25/zero;             /* log(+-0)=-inf */
            if (hx<0) return (x-x)/zero;        /* log(-#) = NaN */
            k -= 25; x *= two25; /* subnormal number, scale up x */
	    GET_FLOAT_WORD(hx,x);
        }
	if (hx >= 0x7f800000) return x+x;
	k += (hx>>23)-127;
	i  = ((uint32_t)k&0x80000000)>>31;
        hx = (hx&0x007fffff)|((0x7f-i)<<23);
        y  = (float)(k+i);
	SET_FLOAT_WORD(x,hx);
	z  = y*log10_2lo + ivln10*__ieee754_logf(x);
	return  z+y*log10_2hi;
}

#endif
