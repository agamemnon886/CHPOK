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
 * Created by julien on Sat Jan 31 20:12:07 2009 
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

/* k_tanf.c -- float version of k_tan.c
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
 * `__kernel_tanf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"
static const float
one   =  1.0000000000e+00, /* 0x3f800000 */
pio4  =  7.8539812565e-01, /* 0x3f490fda */
pio4lo=  3.7748947079e-08, /* 0x33222168 */
T[] =  {
  3.3333334327e-01, /* 0x3eaaaaab */
  1.3333334029e-01, /* 0x3e088889 */
  5.3968254477e-02, /* 0x3d5d0dd1 */
  2.1869488060e-02, /* 0x3cb327a4 */
  8.8632395491e-03, /* 0x3c11371f */
  3.5920790397e-03, /* 0x3b6b6916 */
  1.4562094584e-03, /* 0x3abede48 */
  5.8804126456e-04, /* 0x3a1a26c8 */
  2.4646313977e-04, /* 0x398137b9 */
  7.8179444245e-05, /* 0x38a3f445 */
  7.1407252108e-05, /* 0x3895c07a */
 -1.8558637748e-05, /* 0xb79bae5f */
  2.5907305826e-05, /* 0x37d95384 */
};

/**
 * @funcname `__kernel_tanf`
 *
 * @description
 * The kernel tangent of x + y on [-pi/4, pi/4] (y is the tail of x), or -1/tan
 * if iy is -1.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 * | `y` | `float` | [in] The tail of x. |
 * | `iy` | `int` | [in] 1 for the tangent, -1 for -1/tangent (or 0 if y is 0). |
 *
 * @return The tangent, or -1/tangent.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `T` | `const float[13]` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 * | `pio4` | `const float` | A constant of the algorithm |
 * | `pio4lo` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a` | `float` | A temporary of the computation |
 * | `hx` | `int32_t` | The bits of x |
 * | `i` | `int32_t` | The bits of z |
 * | `ix` | `int32_t` | The bits of x without the sign bit |
 * | `r` | `float` | A temporary of the computation |
 * | `s` | `float` | A temporary of the computation |
 * | `t` | `float` | A temporary of the computation |
 * | `v` | `float` | A temporary of the computation |
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
 * 3. If `ix` is less than `0x31800000`:
 *    1. If `(int)x` is equal to `0`:
 *       1. If `(ix|(iy+1))` is equal to `0`:
 *          1. Return `one/fabsf(x)`.
 *       2. Otherwise:
 *          1. Return `x` if `iy` is equal to `1`, otherwise `-one/x`.
 * 4. If `ix` is greater than or equal to `0x3f2ca140`:
 *    1. If `hx` is less than `0`:
 *       1. Set `x` to `-x`.
 *       2. Set `y` to `-y`.
 *    2. Set `z` to `pio4-x`.
 *    3. Set `w` to `pio4lo-y`.
 *    4. Set `x` to `z+w`.
 *    5. Set `y` to `0.0`.
 * 5. Set `z` to `x*x`.
 * 6. Set `w` to `z*z`.
 * 7. Set `r` to `T[1]+w*(T[3]+w*(T[5]+w*(T[7]+w*(T[9]+w*T[11]))))`.
 * 8. Set `v` to `z*(T[2]+w*(T[4]+w*(T[6]+w*(T[8]+w*(T[10]+w*T[12])))))`.
 * 9. Set `s` to `z*x`.
 * 10. Set `r` to `y + z*(s*(r+v)+y)`.
 * 11. Add `T[0]*s` to `r`.
 * 12. Set `w` to `x+r`.
 * 13. If `ix` is greater than or equal to `0x3f2ca140`:
 *     1. Set `v` to `(float)iy`.
 *     2. Return `(float)(1-((hx>>30)&2))*(v-(float)2.0*(x-(w*w/(w+v)-r)))`.
 * 14. If `iy` is equal to `1`:
 *     1. Return `w`.
 * 15. Otherwise:
 *     1. Set `z` to `w`.
 *     2. Use the macro `GET_FLOAT_WORD()` with the arguments `i` and `z`.
 *     3. Use the macro `SET_FLOAT_WORD()` with the arguments `z` and
 *        `i&0xfffff000`.
 *     4. Set `v` to `r-(z - x)`.
 *     5. Set `t` to `a = -(float)1.0/w`.
 *     6. Use the macro `GET_FLOAT_WORD()` with the arguments `i` and `t`.
 *     7. Use the macro `SET_FLOAT_WORD()` with the arguments `t` and
 *        `i&0xfffff000`.
 *     8. Set `s` to `(float)1.0+t*z`.
 *     9. Return `t+a*(s+t*v)`.
 *
 * @calls
 * - `fabsf()`
 */
float
__kernel_tanf(float x, float y, int iy)
{
	float z,r,v,w,s;
	int32_t ix,hx;
	GET_FLOAT_WORD(hx,x);
	ix = hx&0x7fffffff;	/* high word of |x| */
	if(ix<0x31800000)			/* x < 2**-28 */
	    {if((int)x==0) {			/* generate inexact */
		if((ix|(iy+1))==0) return one/fabsf(x);
		else return (iy==1)? x: -one/x;
	    }
	    }
	if(ix>=0x3f2ca140) { 			/* |x|>=0.6744 */
	    if(hx<0) {x = -x; y = -y;}
	    z = pio4-x;
	    w = pio4lo-y;
	    x = z+w; y = 0.0;
	}
	z	=  x*x;
	w 	=  z*z;
    /* Break x^5*(T[1]+x^2*T[2]+...) into
     *	  x^5(T[1]+x^4*T[3]+...+x^20*T[11]) +
     *	  x^5(x^2*(T[2]+x^4*T[4]+...+x^22*[T12]))
     */
	r = T[1]+w*(T[3]+w*(T[5]+w*(T[7]+w*(T[9]+w*T[11]))));
	v = z*(T[2]+w*(T[4]+w*(T[6]+w*(T[8]+w*(T[10]+w*T[12])))));
	s = z*x;
	r = y + z*(s*(r+v)+y);
	r += T[0]*s;
	w = x+r;
	if(ix>=0x3f2ca140) {
	    v = (float)iy;
	    return (float)(1-((hx>>30)&2))*(v-(float)2.0*(x-(w*w/(w+v)-r)));
	}
	if(iy==1) return w;
	else {		/* if allow error up to 2 ulp,
			   simply return -1.0/(x+r) here */
     /*  compute -1.0/(x+r) accurately */
	    float a,t;
	    int32_t i;
	    z  = w;
	    GET_FLOAT_WORD(i,z);
	    SET_FLOAT_WORD(z,i&0xfffff000);
	    v  = r-(z - x); 	/* z+v = r+x */
	    t = a  = -(float)1.0/w;	/* a = -1.0/w */
	    GET_FLOAT_WORD(i,t);
	    SET_FLOAT_WORD(t,i&0xfffff000);
	    s  = (float)1.0+t*z;
	    return t+a*(s+t*v);
	}
}

#endif
