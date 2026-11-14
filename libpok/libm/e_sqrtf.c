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

/* e_sqrtf.c -- float version of e_sqrt.c.
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
 * `__ieee754_sqrtf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH
#include <types.h>
#include "math_private.h"

static	const float	one	= 1.0, tiny=1.0e-30;

/**
 * @funcname `__ieee754_sqrtf`
 *
 * @description
 * Computes the square root of x, in float: the IEEE 754 core of sqrtf().
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
 * | `one` | `const float` | A constant of the algorithm |
 * | `tiny` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ix` | `int32_t` | The bits of x |
 * | `m` | `int32_t` | A temporary of the computation |
 * | `q` | `int32_t` | A temporary of the computation |
 * | `r` | `uint32_t` | A temporary of the computation |
 * | `s` | `int32_t` | A temporary of the computation |
 * | `sign` | `int32_t` | A temporary of the computation |
 * | `t` | `int32_t` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `sign` to `(int)0x80000000`.
 * 2. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 * 3. If `(ix&0x7f800000)` is equal to `0x7f800000`:
 *    1. Return `x*x+x`.
 * 4. If `ix` is less than or equal to `0`:
 *    1. If `(ix&(~sign))` is equal to `0`:
 *       1. Return `x`.
 *    2. Otherwise, if `ix` is less than `0`:
 *       1. Return `(x-x)/(x-x)`.
 * 5. Set `m` to `(ix>>23)`.
 * 6. If `m` is equal to `0`:
 *    1. Set `i` to `0`; then while `(ix&0x00800000)` is equal to `0`, repeat
 *       (after each pass: increment `i`):
 *       1. Shift `ix` left by `1` bits.
 *    2. Subtract `i-1` from `m`.
 * 7. Subtract `127` from `m`.
 * 8. Set `ix` to `(ix&0x007fffff)|0x00800000`.
 * 9. If `m&1` is true:
 *    1. Add `ix` to `ix`.
 * 10. Shift `m` right by `1` bits.
 * 11. Add `ix` to `ix`.
 * 12. Set `q` to `s = 0`.
 * 13. Set `r` to `0x01000000`.
 * 14. While `r` is not equal to `0`:
 *     1. Set `t` to `s+r`.
 *     2. If `t` is less than or equal to `ix`:
 *        1. Set `s` to `t+r`.
 *        2. Subtract `t` from `ix`.
 *        3. Add `r` to `q`.
 *     3. Add `ix` to `ix`.
 *     4. Shift `r` right by `1` bits.
 * 15. If `ix` is not equal to `0`:
 *     1. Set `z` to `one-tiny`.
 *     2. If `z` is greater than or equal to `one`:
 *        1. Set `z` to `one+tiny`.
 *        2. If `z` is greater than `one`:
 *           1. Add `2` to `q`.
 *        3. Otherwise:
 *           1. Add `(q&1)` to `q`.
 * 16. Set `ix` to `(q>>1)+0x3f000000`.
 * 17. Add `(m <<23)` to `ix`.
 * 18. Use the macro `SET_FLOAT_WORD()` with the arguments `z` and `ix`.
 * 19. Return `z`.
 *
 * @calls
 * None.
 */
float
__ieee754_sqrtf(float x)
{
	float z;
	int32_t sign = (int)0x80000000;
	int32_t ix,s,q,m,t,i;
	uint32_t r;

	GET_FLOAT_WORD(ix,x);

    /* take care of Inf and NaN */
	if((ix&0x7f800000)==0x7f800000) {
	    return x*x+x;		/* sqrt(NaN)=NaN, sqrt(+inf)=+inf
					   sqrt(-inf)=sNaN */
	}
    /* take care of zero */
	if(ix<=0) {
	    if((ix&(~sign))==0) return x;/* sqrt(+-0) = +-0 */
	    else if(ix<0)
		return (x-x)/(x-x);		/* sqrt(-ve) = sNaN */
	}
    /* normalize x */
	m = (ix>>23);
	if(m==0) {				/* subnormal x */
	    for(i=0;(ix&0x00800000)==0;i++) ix<<=1;
	    m -= i-1;
	}
	m -= 127;	/* unbias exponent */
	ix = (ix&0x007fffff)|0x00800000;
	if(m&1)	/* odd m, double x to make it even */
	    ix += ix;
	m >>= 1;	/* m = [m/2] */

    /* generate sqrt(x) bit by bit */
	ix += ix;
	q = s = 0;		/* q = sqrt(x) */
	r = 0x01000000;		/* r = moving bit from right to left */

	while(r!=0) {
	    t = s+r;
	    if(t<=ix) {
		s    = t+r;
		ix  -= t;
		q   += r;
	    }
	    ix += ix;
	    r>>=1;
	}

    /* use floating add to find out rounding direction */
	if(ix!=0) {
	    z = one-tiny; /* trigger inexact flag */
	    if (z>=one) {
	        z = one+tiny;
		if (z>one)
		    q += 2;
		else
		    q += (q&1);
	    }
	}
	ix = (q>>1)+0x3f000000;
	ix += (m <<23);
	SET_FLOAT_WORD(z,ix);
	return z;
}
#endif

