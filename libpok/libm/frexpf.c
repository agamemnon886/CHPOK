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

/* s_frexpf.c -- float version of s_frexp.c.
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
 * frexpf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float
two25 =  3.3554432000e+07; /* 0x4c000000 */

/**
 * @funcname `frexpf`
 *
 * @description
 * Splits x into a mantissa in [0.5, 1) and a power of 2.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `eptr` | `int *` | [out] Where the power of 2 is stored. |
 *
 * @return The mantissa.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `two25` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The bits of x |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `0x7fffffff&hx`.
 * 3. Set `*eptr` to `0`.
 * 4. If `ix` is greater than or equal to `0x7f800000` or `ix` is equal to `0`:
 *    1. Return `x`.
 * 5. If `ix` is less than `0x00800000`:
 *    1. Multiply `x` by `two25`.
 *    2. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 *    3. Set `ix` to `hx&0x7fffffff`.
 *    4. Set `*eptr` to `-25`.
 * 6. Add `(ix>>23)-126` to `*eptr`.
 * 7. Set `hx` to `(hx&0x807fffff)|0x3f000000`.
 * 8. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `hx`.
 * 9. Return `x`.
 *
 * @calls
 * None.
 */
float
frexpf(float x, int *eptr)
{
	int32_t hx,ix;
	GET_FLOAT_WORD(hx,x);
	ix = 0x7fffffff&hx;
	*eptr = 0;
	if(ix>=0x7f800000||(ix==0)) return x;	/* 0,inf,nan */
	if (ix<0x00800000) {		/* subnormal */
	    x *= two25;
	    GET_FLOAT_WORD(hx,x);
	    ix = hx&0x7fffffff;
	    *eptr = -25;
	}
	*eptr += (ix>>23)-126;
	hx = (hx&0x807fffff)|0x3f000000;
	SET_FLOAT_WORD(x,hx);
	return x;
}

#endif

