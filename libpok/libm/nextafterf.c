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

/* s_nextafterf.c -- float version of s_nextafter.c.
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
 * nextafterf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

/**
 * @funcname `nextafterf`
 *
 * @description
 * Computes the next representable value after x in the direction of y, in float
 * (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `y` | `float` | [in] The second argument. |
 *
 * @return The result.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The bits of x |
 * | `hy` | `int32_t` | The bits of y |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `iy` | `int32_t` | The bits of y without the sign bit |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Use the macro `GET_FLOAT_WORD()` with the arguments `hy` and `y`.
 * 3. Set `ix` to `hx&0x7fffffff`.
 * 4. Set `iy` to `hy&0x7fffffff`.
 * 5. If `ix` is greater than `0x7f800000` or `iy` is greater than `0x7f800000`:
 *    1. Return `x+y`.
 * 6. If `x` is equal to `y`:
 *    1. Return `x`.
 * 7. If `ix` is equal to `0`:
 *    1. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and
 *       `(hy&0x80000000)|1`.
 *    2. Set `y` to `x*x`.
 *    3. If `y` is equal to `x`:
 *       1. Return `y`.
 *    4. Otherwise:
 *       1. Return `x`.
 * 8. If `hx` is greater than or equal to `0`:
 *    1. If `hx` is greater than `hy`:
 *       1. Subtract `1` from `hx`.
 *    2. Otherwise:
 *       1. Add `1` to `hx`.
 * 9. Otherwise:
 *    1. If `hy` is greater than or equal to `0` or `hx` is greater than `hy`:
 *       1. Subtract `1` from `hx`.
 *    2. Otherwise:
 *       1. Add `1` to `hx`.
 * 10. Set `hy` to `hx&0x7f800000`.
 * 11. If `hy` is greater than or equal to `0x7f800000`:
 *     1. Return `x+x`.
 * 12. If `hy` is less than `0x00800000`:
 *     1. Set `y` to `x*x`.
 *     2. If `y` is not equal to `x`:
 *        1. Use the macro `SET_FLOAT_WORD()` with the arguments `y` and `hx`.
 *        2. Return `y`.
 * 13. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `hx`.
 * 14. Return `x`.
 *
 * @calls
 * None.
 */
float
nextafterf(float x, float y)
{
	int32_t hx,hy,ix,iy;

	GET_FLOAT_WORD(hx,x);
	GET_FLOAT_WORD(hy,y);
	ix = hx&0x7fffffff;		/* |x| */
	iy = hy&0x7fffffff;		/* |y| */

	if((ix>0x7f800000) ||   /* x is nan */
	   (iy>0x7f800000))     /* y is nan */
	   return x+y;
	if(x==y) return x;		/* x=y, return x */
	if(ix==0) {				/* x == 0 */
	    SET_FLOAT_WORD(x,(hy&0x80000000)|1);/* return +-minsubnormal */
	    y = x*x;
	    if(y==x) return y; else return x;	/* raise underflow flag */
	}
	if(hx>=0) {				/* x > 0 */
	    if(hx>hy) {				/* x > y, x -= ulp */
		hx -= 1;
	    } else {				/* x < y, x += ulp */
		hx += 1;
	    }
	} else {				/* x < 0 */
	    if(hy>=0||hx>hy){			/* x < y, x -= ulp */
		hx -= 1;
	    } else {				/* x > y, x += ulp */
		hx += 1;
	    }
	}
	hy = hx&0x7f800000;
	if(hy>=0x7f800000) return x+x;	/* overflow  */
	if(hy<0x00800000) {		/* underflow */
	    y = x*x;
	    if(y!=x) {		/* raise underflow flag */
	        SET_FLOAT_WORD(y,hx);
		return y;
	    }
	}
	SET_FLOAT_WORD(x,hx);
	return x;
}

#endif
