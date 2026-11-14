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

/* @(#)s_frexp.c 5.1 93/09/24 */
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
 * frexp() of libpok's math library (fdlibm, from Sun Microsystems).
 */
/*
 * for non-zero x
 *	x = frexp(arg,&exp);
 * return a double fp quantity x such that 0.5 <= |x| <1.0
 * and the corresponding binary exponent "exp". That is
 *	arg = x*2^exp.
 * If arg is inf, 0.0, or NaN, then frexp(arg,&exp) returns arg
 * with *exp=0.
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const double
two54 =  1.80143985094819840000e+16; /* 0x43500000, 0x00000000 */

/**
 * @funcname `frexp`
 *
 * @description
 * Splits x into a mantissa in [0.5, 1) and a power of 2.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `eptr` | `int *` | [out] Where the power of 2 is stored. |
 *
 * @return The mantissa.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `two54` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The high word of x |
 * | `ix` | `int32_t` | The high word of x without the sign bit |
 * | `lx` | `int32_t` | The low word of x |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 2. Set `ix` to `0x7fffffff&hx`.
 * 3. Set `*eptr` to `0`.
 * 4. If `ix` is greater than or equal to `0x7ff00000` or `(ix|lx)` is equal to
 *    `0`:
 *    1. Return `x`.
 * 5. If `ix` is less than `0x00100000`:
 *    1. Multiply `x` by `two54`.
 *    2. Use the macro `GET_HIGH_WORD()` with the arguments `hx` and `x`.
 *    3. Set `ix` to `hx&0x7fffffff`.
 *    4. Set `*eptr` to `-54`.
 * 6. Add `((uint32_t)ix>>20)-1022` to `*eptr`.
 * 7. Set `hx` to `(hx&0x800fffff)|0x3fe00000`.
 * 8. Use the macro `SET_HIGH_WORD()` with the arguments `x` and `hx`.
 * 9. Return `x`.
 *
 * @calls
 * None.
 */
double
frexp(double x, int *eptr)
{
	int32_t hx, ix, lx;
	EXTRACT_WORDS(hx,lx,x);
	ix = 0x7fffffff&hx;
	*eptr = 0;
	if(ix>=0x7ff00000||((ix|lx)==0)) return x;	/* 0,inf,nan */
	if (ix<0x00100000) {		/* subnormal */
	    x *= two54;
	    GET_HIGH_WORD(hx,x);
	    ix = hx&0x7fffffff;
	    *eptr = -54;
	}
	*eptr += ((uint32_t)ix>>20)-1022;
	hx = (hx&0x800fffff)|0x3fe00000;
	SET_HIGH_WORD(x,hx);
	return x;
}

#endif
