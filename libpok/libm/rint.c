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

/* @(#)s_rint.c 5.1 93/09/24 */
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
 * rint() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

/*
 * rint(x)
 * Return x rounded to integral value according to the prevailing
 * rounding mode.
 * Method:
 *	Using floating addition.
 * Exception:
 *	Inexact flag raised if x not equal to rint(x).
 */

#include <libm.h>
#include "math_private.h"

static const double
TWO52[2]={
  4.50359962737049600000e+15, /* 0x43300000, 0x00000000 */
 -4.50359962737049600000e+15, /* 0xC3300000, 0x00000000 */
};

/**
 * @funcname `rint`
 *
 * @description
 * Computes x rounded to an integer in the current rounding direction (C
 * standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `TWO52` | `const double[2]` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | A temporary of the computation |
 * | `i0` | `int32_t` | The high word of x |
 * | `i1` | `uint32_t` | The low word of x |
 * | `jj0` | `int32_t` | A temporary of the computation |
 * | `sx` | `int32_t` | A temporary of the computation |
 * | `t` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `i0`, `i1` and `x`.
 * 2. Set `sx` to `(i0>>31)&1`.
 * 3. Set `jj0` to `((i0>>20)&0x7ff)-0x3ff`.
 * 4. If `jj0` is less than `20`:
 *    1. If `jj0` is less than `0`:
 *       1. If `((i0&0x7fffffff)|i1)` is equal to `0`:
 *          1. Return `x`.
 *       2. Set the bits `(i0&0x0fffff)` in `i1`.
 *       3. Keep only the bits `0xfffe0000` of `i0`.
 *       4. Set the bits `((i1|-i1)>>12)&0x80000` in `i0`.
 *       5. Use the macro `SET_HIGH_WORD()` with the arguments `x` and `i0`.
 *       6. Set `w` to `TWO52[sx]+x`.
 *       7. Set `t` to `w-TWO52[sx]`.
 *       8. Use the macro `GET_HIGH_WORD()` with the arguments `i0` and `t`.
 *       9. Use the macro `SET_HIGH_WORD()` with the arguments `t` and
 *          `(i0&0x7fffffff)|(sx<<31)`.
 *       10. Return `t`.
 *    2. Otherwise:
 *       1. Set `i` to `(0x000fffff)>>jj0`.
 *       2. If `((i0&i)|i1)` is equal to `0`:
 *          1. Return `x`.
 *       3. Shift `i` right by `1` bits.
 *       4. If `((i0&i)|i1)` is not equal to `0`:
 *          1. If `jj0` is equal to `19`:
 *             1. Set `i1` to `0x40000000`.
 *          2. Otherwise:
 *             1. Set `i0` to `(i0&(~i))|((0x20000)>>jj0)`.
 * 5. Otherwise, if `jj0` is greater than `51`:
 *    1. If `jj0` is equal to `0x400`:
 *       1. Return `x+x`.
 *    2. Otherwise:
 *       1. Return `x`.
 * 6. Otherwise:
 *    1. Set `i` to `((uint32_t)(0xffffffff))>>(jj0-20)`.
 *    2. If `(i1&i)` is equal to `0`:
 *       1. Return `x`.
 *    3. Shift `i` right by `1` bits.
 *    4. If `(i1&i)` is not equal to `0`:
 *       1. Set `i1` to `(i1&(~i))|((0x40000000)>>(jj0-20))`.
 * 7. Use the macro `INSERT_WORDS()` with the arguments `x`, `i0` and `i1`.
 * 8. Set `w` to `TWO52[sx]+x`.
 * 9. Return `w-TWO52[sx]`.
 *
 * @calls
 * None.
 */
double
rint(double x)
{
	int32_t i0,jj0,sx;
	uint32_t i,i1;
	double w,t;
	EXTRACT_WORDS(i0,i1,x);
	sx = (i0>>31)&1;
	jj0 = ((i0>>20)&0x7ff)-0x3ff;
	if(jj0<20) {
	    if(jj0<0) {
		if(((i0&0x7fffffff)|i1)==0) return x;
		i1 |= (i0&0x0fffff);
		i0 &= 0xfffe0000;
		i0 |= ((i1|-i1)>>12)&0x80000;
		SET_HIGH_WORD(x,i0);
	        w = TWO52[sx]+x;
	        t =  w-TWO52[sx];
		GET_HIGH_WORD(i0,t);
		SET_HIGH_WORD(t,(i0&0x7fffffff)|(sx<<31));
	        return t;
	    } else {
		i = (0x000fffff)>>jj0;
		if(((i0&i)|i1)==0) return x; /* x is integral */
		i>>=1;
		if(((i0&i)|i1)!=0) {
		    if(jj0==19) i1 = 0x40000000; else
		    i0 = (i0&(~i))|((0x20000)>>jj0);
		}
	    }
	} else if (jj0>51) {
	    if(jj0==0x400) return x+x;	/* inf or NaN */
	    else return x;		/* x is integral */
	} else {
	    i = ((uint32_t)(0xffffffff))>>(jj0-20);
	    if((i1&i)==0) return x;	/* x is integral */
	    i>>=1;
	    if((i1&i)!=0) i1 = (i1&(~i))|((0x40000000)>>(jj0-20));
	}
	INSERT_WORDS(x,i0,i1);
	w = TWO52[sx]+x;
	return w-TWO52[sx];
}

#endif

