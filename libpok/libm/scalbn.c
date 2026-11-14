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

/* @(#)s_scalbn.c 5.1 93/09/24 */
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
 * scalbn() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

/*
 * scalbn (double x, int n)
 * scalbn(x,n) returns x* 2**n  computed by  exponent
 * manipulation rather than by actually performing an
 * exponentiation or a multiplication.
 */

#include <libm.h>
#include "math_private.h"

static const double
two54   =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
twom54  =  5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
huge   = 1.0e+300,
tiny   = 1.0e-300;

/**
 * @funcname `scalbn`
 *
 * @description
 * Computes x times 2 raised to the power n (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `n` | `int` | [in] The power of 2. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `huge` | `const double` | A constant of the algorithm |
 * | `tiny` | `const double` | A constant of the algorithm |
 * | `two54` | `const double` | A constant of the algorithm |
 * | `twom54` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The high word of x |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `lx` | `int32_t` | The low word of x |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 2. Set `k` to `((uint32_t)hx&0x7ff00000)>>20`.
 * 3. If `k` is equal to `0`:
 *    1. If `(lx|(hx&0x7fffffff))` is equal to `0`:
 *       1. Return `x`.
 *    2. Multiply `x` by `two54`.
 *    3. Use the macro `GET_HIGH_WORD()` with the arguments `hx` and `x`.
 *    4. Set `k` to `(((uint32_t)hx&0x7ff00000)>>20) - 54`.
 *    5. If `n` is less than `-50000`:
 *       1. Return `tiny*x`.
 * 4. If `k` is equal to `0x7ff`:
 *    1. Return `x+x`.
 * 5. Set `k` to `k+n`.
 * 6. If `k` is greater than `0x7fe`:
 *    1. Return `huge*copysign(huge,x)`.
 * 7. If `k` is greater than `0`:
 *    1. Use the macro `SET_HIGH_WORD()` with the arguments `x` and
 *       `(hx&0x800fffff)|(k<<20)`.
 *    2. Return `x`.
 * 8. If `k` is less than or equal to `-54`:
 *    1. If `n` is greater than `50000`:
 *       1. Return `huge*copysign(huge,x)`.
 *    2. Otherwise:
 *       1. Return `tiny*copysign(tiny,x)`.
 * 9. Add `54` to `k`.
 * 10. Use the macro `SET_HIGH_WORD()` with the arguments `x` and
 *     `(hx&0x800fffff)|(k<<20)`.
 * 11. Return `x*twom54`.
 *
 * @calls
 * - `copysign()`
 */
double
scalbn(double x, int n)
{
	int32_t k,hx,lx;
	EXTRACT_WORDS(hx,lx,x);
        k = ((uint32_t)hx&0x7ff00000)>>20;		/* extract exponent */
        if (k==0) {				/* 0 or subnormal x */
            if ((lx|(hx&0x7fffffff))==0) return x; /* +-0 */
	    x *= two54;
	    GET_HIGH_WORD(hx,x);
	    k = (((uint32_t)hx&0x7ff00000)>>20) - 54;
            if (n< -50000) return tiny*x; 	/*underflow*/
	    }
        if (k==0x7ff) return x+x;		/* NaN or Inf */
        k = k+n;
        if (k >  0x7fe) return huge*copysign(huge,x); /* overflow  */
        if (k > 0) 				/* normal result */
	    {SET_HIGH_WORD(x,(hx&0x800fffff)|(k<<20)); return x;}
        if (k <= -54) {
            if (n > 50000) 	/* in case integer overflow in n+k */
		return huge*copysign(huge,x);	/*overflow*/
	    else return tiny*copysign(tiny,x); 	/*underflow*/
	}
        k += 54;				/* subnormal result */
	SET_HIGH_WORD(x,(hx&0x800fffff)|(k<<20));
        return x*twom54;
}

#endif
