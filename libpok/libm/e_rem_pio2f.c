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

/* e_rem_pio2f.c -- float version of e_rem_pio2.c
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
 * `__ieee754_rem_pio2f()` of libpok's math library (fdlibm, from Sun
 * Microsystems).
 */
/* __ieee754_rem_pio2f(x,y)
 *
 * return the remainder of x rem pi/2 in y[0]+y[1]
 * use __kernel_rem_pio2f()
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH
#include <libm.h>
#include "math_private.h"

/*
 * Table of constants for 2/pi, 396 Hex digits (476 decimal) of 2/pi
 */
static const int32_t two_over_pi[] = {
0xA2, 0xF9, 0x83, 0x6E, 0x4E, 0x44, 0x15, 0x29, 0xFC,
0x27, 0x57, 0xD1, 0xF5, 0x34, 0xDD, 0xC0, 0xDB, 0x62,
0x95, 0x99, 0x3C, 0x43, 0x90, 0x41, 0xFE, 0x51, 0x63,
0xAB, 0xDE, 0xBB, 0xC5, 0x61, 0xB7, 0x24, 0x6E, 0x3A,
0x42, 0x4D, 0xD2, 0xE0, 0x06, 0x49, 0x2E, 0xEA, 0x09,
0xD1, 0x92, 0x1C, 0xFE, 0x1D, 0xEB, 0x1C, 0xB1, 0x29,
0xA7, 0x3E, 0xE8, 0x82, 0x35, 0xF5, 0x2E, 0xBB, 0x44,
0x84, 0xE9, 0x9C, 0x70, 0x26, 0xB4, 0x5F, 0x7E, 0x41,
0x39, 0x91, 0xD6, 0x39, 0x83, 0x53, 0x39, 0xF4, 0x9C,
0x84, 0x5F, 0x8B, 0xBD, 0xF9, 0x28, 0x3B, 0x1F, 0xF8,
0x97, 0xFF, 0xDE, 0x05, 0x98, 0x0F, 0xEF, 0x2F, 0x11,
0x8B, 0x5A, 0x0A, 0x6D, 0x1F, 0x6D, 0x36, 0x7E, 0xCF,
0x27, 0xCB, 0x09, 0xB7, 0x4F, 0x46, 0x3F, 0x66, 0x9E,
0x5F, 0xEA, 0x2D, 0x75, 0x27, 0xBA, 0xC7, 0xEB, 0xE5,
0xF1, 0x7B, 0x3D, 0x07, 0x39, 0xF7, 0x8A, 0x52, 0x92,
0xEA, 0x6B, 0xFB, 0x5F, 0xB1, 0x1F, 0x8D, 0x5D, 0x08,
0x56, 0x03, 0x30, 0x46, 0xFC, 0x7B, 0x6B, 0xAB, 0xF0,
0xCF, 0xBC, 0x20, 0x9A, 0xF4, 0x36, 0x1D, 0xA9, 0xE3,
0x91, 0x61, 0x5E, 0xE6, 0x1B, 0x08, 0x65, 0x99, 0x85,
0x5F, 0x14, 0xA0, 0x68, 0x40, 0x8D, 0xFF, 0xD8, 0x80,
0x4D, 0x73, 0x27, 0x31, 0x06, 0x06, 0x15, 0x56, 0xCA,
0x73, 0xA8, 0xC9, 0x60, 0xE2, 0x7B, 0xC0, 0x8C, 0x6B,
};

/* This array is like the one in e_rem_pio2.c, but the numbers are
   single precision and the last 8 bits are forced to 0.  */
static const int32_t npio2_hw[] = {
0x3fc90f00, 0x40490f00, 0x4096cb00, 0x40c90f00, 0x40fb5300, 0x4116cb00,
0x412fed00, 0x41490f00, 0x41623100, 0x417b5300, 0x418a3a00, 0x4196cb00,
0x41a35c00, 0x41afed00, 0x41bc7e00, 0x41c90f00, 0x41d5a000, 0x41e23100,
0x41eec200, 0x41fb5300, 0x4203f200, 0x420a3a00, 0x42108300, 0x4216cb00,
0x421d1400, 0x42235c00, 0x4229a500, 0x422fed00, 0x42363600, 0x423c7e00,
0x4242c700, 0x42490f00
};

