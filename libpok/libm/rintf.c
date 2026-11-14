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

/* s_rintf.c -- float version of s_rint.c.
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
 * rintf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float
TWO23[2]={
  8.3886080000e+06, /* 0x4b000000 */
 -8.3886080000e+06, /* 0xcb000000 */
};

/**
 * @funcname `rintf`
 *
 * @description
 * Computes x rounded to an integer in the current rounding direction, in float
 * (C standard).
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
 * | `TWO23` | `const float[2]` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | A temporary of the computation |
 * | `i0` | `int32_t` | The bits of x |
 * | `i1` | `uint32_t` | A temporary of the computation |
 * | `jj0` | `int32_t` | A temporary of the computation |
 * | `sx` | `int32_t` | A temporary of the computation |
 * | `t` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `i0` and `x`.
 * 2. Set `sx` to `(i0>>31)&1`.
 * 3. Set `jj0` to `((i0>>23)&0xff)-0x7f`.
 * 4. If `jj0` is less than `23`:
 *    1. If `jj0` is less than `0`:
 *       1. If `(i0&0x7fffffff)` is equal to `0`:
 *          1. Return `x`.
 *       2. Set `i1` to `(i0&0x07fffff)`.
 *       3. Keep only the bits `0xfff00000` of `i0`.
 *       4. Set the bits `((i1|-i1)>>9)&0x400000` in `i0`.
 *       5. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `i0`.
 *       6. Set `w` to `TWO23[sx]+x`.
 *       7. Set `t` to `w-TWO23[sx]`.
 *       8. Use the macro `GET_FLOAT_WORD()` with the arguments `i0` and `t`.
 *       9. Use the macro `SET_FLOAT_WORD()` with the arguments `t` and
 *          `(i0&0x7fffffff)|(sx<<31)`.
 *       10. Return `t`.
 *    2. Otherwise:
 *       1. Set `i` to `(0x007fffff)>>jj0`.
 *       2. If `(i0&i)` is equal to `0`:
 *          1. Return `x`.
 *       3. Shift `i` right by `1` bits.
 *       4. If `(i0&i)` is not equal to `0`:
 *          1. Set `i0` to `(i0&(~i))|((0x100000)>>jj0)`.
 * 5. Otherwise:
 *    1. If `jj0` is equal to `0x80`:
 *       1. Return `x+x`.
 *    2. Otherwise:
 *       1. Return `x`.
 * 6. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `i0`.
 * 7. Set `w` to `TWO23[sx]+x`.
 * 8. Return `w-TWO23[sx]`.
 *
 * @calls
 * None.
 */
float
rintf(float x)
{
	int32_t i0,jj0,sx;
	uint32_t i,i1;
#ifdef __i386__ /* XXX gcc4 will omit the rounding otherwise */
	volatile
#endif
		float w;
	float t;
	GET_FLOAT_WORD(i0,x);
	sx = (i0>>31)&1;
	jj0 = ((i0>>23)&0xff)-0x7f;
	if(jj0<23) {
	    if(jj0<0) {
		if((i0&0x7fffffff)==0) return x;
		i1 = (i0&0x07fffff);
		i0 &= 0xfff00000;
		i0 |= ((i1|-i1)>>9)&0x400000;
		SET_FLOAT_WORD(x,i0);
	        w = TWO23[sx]+x;
	        t =  w-TWO23[sx];
		GET_FLOAT_WORD(i0,t);
		SET_FLOAT_WORD(t,(i0&0x7fffffff)|(sx<<31));
	        return t;
	    } else {
		i = (0x007fffff)>>jj0;
		if((i0&i)==0) return x; /* x is integral */
		i>>=1;
		if((i0&i)!=0) i0 = (i0&(~i))|((0x100000)>>jj0);
	    }
	} else {
	    if(jj0==0x80) return x+x;	/* inf or NaN */
	    else return x;		/* x is integral */
	}
	SET_FLOAT_WORD(x,i0);
	w = TWO23[sx]+x;
	return w-TWO23[sx];
}

#endif
