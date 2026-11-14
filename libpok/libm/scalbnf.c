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

/* s_scalbnf.c -- float version of s_scalbn.c.
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
 * scalbnf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float
two25   =  3.355443200e+07,	/* 0x4c000000 */
twom25  =  2.9802322388e-08,	/* 0x33000000 */
huge   = 1.0e+30,
tiny   = 1.0e-30;

/**
 * @funcname `scalbnf`
 *
 * @description
 * Computes x times 2 raised to the power n, in float (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `n` | `int` | [in] The power of 2. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `huge` | `const float` | A constant of the algorithm |
 * | `tiny` | `const float` | A constant of the algorithm |
 * | `two25` | `const float` | A constant of the algorithm |
 * | `twom25` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ix` | `int32_t` | The bits of x |
 * | `k` | `int32_t` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 * 2. Set `k` to `(ix&0x7f800000)>>23`.
 * 3. If `k` is equal to `0`:
 *    1. If `(ix&0x7fffffff)` is equal to `0`:
 *       1. Return `x`.
 *    2. Multiply `x` by `two25`.
 *    3. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 *    4. Set `k` to `((ix&0x7f800000)>>23) - 25`.
 *    5. If `n` is less than `-50000`:
 *       1. Return `tiny*x`.
 * 4. If `k` is equal to `0xff`:
 *    1. Return `x+x`.
 * 5. Set `k` to `k+n`.
 * 6. If `k` is greater than `0xfe`:
 *    1. Return `huge*copysignf(huge,x)`.
 * 7. If `k` is greater than `0`:
 *    1. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and
 *       `(ix&0x807fffff)|(k<<23)`.
 *    2. Return `x`.
 * 8. If `k` is less than or equal to `-25`:
 *    1. If `n` is greater than `50000`:
 *       1. Return `huge*copysignf(huge,x)`.
 *    2. Otherwise:
 *       1. Return `tiny*copysignf(tiny,x)`.
 * 9. Add `25` to `k`.
 * 10. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and
 *     `(ix&0x807fffff)|(k<<23)`.
 * 11. Return `x*twom25`.
 *
 * @calls
 * - `copysignf()`
 */
float
scalbnf(float x, int n)
{
	int32_t k,ix;
	GET_FLOAT_WORD(ix,x);
        k = (ix&0x7f800000)>>23;		/* extract exponent */
        if (k==0) {				/* 0 or subnormal x */
            if ((ix&0x7fffffff)==0) return x; /* +-0 */
	    x *= two25;
	    GET_FLOAT_WORD(ix,x);
	    k = ((ix&0x7f800000)>>23) - 25;
            if (n< -50000) return tiny*x; 	/*underflow*/
	    }
        if (k==0xff) return x+x;		/* NaN or Inf */
        k = k+n;
        if (k >  0xfe) return huge*copysignf(huge,x); /* overflow  */
        if (k > 0) 				/* normal result */
	    {SET_FLOAT_WORD(x,(ix&0x807fffff)|(k<<23)); return x;}
        if (k <= -25) {
            if (n > 50000) 	/* in case integer overflow in n+k */
		return huge*copysignf(huge,x);	/*overflow*/
	    else return tiny*copysignf(tiny,x);	/*underflow*/
	}
        k += 25;				/* subnormal result */
	SET_FLOAT_WORD(x,(ix&0x807fffff)|(k<<23));
        return x*twom25;
}

#endif