/*
 * invpio2:  24 bits of 2/pi
 * pio2_1:   first  17 bit of pi/2
 * pio2_1t:  pi/2 - pio2_1
 * pio2_2:   second 17 bit of pi/2
 * pio2_2t:  pi/2 - (pio2_1+pio2_2)
 * pio2_3:   third  17 bit of pi/2
 * pio2_3t:  pi/2 - (pio2_1+pio2_2+pio2_3)
 */

static const float
zero =  0.0000000000e+00, /* 0x00000000 */
half =  5.0000000000e-01, /* 0x3f000000 */
two8 =  2.5600000000e+02, /* 0x43800000 */
invpio2 =  6.3661980629e-01, /* 0x3f22f984 */
pio2_1  =  1.5707855225e+00, /* 0x3fc90f80 */
pio2_1t =  1.0804334124e-05, /* 0x37354443 */
pio2_2  =  1.0804273188e-05, /* 0x37354400 */
pio2_2t =  6.0770999344e-11, /* 0x2e85a308 */
pio2_3  =  6.0770943833e-11, /* 0x2e85a300 */
pio2_3t =  6.1232342629e-17; /* 0x248d3132 */

/**
 * @funcname `__ieee754_rem_pio2f`
 *
 * @description
 * Reduces x modulo pi/2: x - n*pi/2 as the sum y[0] + y[1].
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `y` | `float *` | [in,out] Where the reduced argument is stored. |
 *
 * @return n (modulo 8).
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `half` | `const float` | A constant of the algorithm |
 * | `invpio2` | `const float` | A constant of the algorithm |
 * | `npio2_hw` | `const int32_t[32]` | A constant of the algorithm |
 * | `pio2_1` | `const float` | A constant of the algorithm |
 * | `pio2_1t` | `const float` | A constant of the algorithm |
 * | `pio2_2` | `const float` | A constant of the algorithm |
 * | `pio2_2t` | `const float` | A constant of the algorithm |
 * | `pio2_3` | `const float` | A constant of the algorithm |
 * | `pio2_3t` | `const float` | A constant of the algorithm |
 * | `two8` | `const float` | A constant of the algorithm |
 * | `two_over_pi` | `const int32_t[198]` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `e0` | `int32_t` | A temporary of the computation |
 * | `fn` | `float` | A temporary of the computation |
 * | `high` | `uint32_t` | The bits of y[0] |
 * | `hx` | `int32_t` | The bits of x |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `j` | `int32_t` | A temporary of the computation |
 * | `n` | `int32_t` | A temporary of the computation |
 * | `nx` | `int32_t` | A temporary of the computation |
 * | `r` | `float` | A temporary of the computation |
 * | `t` | `float` | A temporary of the computation |
 * | `tx` | `float[3]` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Set `ix` to `hx&0x7fffffff`.
 * 3. If `ix` is less than or equal to `0x3f490fd8`:
 *    1. Set `y[0]` to `x`.
 *    2. Set `y[1]` to `0`.
 *    3. Return `0`.
 * 4. If `ix` is less than `0x4016cbe4`:
 *    1. If `hx` is greater than `0`:
 *       1. Set `z` to `x - pio2_1`.
 *       2. If `(ix&0xfffffff0)` is not equal to `0x3fc90fd0`:
 *          1. Set `y[0]` to `z - pio2_1t`.
 *          2. Set `y[1]` to `(z-y[0])-pio2_1t`.
 *       3. Otherwise:
 *          1. Subtract `pio2_2` from `z`.
 *          2. Set `y[0]` to `z - pio2_2t`.
 *          3. Set `y[1]` to `(z-y[0])-pio2_2t`.
 *       4. Return `1`.
 *    2. Otherwise:
 *       1. Set `z` to `x + pio2_1`.
 *       2. If `(ix&0xfffffff0)` is not equal to `0x3fc90fd0`:
 *          1. Set `y[0]` to `z + pio2_1t`.
 *          2. Set `y[1]` to `(z-y[0])+pio2_1t`.
 *       3. Otherwise:
 *          1. Add `pio2_2` to `z`.
 *          2. Set `y[0]` to `z + pio2_2t`.
 *          3. Set `y[1]` to `(z-y[0])+pio2_2t`.
 *       4. Return `-1`.
 * 5. If `ix` is less than or equal to `0x43490f80`:
 *    1. Call `fabsf()` with the argument `x`, and store the result in `t`.
 *    2. Set `n` to `(int32_t) (t*invpio2+half)`.
 *    3. Set `fn` to `(float)n`.
 *    4. Set `r` to `t-fn*pio2_1`.
 *    5. Set `w` to `fn*pio2_1t`.
 *    6. If `n` is less than `32` and `(int32_t)(ix&0xffffff00)` is not equal
 *       to `npio2_hw[n-1]`:
 *       1. Set `y[0]` to `r-w`.
 *    7. Otherwise:
 *       1. Set `j` to `ix>>23`.
 *       2. Set `y[0]` to `r-w`.
 *       3. Use the macro `GET_FLOAT_WORD()` with the arguments `high` and
 *          `y[0]`.
 *       4. Set `i` to `j-((high>>23)&0xff)`.
 *       5. If `i` is greater than `8`:
 *          1. Set `t` to `r`.
 *          2. Set `w` to `fn*pio2_2`.
 *          3. Set `r` to `t-w`.
 *          4. Set `w` to `fn*pio2_2t-((t-r)-w)`.
 *          5. Set `y[0]` to `r-w`.
 *          6. Use the macro `GET_FLOAT_WORD()` with the arguments `high` and
 *             `y[0]`.
 *          7. Set `i` to `j-((high>>23)&0xff)`.
 *          8. If `i` is greater than `25`:
 *             1. Set `t` to `r`.
 *             2. Set `w` to `fn*pio2_3`.
 *             3. Set `r` to `t-w`.
 *             4. Set `w` to `fn*pio2_3t-((t-r)-w)`.
 *             5. Set `y[0]` to `r-w`.
 *    8. Set `y[1]` to `(r-y[0])-w`.
 *    9. If `hx` is less than `0`:
 *       1. Set `y[0]` to `-y[0]`.
 *       2. Set `y[1]` to `-y[1]`.
 *       3. Return `-n`.
 *    10. Otherwise:
 *        1. Return `n`.
 * 6. If `ix` is greater than or equal to `0x7f800000`:
 *    1. Set `y[0]` to `y[1]=x-x`.
 *    2. Return `0`.
 * 7. Set `e0` to `(ix>>23)-134`.
 * 8. Use the macro `SET_FLOAT_WORD()` with the arguments `z` and
 *    `ix - ((int32_t)(e0<<23))`.
 * 9. Set `i` to `0`; then while `i` is less than `2`, repeat (after each pass:
 *    increment `i`):
 *    1. Set `tx[i]` to `(float)((int32_t)(z))`.
 *    2. Set `z` to `(z-tx[i])*two8`.
 * 10. Set `tx[2]` to `z`.
 * 11. Set `nx` to `3`.
 * 12. While `tx[nx-1]` is equal to `zero`:
 *     1. Decrement `nx`.
 * 13. Call `__kernel_rem_pio2f()` with the arguments `tx`, `y`, `e0`, `nx`, `2`
 *     and `(int*)two_over_pi`, and store the result in `n`.
 * 14. If `hx` is less than `0`:
 *     1. Set `y[0]` to `-y[0]`.
 *     2. Set `y[1]` to `-y[1]`.
 *     3. Return `-n`.
 * 15. Return `n`.
 *
 * @calls
 * - `fabsf()`
 * - `__kernel_rem_pio2f()`
 */
