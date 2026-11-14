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

/* @(#)s_modf.c 5.1 93/09/24 */
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
 * modf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
/*
 * modf(double x, double *iptr)
 * return fraction part of x, and return x's integral part in *iptr.
 * Method:
 *	Bit twiddling.
 *
 * Exception:
 *	No exception.
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const double one = 1.0;

/**
 * @funcname `modf`
 *
 * @description
 * Splits x into its integer and fractional parts, both with the sign of x.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `iptr` | `double *` | [out] Where the integer part is stored. |
 *
 * @return The fractional part.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `one` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `high` | `uint32_t` | The high word of x |
 * | `i` | `uint32_t` | A temporary of the computation |
 * | `i0` | `int32_t` | The high word of x |
 * | `i1` | `int32_t` | The low word of x |
 * | `jj0` | `int32_t` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `i0`, `i1` and `x`.
 * 2. Set `jj0` to `(((uint32_t)i0>>20)&0x7ff)-0x3ff`.
 * 3. If `jj0` is less than `20`:
 *    1. If `jj0` is less than `0`:
 *       1. Use the macro `INSERT_WORDS()` with the arguments `*iptr`,
 *          `i0&0x80000000` and `0`.
 *       2. Return `x`.
 *    2. Otherwise:
 *       1. Set `i` to `(0x000fffff)>>jj0`.
 *       2. If `((i0&i)|i1)` is equal to `0`:
 *          1. Set `*iptr` to `x`.
 *          2. Use the macro `GET_HIGH_WORD()` with the arguments `high` and
 *             `x`.
 *          3. Use the macro `INSERT_WORDS()` with the arguments `x`,
 *             `high&0x80000000` and `0`.
 *          4. Return `x`.
 *       3. Otherwise:
 *          1. Use the macro `INSERT_WORDS()` with the arguments `*iptr`,
 *             `i0&(~i)` and `0`.
 *          2. Return `x - *iptr`.
 * 4. Otherwise, if `jj0` is greater than `51`:
 *    1. Set `*iptr` to `x*one`.
 *    2. Use the macro `GET_HIGH_WORD()` with the arguments `high` and `x`.
 *    3. Use the macro `INSERT_WORDS()` with the arguments `x`,
 *       `high&0x80000000` and `0`.
 *    4. Return `x`.
 * 5. Otherwise:
 *    1. Set `i` to `((uint32_t)(0xffffffff))>>(jj0-20)`.
 *    2. If `(i1&i)` is equal to `0`:
 *       1. Set `*iptr` to `x`.
 *       2. Use the macro `GET_HIGH_WORD()` with the arguments `high` and `x`.
 *       3. Use the macro `INSERT_WORDS()` with the arguments `x`,
 *          `high&0x80000000` and `0`.
 *       4. Return `x`.
 *    3. Otherwise:
 *       1. Use the macro `INSERT_WORDS()` with the arguments `*iptr`, `i0` and
 *          `i1&(~i)`.
 *       2. Return `x - *iptr`.
 *
 * @calls
 * None.
 */
double
modf(double x, double *iptr)
{
	int32_t i0,i1,jj0;
	uint32_t i;
	EXTRACT_WORDS(i0,i1,x);
	jj0 = (((uint32_t)i0>>20)&0x7ff)-0x3ff;	/* exponent of x */
	if(jj0<20) {			/* integer part in high x */
	    if(jj0<0) {			/* |x|<1 */
	        INSERT_WORDS(*iptr,i0&0x80000000,0);	/* *iptr = +-0 */
		return x;
	    } else {
		i = (0x000fffff)>>jj0;
		if(((i0&i)|i1)==0) {		/* x is integral */
		    uint32_t high;
		    *iptr = x;
		    GET_HIGH_WORD(high,x);
		    INSERT_WORDS(x,high&0x80000000,0);	/* return +-0 */
		    return x;
		} else {
		    INSERT_WORDS(*iptr,i0&(~i),0);
		    return x - *iptr;
		}
	    }
	} else if (jj0>51) {		/* no fraction part */
	    uint32_t high;
	    *iptr = x*one;
	    GET_HIGH_WORD(high,x);
	    INSERT_WORDS(x,high&0x80000000,0);	/* return +-0 */
	    return x;
	} else {			/* fraction part in low x */
	    i = ((uint32_t)(0xffffffff))>>(jj0-20);
	    if((i1&i)==0) { 		/* x is integral */
	        uint32_t high;
		*iptr = x;
		GET_HIGH_WORD(high,x);
		INSERT_WORDS(x,high&0x80000000,0);	/* return +-0 */
		return x;
	    } else {
	        INSERT_WORDS(*iptr,i0,i1&(~i));
		return x - *iptr;
	    }
	}
}

#endif
