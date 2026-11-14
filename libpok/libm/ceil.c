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

/* @(#)s_ceil.c 5.1 93/09/24 */
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
 * ceil() of libpok's math library (fdlibm, from Sun Microsystems).
 */

/*
 * ceil(x)
 * Return x rounded toward -inf to integral value
 * Method:
 *	Bit twiddling.
 * Exception:
 *	Inexact flag raised if x not equal to ceil(x).
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const double huge = 1.0e300;

/**
 * @funcname `ceil`
 *
 * @description
 * Computes x rounded up to an integer (C standard).
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
 * | `huge` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | A temporary of the computation |
 * | `i0` | `int32_t` | The high word of x |
 * | `i1` | `int32_t` | The low word of x |
 * | `j` | `uint32_t` | A temporary of the computation |
 * | `jj0` | `int32_t` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `i0`, `i1` and `x`.
 * 2. Set `jj0` to `((i0>>20)&0x7ff)-0x3ff`.
 * 3. If `jj0` is less than `20`:
 *    1. If `jj0` is less than `0`:
 *       1. If `huge+x` is greater than `0.0`:
 *          1. If `i0` is less than `0`:
 *             1. Set `i0` to `0x80000000`.
 *             2. Set `i1` to `0`.
 *          2. Otherwise, if `(i0|i1)` is not equal to `0`:
 *             1. Set `i0` to `0x3ff00000`.
 *             2. Set `i1` to `0`.
 *    2. Otherwise:
 *       1. Set `i` to `(0x000fffff)>>jj0`.
 *       2. If `((i0&i)|i1)` is equal to `0`:
 *          1. Return `x`.
 *       3. If `huge+x` is greater than `0.0`:
 *          1. If `i0` is greater than `0`:
 *             1. Add `(0x00100000)>>jj0` to `i0`.
 *          2. Keep only the bits `~i` of `i0`.
 *          3. Set `i1` to `0`.
 * 4. Otherwise, if `jj0` is greater than `51`:
 *    1. If `jj0` is equal to `0x400`:
 *       1. Return `x+x`.
 *    2. Otherwise:
 *       1. Return `x`.
 * 5. Otherwise:
 *    1. Set `i` to `((uint32_t)(0xffffffff))>>(jj0-20)`.
 *    2. If `(i1&i)` is equal to `0`:
 *       1. Return `x`.
 *    3. If `huge+x` is greater than `0.0`:
 *       1. If `i0` is greater than `0`:
 *          1. If `jj0` is equal to `20`:
 *             1. Add `1` to `i0`.
 *          2. Otherwise:
 *             1. Set `j` to `i1 + (1<<(52-jj0))`.
 *             2. If `j` is less than `(uint32_t)i1`:
 *                1. Add `1` to `i0`.
 *             3. Set `i1` to `j`.
 *       2. Keep only the bits `~i` of `i1`.
 * 6. Use the macro `INSERT_WORDS()` with the arguments `x`, `i0` and `i1`.
 * 7. Return `x`.
 *
 * @calls
 * None.
 */
double
ceil(double x)
{
	int32_t i0,i1,jj0;
	uint32_t i,j;
	EXTRACT_WORDS(i0,i1,x);
	jj0 = ((i0>>20)&0x7ff)-0x3ff;
	if(jj0<20) {
	    if(jj0<0) { 	/* raise inexact if x != 0 */
		if(huge+x>0.0) {/* return 0*sign(x) if |x|<1 */
		    if(i0<0) {i0=0x80000000;i1=0;}
		    else if((i0|i1)!=0) { i0=0x3ff00000;i1=0;}
		}
	    } else {
		i = (0x000fffff)>>jj0;
		if(((i0&i)|i1)==0) return x; /* x is integral */
		if(huge+x>0.0) {	/* raise inexact flag */
		    if(i0>0) i0 += (0x00100000)>>jj0;
		    i0 &= (~i); i1=0;
		}
	    }
	} else if (jj0>51) {
	    if(jj0==0x400) return x+x;	/* inf or NaN */
	    else return x;		/* x is integral */
	} else {
	    i = ((uint32_t)(0xffffffff))>>(jj0-20);
	    if((i1&i)==0) return x;	/* x is integral */
	    if(huge+x>0.0) { 		/* raise inexact flag */
		if(i0>0) {
		    if(jj0==20) i0+=1;
		    else {
			j = i1 + (1<<(52-jj0));
			if(j<(uint32_t)i1) i0+=1;	/* got a carry */
			i1 = j;
		    }
		}
		i1 &= (~i);
	    }
	}
	INSERT_WORDS(x,i0,i1);
	return x;
}

#endif
