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

/* s_modff.c -- float version of s_modf.c.
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
 * modff() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float one = 1.0;

/**
 * @funcname `modff`
 *
 * @description
 * Splits x into its integer and fractional parts, both with the sign of x.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `iptr` | `float *` | [out] Where the integer part is stored. |
 *
 * @return The fractional part.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `one` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | A temporary of the computation |
 * | `i0` | `int32_t` | The bits of x |
 * | `ix` | `uint32_t` | The bits of x |
 * | `jj0` | `int32_t` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `i0` and `x`.
 * 2. Set `jj0` to `((i0>>23)&0xff)-0x7f`.
 * 3. If `jj0` is less than `23`:
 *    1. If `jj0` is less than `0`:
 *       1. Use the macro `SET_FLOAT_WORD()` with the arguments `*iptr` and
 *          `i0&0x80000000`.
 *       2. Return `x`.
 *    2. Otherwise:
 *       1. Set `i` to `(0x007fffff)>>jj0`.
 *       2. If `(i0&i)` is equal to `0`:
 *          1. Set `*iptr` to `x`.
 *          2. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 *          3. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and
 *             `ix&0x80000000`.
 *          4. Return `x`.
 *       3. Otherwise:
 *          1. Use the macro `SET_FLOAT_WORD()` with the arguments `*iptr` and
 *             `i0&(~i)`.
 *          2. Return `x - *iptr`.
 * 4. Otherwise:
 *    1. Set `*iptr` to `x*one`.
 *    2. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 *    3. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and
 *       `ix&0x80000000`.
 *    4. Return `x`.
 *
 * @calls
 * None.
 */
float
modff(float x, float *iptr)
{
	int32_t i0,jj0;
	uint32_t i;
	GET_FLOAT_WORD(i0,x);
	jj0 = ((i0>>23)&0xff)-0x7f;	/* exponent of x */
	if(jj0<23) {			/* integer part in x */
	    if(jj0<0) {			/* |x|<1 */
	        SET_FLOAT_WORD(*iptr,i0&0x80000000);	/* *iptr = +-0 */
		return x;
	    } else {
		i = (0x007fffff)>>jj0;
		if((i0&i)==0) {			/* x is integral */
		    uint32_t ix;
		    *iptr = x;
		    GET_FLOAT_WORD(ix,x);
		    SET_FLOAT_WORD(x,ix&0x80000000);	/* return +-0 */
		    return x;
		} else {
		    SET_FLOAT_WORD(*iptr,i0&(~i));
		    return x - *iptr;
		}
	    }
	} else {			/* no fraction part */
	    uint32_t ix;
	    *iptr = x*one;
	    GET_FLOAT_WORD(ix,x);
	    SET_FLOAT_WORD(x,ix&0x80000000);	/* return +-0 */
	    return x;
	}
}

#endif
