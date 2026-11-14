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

/* @(#)e_fmod.c 5.1 93/09/24 */
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
 * `__ieee754_fmod()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
/*
 * __ieee754_fmod(x,y)
 * Return x mod y in exact arithmetic
 * Method: shift and subtract
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

static const double one = 1.0, Zero[] = {0.0, -0.0,};

/**
 * @funcname `__ieee754_fmod`
 *
 * @description
 * Computes the remainder of x/y with the sign of x: the IEEE 754 core of
 * fmod().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 * | `y` | `double` | [in] The second argument. |
 *
 * @return The result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `Zero` | `const double[2]` | A constant of the algorithm |
 * | `one` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hx` | `int32_t` | The high word of x |
 * | `hy` | `int32_t` | The high word of y |
 * | `hz` | `int32_t` | A temporary of the computation |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ix` | `int32_t` | A temporary of the computation |
 * | `iy` | `int32_t` | A temporary of the computation |
 * | `lx` | `uint32_t` | The low word of x |
 * | `ly` | `uint32_t` | The low word of y |
 * | `lz` | `uint32_t` | A temporary of the computation |
 * | `n` | `int32_t` | A temporary of the computation |
 * | `sx` | `int32_t` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 2. Use the macro `EXTRACT_WORDS()` with the arguments `hy`, `ly` and `y`.
 * 3. Set `sx` to `hx&0x80000000`.
 * 4. Toggle the bits `sx` in `hx`.
 * 5. Keep only the bits `0x7fffffff` of `hy`.
 * 6. If `(hy|ly)` is equal to `0` or `hx` is greater than or equal to
 *    `0x7ff00000` or `(hy|((ly|-ly)>>31))` is greater than `0x7ff00000`:
 *    1. Return `(x*y)/(x*y)`.
 * 7. If `hx` is less than or equal to `hy`:
 *    1. If `hx` is less than `hy` or `lx` is less than `ly`:
 *       1. Return `x`.
 *    2. If `lx` is equal to `ly`:
 *       1. Return `Zero[(uint32_t)sx>>31]`.
 * 8. If `hx` is less than `0x00100000`:
 *    1. If `hx` is equal to `0`:
 *       1. Set `ix` to `-1043`. Set `i` to `lx`; then while `i` is greater
 *          than `0`, repeat (after each pass: shift `i` left by `1` bits):
 *          1. Subtract `1` from `ix`.
 *    2. Otherwise:
 *       1. Set `ix` to `-1022`. Set `i` to `(hx<<11)`; then while `i` is
 *          greater than `0`, repeat (after each pass: shift `i` left by `1`
 *          bits):
 *          1. Subtract `1` from `ix`.
 * 9. Otherwise:
 *    1. Set `ix` to `(hx>>20)-1023`.
 * 10. If `hy` is less than `0x00100000`:
 *     1. If `hy` is equal to `0`:
 *        1. Set `iy` to `-1043`. Set `i` to `ly`; then while `i` is greater
 *           than `0`, repeat (after each pass: shift `i` left by `1` bits):
 *           1. Subtract `1` from `iy`.
 *     2. Otherwise:
 *        1. Set `iy` to `-1022`. Set `i` to `(hy<<11)`; then while `i` is
 *           greater than `0`, repeat (after each pass: shift `i` left by `1`
 *           bits):
 *           1. Subtract `1` from `iy`.
 * 11. Otherwise:
 *     1. Set `iy` to `(hy>>20)-1023`.
 * 12. If `ix` is greater than or equal to `-1022`:
 *     1. Set `hx` to `0x00100000|(0x000fffff&hx)`.
 * 13. Otherwise:
 *     1. Set `n` to `-1022-ix`.
 *     2. If `n` is less than or equal to `31`:
 *        1. Set `hx` to `(hx<<n)|(lx>>(32-n))`.
 *        2. Shift `lx` left by `n` bits.
 *     3. Otherwise:
 *        1. Set `hx` to `lx<<(n-32)`.
 *        2. Set `lx` to `0`.
 * 14. If `iy` is greater than or equal to `-1022`:
 *     1. Set `hy` to `0x00100000|(0x000fffff&hy)`.
 * 15. Otherwise:
 *     1. Set `n` to `-1022-iy`.
 *     2. If `n` is less than or equal to `31`:
 *        1. Set `hy` to `(hy<<n)|(ly>>(32-n))`.
 *        2. Shift `ly` left by `n` bits.
 *     3. Otherwise:
 *        1. Set `hy` to `ly<<(n-32)`.
 *        2. Set `ly` to `0`.
 * 16. Set `n` to `ix - iy`.
 * 17. While `n--` is true:
 *     1. Set `hz` to `hx-hy`.
 *     2. Set `lz` to `lx-ly`.
 *     3. If `lx` is less than `ly`:
 *        1. Subtract `1` from `hz`.
 *     4. If `hz` is less than `0`:
 *        1. Set `hx` to `hx+hx+(lx>>31)`.
 *        2. Set `lx` to `lx+lx`.
 *     5. Otherwise:
 *        1. If `(hz|lz)` is equal to `0`:
 *           1. Return `Zero[(uint32_t)sx>>31]`.
 *        2. Set `hx` to `hz+hz+(lz>>31)`.
 *        3. Set `lx` to `lz+lz`.
 * 18. Set `hz` to `hx-hy`.
 * 19. Set `lz` to `lx-ly`.
 * 20. If `lx` is less than `ly`:
 *     1. Subtract `1` from `hz`.
 * 21. If `hz` is greater than or equal to `0`:
 *     1. Set `hx` to `hz`.
 *     2. Set `lx` to `lz`.
 * 22. If `(hx|lx)` is equal to `0`:
 *     1. Return `Zero[(uint32_t)sx>>31]`.
 * 23. While `hx` is less than `0x00100000`:
 *     1. Set `hx` to `hx+hx+(lx>>31)`.
 *     2. Set `lx` to `lx+lx`.
 *     3. Subtract `1` from `iy`.
 * 24. If `iy` is greater than or equal to `-1022`:
 *     1. Set `hx` to `((hx-0x00100000)|((iy+1023)<<20))`.
 *     2. Use the macro `INSERT_WORDS()` with the arguments `x`, `hx|sx` and
 *        `lx`.
 * 25. Otherwise:
 *     1. Set `n` to `-1022 - iy`.
 *     2. If `n` is less than or equal to `20`:
 *        1. Set `lx` to `(lx>>n)|((uint32_t)hx<<(32-n))`.
 *        2. Shift `hx` right by `n` bits.
 *     3. Otherwise, if `n` is less than or equal to `31`:
 *        1. Set `lx` to `(hx<<(32-n))|(lx>>n)`.
 *        2. Set `hx` to `sx`.
 *     4. Otherwise:
 *        1. Set `lx` to `hx>>(n-32)`.
 *        2. Set `hx` to `sx`.
 *     5. Use the macro `INSERT_WORDS()` with the arguments `x`, `hx|sx` and
 *        `lx`.
 *     6. Multiply `x` by `one`.
 * 26. Return `x`.
 *
 * @calls
 * None.
 */
double
__ieee754_fmod(double x, double y)
{
	int32_t n,hx,hy,hz,ix,iy,sx,i;
	uint32_t lx,ly,lz;

	EXTRACT_WORDS(hx,lx,x);
	EXTRACT_WORDS(hy,ly,y);
	sx = hx&0x80000000;		/* sign of x */
	hx ^=sx;		/* |x| */
	hy &= 0x7fffffff;	/* |y| */

    /* purge off exception values */
	if((hy|ly)==0||(hx>=0x7ff00000)||	/* y=0,or x not finite */
	  ((hy|((ly|-ly)>>31))>0x7ff00000))	/* or y is NaN */
	    return (x*y)/(x*y);
	if(hx<=hy) {
	    if((hx<hy)||(lx<ly)) return x;	/* |x|<|y| return x */
	    if(lx==ly)
		return Zero[(uint32_t)sx>>31];	/* |x|=|y| return x*0*/
	}

    /* determine ix = ilogb(x) */
	if(hx<0x00100000) {	/* subnormal x */
	    if(hx==0) {
		for (ix = -1043, i=lx; i>0; i<<=1) ix -=1;
	    } else {
		for (ix = -1022,i=(hx<<11); i>0; i<<=1) ix -=1;
	    }
	} else ix = (hx>>20)-1023;

    /* determine iy = ilogb(y) */
	if(hy<0x00100000) {	/* subnormal y */
	    if(hy==0) {
		for (iy = -1043, i=ly; i>0; i<<=1) iy -=1;
	    } else {
		for (iy = -1022,i=(hy<<11); i>0; i<<=1) iy -=1;
	    }
	} else iy = (hy>>20)-1023;

    /* set up {hx,lx}, {hy,ly} and align y to x */
	if(ix >= -1022)
	    hx = 0x00100000|(0x000fffff&hx);
	else {		/* subnormal x, shift x to normal */
	    n = -1022-ix;
	    if(n<=31) {
	        hx = (hx<<n)|(lx>>(32-n));
	        lx <<= n;
	    } else {
		hx = lx<<(n-32);
		lx = 0;
	    }
	}
	if(iy >= -1022)
	    hy = 0x00100000|(0x000fffff&hy);
	else {		/* subnormal y, shift y to normal */
	    n = -1022-iy;
	    if(n<=31) {
	        hy = (hy<<n)|(ly>>(32-n));
	        ly <<= n;
	    } else {
		hy = ly<<(n-32);
		ly = 0;
	    }
	}

    /* fix point fmod */
	n = ix - iy;
	while(n--) {
	    hz=hx-hy;lz=lx-ly; if(lx<ly) hz -= 1;
	    if(hz<0){hx = hx+hx+(lx>>31); lx = lx+lx;}
	    else {
	    	if((hz|lz)==0) 		/* return sign(x)*0 */
		    return Zero[(uint32_t)sx>>31];
	    	hx = hz+hz+(lz>>31); lx = lz+lz;
	    }
	}
	hz=hx-hy;lz=lx-ly; if(lx<ly) hz -= 1;
	if(hz>=0) {hx=hz;lx=lz;}

    /* convert back to floating value and restore the sign */
	if((hx|lx)==0) 			/* return sign(x)*0 */
	    return Zero[(uint32_t)sx>>31];
	while(hx<0x00100000) {		/* normalize x */
	    hx = hx+hx+(lx>>31); lx = lx+lx;
	    iy -= 1;
	}
	if(iy>= -1022) {	/* normalize output */
	    hx = ((hx-0x00100000)|((iy+1023)<<20));
	    INSERT_WORDS(x,hx|sx,lx);
	} else {		/* subnormal output */
	    n = -1022 - iy;
	    if(n<=20) {
		lx = (lx>>n)|((uint32_t)hx<<(32-n));
		hx >>= n;
	    } else if (n<=31) {
		lx = (hx<<(32-n))|(lx>>n); hx = sx;
	    } else {
		lx = hx>>(n-32); hx = sx;
	    }
	    INSERT_WORDS(x,hx|sx,lx);
	    x *= one;		/* create necessary signal */
	}
	return x;		/* exact output */
}
#endif

