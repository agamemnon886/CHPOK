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

/* s_floorf.c -- float version of s_floor.c.
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
 * floorf() of libpok's math library (fdlibm, from Sun Microsystems).
 */

/*
 * floorf(x)
 * Return x rounded toward -inf to integral value
 * Method:
 *	Bit twiddling.
 * Exception:
 *	Inexact flag raised if x not equal to floorf(x).
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float huge = 1.0e30;

/**
 * @funcname `floorf`
 *
 * @description
 * Computes x rounded down to an integer, in float (C standard).
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
 * | `huge` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | A temporary of the computation |
 * | `i0` | `int32_t` | The bits of x |
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
 *       1. If `huge+x` is greater than `(float)0.0`:
 *          1. If `i0` is greater than or equal to `0`:
 *             1. Set `i0` to `0`.
 *          2. Otherwise, if `(i0&0x7fffffff)` is not equal to `0`:
 *             1. Set `i0` to `0xbf800000`.
 *    2. Otherwise:
 *       1. Set `i` to `(0x007fffff)>>jj0`.
 *       2. If `(i0&i)` is equal to `0`:
 *          1. Return `x`.
 *       3. If `huge+x` is greater than `(float)0.0`:
 *          1. If `i0` is less than `0`:
 *             1. Add `(0x00800000)>>jj0` to `i0`.
 *          2. Keep only the bits `~i` of `i0`.
 * 4. Otherwise:
 *    1. If `jj0` is equal to `0x80`:
 *       1. Return `x+x`.
 *    2. Otherwise:
 *       1. Return `x`.
 * 5. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `i0`.
 * 6. Return `x`.
 *
 * @calls
 * None.
 */
float
floorf(float x)
{
	int32_t i0,jj0;
	uint32_t i;
	GET_FLOAT_WORD(i0,x);
	jj0 = ((i0>>23)&0xff)-0x7f;
	if(jj0<23) {
	    if(jj0<0) { 	/* raise inexact if x != 0 */
		if(huge+x>(float)0.0) {/* return 0*sign(x) if |x|<1 */
		    if(i0>=0) {i0=0;}
		    else if((i0&0x7fffffff)!=0)
			{ i0=0xbf800000;}
		}
	    } else {
		i = (0x007fffff)>>jj0;
		if((i0&i)==0) return x; /* x is integral */
		if(huge+x>(float)0.0) {	/* raise inexact flag */
		    if(i0<0) i0 += (0x00800000)>>jj0;
		    i0 &= (~i);
		}
	    }
	} else {
	    if(jj0==0x80) return x+x;	/* inf or NaN */
	    else return x;		/* x is integral */
	}
	SET_FLOAT_WORD(x,i0);
	return x;
}

#endif
