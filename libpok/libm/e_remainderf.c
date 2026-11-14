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

/* e_remainderf.c -- float version of e_remainder.c.
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
 * `__ieee754_remainderf()` of libpok's math library (fdlibm, from Sun
 * Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float zero = 0.0;


/**
 * @funcname `__ieee754_remainderf`
 *
 * @description
 * Computes the IEEE remainder of x/y, in float: the IEEE 754 core of
 * remainderf().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `p` | `float` | [in] The divisor. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hp` | `int32_t` | The bits of p |
 * | `hx` | `int32_t` | The bits of x |
 * | `p_half` | `float` | A temporary of the computation |
 * | `sx` | `uint32_t` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Use the macro `GET_FLOAT_WORD()` with the arguments `hp` and `p`.
 * 3. Set `sx` to `hx&0x80000000`.
 * 4. Keep only the bits `0x7fffffff` of `hp`.
 * 5. Keep only the bits `0x7fffffff` of `hx`.
 * 6. If `hp` is equal to `0`:
 *    1. Return `(x*p)/(x*p)`.
 * 7. If `hx` is greater than or equal to `0x7f800000` or `hp` is greater than
 *    `0x7f800000`:
 *    1. Return `(x*p)/(x*p)`.
 * 8. If `hp` is less than or equal to `0x7effffff`:
 *    1. Call `__ieee754_fmodf()` with the arguments `x` and `p+p`, and store
 *       the result in `x`.
 * 9. If `(hx-hp)` is equal to `0`:
 *    1. Return `zero*x`.
 * 10. Call `fabsf()` with the argument `x`, and store the result in `x`.
 * 11. Call `fabsf()` with the argument `p`, and store the result in `p`.
 * 12. If `hp` is less than `0x01000000`:
 *     1. If `x+x` is greater than `p`:
 *        1. Subtract `p` from `x`.
 *        2. If `x+x` is greater than or equal to `p`:
 *           1. Subtract `p` from `x`.
 * 13. Otherwise:
 *     1. Set `p_half` to `(float)0.5*p`.
 *     2. If `x` is greater than `p_half`:
 *        1. Subtract `p` from `x`.
 *        2. If `x` is greater than or equal to `p_half`:
 *           1. Subtract `p` from `x`.
 * 14. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 15. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `hx^sx`.
 * 16. Return `x`.
 *
 * @calls
 * - `__ieee754_fmodf()`
 * - `fabsf()`
 */
float
__ieee754_remainderf(float x, float p)
{
	int32_t hx,hp;
	uint32_t sx;
	float p_half;

	GET_FLOAT_WORD(hx,x);
	GET_FLOAT_WORD(hp,p);
	sx = hx&0x80000000;
	hp &= 0x7fffffff;
	hx &= 0x7fffffff;

    /* purge off exception values */
	if(hp==0) return (x*p)/(x*p);	 	/* p = 0 */
	if((hx>=0x7f800000)||			/* x not finite */
	  ((hp>0x7f800000)))			/* p is NaN */
	    return (x*p)/(x*p);


	if (hp<=0x7effffff) x = __ieee754_fmodf(x,p+p);	/* now x < 2p */
	if ((hx-hp)==0) return zero*x;
	x  = fabsf(x);
	p  = fabsf(p);
	if (hp<0x01000000) {
	    if(x+x>p) {
		x-=p;
		if(x+x>=p) x -= p;
	    }
	} else {
	    p_half = (float)0.5*p;
	    if(x>p_half) {
		x-=p;
		if(x>=p_half) x -= p;
	    }
	}
	GET_FLOAT_WORD(hx,x);
	SET_FLOAT_WORD(x,hx^sx);
	return x;
}
#endif
