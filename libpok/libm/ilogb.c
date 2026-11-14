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

/* @(#)s_ilogb.c 5.1 93/09/24 */
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
 * ilogb() of libpok's math library (fdlibm, from Sun Microsystems).
 */
/* ilogb(double x)
 * return the binary exponent of non-zero x
 * ilogb(0) = 0x80000001
 * ilogb(inf/NaN) = 0x7fffffff (no signal is raised)
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

/**
 * @funcname `ilogb`
 *
 * @description
 * Returns the binary exponent of x, as an int.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 *
 * @return The exponent (FP_ILOGB0 for 0, INT_MAX for an infinity or NaN).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The high word of x |
 * | `ix` | `int32_t` | A temporary of the computation |
 * | `lx` | `int32_t` | The low word of x |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_HIGH_WORD()` with the arguments `hx` and `x`.
 * 2. Keep only the bits `0x7fffffff` of `hx`.
 * 3. If `hx` is less than `0x00100000`:
 *    1. Use the macro `GET_LOW_WORD()` with the arguments `lx` and `x`.
 *    2. If `(hx|lx)` is equal to `0`:
 *       1. Return `0x80000001`.
 *    3. Otherwise, if `hx` is equal to `0`:
 *       1. Set `ix` to `-1043`; then while `lx` is greater than `0`, repeat
 *          (after each pass: shift `lx` left by `1` bits):
 *          1. Subtract `1` from `ix`.
 *    4. Otherwise:
 *       1. Set `ix` to `-1022`. Shift `hx` left by `11` bits; then while `hx`
 *          is greater than `0`, repeat (after each pass: shift `hx` left by `1`
 *          bits):
 *          1. Subtract `1` from `ix`.
 *    5. Return `ix`.
 * 4. Otherwise, if `hx` is less than `0x7ff00000`:
 *    1. Return `(hx>>20)-1023`.
 * 5. Otherwise:
 *    1. Return `0x7fffffff`.
 *
 * @calls
 * None.
 */
int
ilogb(double x)
{
	int32_t hx,lx,ix;

	GET_HIGH_WORD(hx,x);
	hx &= 0x7fffffff;
	if(hx<0x00100000) {
	    GET_LOW_WORD(lx,x);
	    if((hx|lx)==0)
		return 0x80000001;	/* ilogb(0) = 0x80000001 */
	    else			/* subnormal x */
		if(hx==0) {
		    for (ix = -1043; lx>0; lx<<=1) ix -=1;
		} else {
		    for (ix = -1022,hx<<=11; hx>0; hx<<=1) ix -=1;
		}
	    return ix;
	}
	else if (hx<0x7ff00000) return (hx>>20)-1023;
	else return 0x7fffffff;
}

#endif