int32_t
__ieee754_rem_pio2f(float x, float *y)
{
	float z,w,t,r,fn;
	float tx[3];
	int32_t e0,i,j,nx,n,ix,hx;

	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;
	if(ix<=0x3f490fd8)   /* |x| ~<= pi/4 , no need for reduction */
	    {y[0] = x; y[1] = 0; return 0;}
	if(ix<0x4016cbe4) {  /* |x| < 3pi/4, special case with n=+-1 */
	    if(hx>0) {
		z = x - pio2_1;
		if((ix&0xfffffff0)!=0x3fc90fd0) { /* 24+24 bit pi OK */
		    y[0] = z - pio2_1t;
		    y[1] = (z-y[0])-pio2_1t;
		} else {		/* near pi/2, use 24+24+24 bit pi */
		    z -= pio2_2;
		    y[0] = z - pio2_2t;
		    y[1] = (z-y[0])-pio2_2t;
		}
		return 1;
	    } else {	/* negative x */
		z = x + pio2_1;
		if((ix&0xfffffff0)!=0x3fc90fd0) { /* 24+24 bit pi OK */
		    y[0] = z + pio2_1t;
		    y[1] = (z-y[0])+pio2_1t;
		} else {		/* near pi/2, use 24+24+24 bit pi */
		    z += pio2_2;
		    y[0] = z + pio2_2t;
		    y[1] = (z-y[0])+pio2_2t;
		}
		return -1;
	    }
	}
	if(ix<=0x43490f80) { /* |x| ~<= 2^7*(pi/2), medium size */
	    t  = fabsf(x);
	    n  = (int32_t) (t*invpio2+half);
	    fn = (float)n;
	    r  = t-fn*pio2_1;
	    w  = fn*pio2_1t;	/* 1st round good to 40 bit */
	    if(n<32&&(int32_t)(ix&0xffffff00)!=npio2_hw[n-1]) {
		y[0] = r-w;	/* quick check no cancellation */
	    } else {
	        uint32_t high;
	        j  = ix>>23;
	        y[0] = r-w;
		GET_FLOAT_WORD(high,y[0]);
	        i = j-((high>>23)&0xff);
	        if(i>8) {  /* 2nd iteration needed, good to 57 */
		    t  = r;
		    w  = fn*pio2_2;
		    r  = t-w;
		    w  = fn*pio2_2t-((t-r)-w);
		    y[0] = r-w;
		    GET_FLOAT_WORD(high,y[0]);
		    i = j-((high>>23)&0xff);
		    if(i>25)  {	/* 3rd iteration need, 74 bits acc */
		    	t  = r;	/* will cover all possible cases */
		    	w  = fn*pio2_3;
		    	r  = t-w;
		    	w  = fn*pio2_3t-((t-r)-w);
		    	y[0] = r-w;
		    }
		}
	    }
	    y[1] = (r-y[0])-w;
	    if(hx<0) 	{y[0] = -y[0]; y[1] = -y[1]; return -n;}
	    else	 return n;
	}
    /*
     * all other (large) arguments
     */
	if(ix>=0x7f800000) {		/* x is inf or NaN */
	    y[0]=y[1]=x-x; return 0;
	}
    /* set z = scalbn(|x|,ilogb(x)-7) */
	e0 	= (ix>>23)-134;		/* e0 = ilogb(z)-7; */
	SET_FLOAT_WORD(z, ix - ((int32_t)(e0<<23)));
	for(i=0;i<2;i++) {
		tx[i] = (float)((int32_t)(z));
		z     = (z-tx[i])*two8;
	}
	tx[2] = z;
	nx = 3;
	while(tx[nx-1]==zero) nx--;	/* skip zero term */
	n  =  __kernel_rem_pio2f(tx,y,e0,nx,2,(int*)two_over_pi);
	if(hx<0) {y[0] = -y[0]; y[1] = -y[1]; return -n;}
	return n;
}
#endif
