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

/* e_fmodf.c -- float version of e_fmod.c.
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
 * `__ieee754_fmodf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
/*
 * __ieee754_fmodf(x,y)
 * Return x mod y in exact arithmetic
 * Method: shift and subtract
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

static const float one = 1.0, Zero[] = {0.0, -0.0,};

/**
 * @funcname `__ieee754_fmodf`
 *
 * @description
 * Computes the remainder of x/y with the sign of x, in float: the IEEE 754 core
 * of fmodf().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `y` | `float` | [in] The second argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `Zero` | `const float[2]` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The bits of x |
 * | `hy` | `int32_t` | The bits of y |
 * | `hz` | `int32_t` | A temporary of the computation |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ix` | `int32_t` | A temporary of the computation |
 * | `iy` | `int32_t` | A temporary of the computation |
 * | `n` | `int32_t` | A temporary of the computation |
 * | `sx` | `int32_t` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Use the macro `GET_FLOAT_WORD()` with the arguments `hy` and `y`.
 * 3. Set `sx` to `hx&0x80000000`.
 * 4. Toggle the bits `sx` in `hx`.
 * 5. Keep only the bits `0x7fffffff` of `hy`.
 * 6. If `hy` is equal to `0` or `hx` is greater than or equal to `0x7f800000`
 *    or `hy` is greater than `0x7f800000`:
 *    1. Return `(x*y)/(x*y)`.
 * 7. If `hx` is less than `hy`:
 *    1. Return `x`.
 * 8. If `hx` is equal to `hy`:
 *    1. Return `Zero[(uint32_t)sx>>31]`.
 * 9. If `hx` is less than `0x00800000`:
 *    1. Set `ix` to `-126`. Set `i` to `(hx<<8)`; then while `i` is greater
 *       than `0`, repeat (after each pass: shift `i` left by `1` bits):
 *       1. Subtract `1` from `ix`.
 * 10. Otherwise:
 *     1. Set `ix` to `(hx>>23)-127`.
 * 11. If `hy` is less than `0x00800000`:
 *     1. Set `iy` to `-126`. Set `i` to `(hy<<8)`; then while `i` is greater
 *        than or equal to `0`, repeat (after each pass: shift `i` left by `1`
 *        bits):
 *        1. Subtract `1` from `iy`.
 * 12. Otherwise:
 *     1. Set `iy` to `(hy>>23)-127`.
 * 13. If `ix` is greater than or equal to `-126`:
 *     1. Set `hx` to `0x00800000|(0x007fffff&hx)`.
 * 14. Otherwise:
 *     1. Set `n` to `-126-ix`.
 *     2. Set `hx` to `hx<<n`.
 * 15. If `iy` is greater than or equal to `-126`:
 *     1. Set `hy` to `0x00800000|(0x007fffff&hy)`.
 * 16. Otherwise:
 *     1. Set `n` to `-126-iy`.
 *     2. Set `hy` to `hy<<n`.
 * 17. Set `n` to `ix - iy`.
 * 18. While `n--` is true:
 *     1. Set `hz` to `hx-hy`.
 *     2. If `hz` is less than `0`:
 *        1. Set `hx` to `hx+hx`.
 *     3. Otherwise:
 *        1. If `hz` is equal to `0`:
 *           1. Return `Zero[(uint32_t)sx>>31]`.
 *        2. Set `hx` to `hz+hz`.
 * 19. Set `hz` to `hx-hy`.
 * 20. If `hz` is greater than or equal to `0`:
 *     1. Set `hx` to `hz`.
 * 21. If `hx` is equal to `0`:
 *     1. Return `Zero[(uint32_t)sx>>31]`.
 * 22. While `hx` is less than `0x00800000`:
 *     1. Set `hx` to `hx+hx`.
 *     2. Subtract `1` from `iy`.
 * 23. If `iy` is greater than or equal to `-126`:
 *     1. Set `hx` to `((hx-0x00800000)|((iy+127)<<23))`.
 *     2. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `hx|sx`.
 * 24. Otherwise:
 *     1. Set `n` to `-126 - iy`.
 *     2. Shift `hx` right by `n` bits.
 *     3. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and `hx|sx`.
 *     4. Multiply `x` by `one`.
 * 25. Return `x`.
 *
 * @calls
 * None.
 */
float
__ieee754_fmodf(float x, float y)
{
	int32_t n,hx,hy,hz,ix,iy,sx,i;

	GET_FLOAT_WORD(hx,x);
	GET_FLOAT_WORD(hy,y);
	sx = hx&0x80000000;		/* sign of x */
	hx ^=sx;		/* |x| */
	hy &= 0x7fffffff;	/* |y| */

    /* purge off exception values */
	if(hy==0||(hx>=0x7f800000)||		/* y=0,or x not finite */
	   (hy>0x7f800000))			/* or y is NaN */
	    return (x*y)/(x*y);
	if(hx<hy) return x;			/* |x|<|y| return x */
	if(hx==hy)
	    return Zero[(uint32_t)sx>>31];	/* |x|=|y| return x*0*/

    /* determine ix = ilogb(x) */
	if(hx<0x00800000) {	/* subnormal x */
	    for (ix = -126,i=(hx<<8); i>0; i<<=1) ix -=1;
	} else ix = (hx>>23)-127;

    /* determine iy = ilogb(y) */
	if(hy<0x00800000) {	/* subnormal y */
	    for (iy = -126,i=(hy<<8); i>=0; i<<=1) iy -=1;
	} else iy = (hy>>23)-127;

    /* set up {hx,lx}, {hy,ly} and align y to x */
	if(ix >= -126)
	    hx = 0x00800000|(0x007fffff&hx);
	else {		/* subnormal x, shift x to normal */
	    n = -126-ix;
	    hx = hx<<n;
	}
	if(iy >= -126)
	    hy = 0x00800000|(0x007fffff&hy);
	else {		/* subnormal y, shift y to normal */
	    n = -126-iy;
	    hy = hy<<n;
	}

    /* fix point fmod */
	n = ix - iy;
	while(n--) {
	    hz=hx-hy;
	    if(hz<0){hx = hx+hx;}
	    else {
	    	if(hz==0) 		/* return sign(x)*0 */
		    return Zero[(uint32_t)sx>>31];
	    	hx = hz+hz;
	    }
	}
	hz=hx-hy;
	if(hz>=0) {hx=hz;}

    /* convert back to floating value and restore the sign */
	if(hx==0) 			/* return sign(x)*0 */
	    return Zero[(uint32_t)sx>>31];
	while(hx<0x00800000) {		/* normalize x */
	    hx = hx+hx;
	    iy -= 1;
	}
	if(iy>= -126) {		/* normalize output */
	    hx = ((hx-0x00800000)|((iy+127)<<23));
	    SET_FLOAT_WORD(x,hx|sx);
	} else {		/* subnormal output */
	    n = -126 - iy;
	    hx >>= n;
	    SET_FLOAT_WORD(x,hx|sx);
	    x *= one;		/* create necessary signal */
	}
	return x;		/* exact output */
}

#endif

