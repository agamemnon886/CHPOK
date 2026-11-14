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

/* s_log1pf.c -- float version of s_log1p.c.
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
 * log1pf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <types.h>
#include "math_private.h"

static const float
ln2_hi =   6.9313812256e-01,	/* 0x3f317180 */
ln2_lo =   9.0580006145e-06,	/* 0x3717f7d1 */
two25 =    3.355443200e+07,	/* 0x4c000000 */
Lp1 = 6.6666668653e-01,	/* 3F2AAAAB */
Lp2 = 4.0000000596e-01,	/* 3ECCCCCD */
Lp3 = 2.8571429849e-01, /* 3E924925 */
Lp4 = 2.2222198546e-01, /* 3E638E29 */
Lp5 = 1.8183572590e-01, /* 3E3A3325 */
Lp6 = 1.5313838422e-01, /* 3E1CD04F */
Lp7 = 1.4798198640e-01; /* 3E178897 */

static const float zero = 0.0;

/**
 * @funcname `log1pf`
 *
 * @description
 * Computes log(1 + x), accurate near 0, in float (C standard).
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
 * | `Lp1` | `const float` | A constant of the algorithm |
 * | `Lp2` | `const float` | A constant of the algorithm |
 * | `Lp3` | `const float` | A constant of the algorithm |
 * | `Lp4` | `const float` | A constant of the algorithm |
 * | `Lp5` | `const float` | A constant of the algorithm |
 * | `Lp6` | `const float` | A constant of the algorithm |
 * | `Lp7` | `const float` | A constant of the algorithm |
 * | `ln2_hi` | `const float` | A constant of the algorithm |
 * | `ln2_lo` | `const float` | A constant of the algorithm |
 * | `two25` | `const float` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `R` | `float` | A temporary of the computation |
 * | `ax` | `int32_t` | The bits of x without the sign bit |
 * | `c` | `float` | A temporary of the computation |
 * | `f` | `float` | A temporary of the computation |
 * | `hfsq` | `float` | A temporary of the computation |
 * | `hu` | `int32_t` | The bits of u |
 * | `hx` | `int32_t` | The bits of x |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `s` | `float` | A temporary of the computation |
 * | `u` | `float` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `f` to `c = 0`.
 * 2. Set `hu` to `0`.
 * 3. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 4. Set `ax` to `hx&0x7fffffff`.
 * 5. Set `k` to `1`.
 * 6. If `hx` is less than `0x3ed413d7`:
 *    1. If `ax` is greater than or equal to `0x3f800000`:
 *       1. If `x` is equal to `(float)-1.0`:
 *          1. Return `-two25/zero`.
 *       2. Otherwise:
 *          1. Return `(x-x)/(x-x)`.
 *    2. If `ax` is less than `0x31000000`:
 *       1. If `two25+x` is greater than `zero` and `ax` is less than
 *          `0x24800000`:
 *          1. Return `x`.
 *       2. Otherwise:
 *          1. Return `x - x*x*(float)0.5`.
 *    3. If `hx` is greater than `0` or `hx` is less than or equal to
 *       `((int32_t)0xbe95f61f)`:
 *       1. Set `k` to `0`.
 *       2. Set `f` to `x`.
 *       3. Set `hu` to `1`.
 * 7. If `hx` is greater than or equal to `0x7f800000`:
 *    1. Return `x+x`.
 * 8. If `k` is not equal to `0`:
 *    1. If `hx` is less than `0x5a000000`:
 *       1. Set `u` to `(float)1.0+x`.
 *       2. Use the macro `GET_FLOAT_WORD()` with the arguments `hu` and `u`.
 *       3. Set `k` to `(hu>>23)-127`.
 *       4. Set `c` to `(float)1.0-(u-x)` if `k` is greater than `0`, otherwise
 *          `x-(u-(float)1.0)`.
 *       5. Divide `c` by `u`.
 *    2. Otherwise:
 *       1. Set `u` to `x`.
 *       2. Use the macro `GET_FLOAT_WORD()` with the arguments `hu` and `u`.
 *       3. Set `k` to `(hu>>23)-127`.
 *       4. Set `c` to `0`.
 *    3. Keep only the bits `0x007fffff` of `hu`.
 *    4. If `hu` is less than `0x3504f7`:
 *       1. Use the macro `SET_FLOAT_WORD()` with the arguments `u` and
 *          `hu|0x3f800000`.
 *    5. Otherwise:
 *       1. Add `1` to `k`.
 *       2. Use the macro `SET_FLOAT_WORD()` with the arguments `u` and
 *          `hu|0x3f000000`.
 *       3. Set `hu` to `(0x00800000-hu)>>2`.
 *    6. Set `f` to `u-(float)1.0`.
 * 9. Set `hfsq` to `(float)0.5*f*f`.
 * 10. If `hu` is equal to `0`:
 *     1. If `f` is equal to `zero`:
 *        1. If `k` is equal to `0`:
 *           1. Return `zero`.
 *        2. Otherwise:
 *           1. Add `k*ln2_lo` to `c`.
 *           2. Return `k*ln2_hi+c`.
 *     2. Set `R` to `hfsq*((float)1.0-(float)0.66666666666666666*f)`.
 *     3. If `k` is equal to `0`:
 *        1. Return `f-R`.
 *     4. Otherwise:
 *        1. Return `k*ln2_hi-((R-(k*ln2_lo+c))-f)`.
 * 11. Set `s` to `f/((float)2.0+f)`.
 * 12. Set `z` to `s*s`.
 * 13. Set `R` to `z*(Lp1+z*(Lp2+z*(Lp3+z*(Lp4+z*(Lp5+z*(Lp6+z*Lp7))))))`.
 * 14. If `k` is equal to `0`:
 *     1. Return `f-(hfsq-s*(hfsq+R))`.
 * 15. Otherwise:
 *     1. Return `k*ln2_hi-((hfsq-(s*(hfsq+R)+(k*ln2_lo+c)))-f)`.
 *
 * @calls
 * None.
 */
float
log1pf(float x)
{
	float hfsq,f,c,s,z,R,u;
	int32_t k,hx,hu,ax;

	f = c = 0;
	hu = 0;
	GET_FLOAT_WORD(hx,x);
	ax = hx&0x7fffffff;

	k = 1;
	if (hx < 0x3ed413d7) {			/* x < 0.41422  */
	    if(ax>=0x3f800000) {		/* x <= -1.0 */
		if(x==(float)-1.0) return -two25/zero; /* log1p(-1)=+inf */
		else return (x-x)/(x-x);	/* log1p(x<-1)=NaN */
	    }
	    if(ax<0x31000000) {			/* |x| < 2**-29 */
		if(two25+x>zero			/* raise inexact */
	            &&ax<0x24800000) 		/* |x| < 2**-54 */
		    return x;
		else
		    return x - x*x*(float)0.5;
	    }
	    if(hx>0||hx<=((int32_t)0xbe95f61f)) {
		k=0;f=x;hu=1;}	/* -0.2929<x<0.41422 */
	}
	if (hx >= 0x7f800000) return x+x;
	if(k!=0) {
	    if(hx<0x5a000000) {
		u  = (float)1.0+x;
		GET_FLOAT_WORD(hu,u);
	        k  = (hu>>23)-127;
		/* correction term */
	        c  = (k>0)? (float)1.0-(u-x):x-(u-(float)1.0);
		c /= u;
	    } else {
		u  = x;
		GET_FLOAT_WORD(hu,u);
	        k  = (hu>>23)-127;
		c  = 0;
	    }
	    hu &= 0x007fffff;
	    if(hu<0x3504f7) {
	        SET_FLOAT_WORD(u,hu|0x3f800000);/* normalize u */
	    } else {
	        k += 1;
		SET_FLOAT_WORD(u,hu|0x3f000000);	/* normalize u/2 */
	        hu = (0x00800000-hu)>>2;
	    }
	    f = u-(float)1.0;
	}
	hfsq=(float)0.5*f*f;
	if(hu==0) {	/* |f| < 2**-20 */
	    if(f==zero) { if(k==0) return zero;
			  else {c += k*ln2_lo; return k*ln2_hi+c;}
	    }
	    R = hfsq*((float)1.0-(float)0.66666666666666666*f);
	    if(k==0) return f-R; else
	    	     return k*ln2_hi-((R-(k*ln2_lo+c))-f);
	}
 	s = f/((float)2.0+f);
	z = s*s;
	R = z*(Lp1+z*(Lp2+z*(Lp3+z*(Lp4+z*(Lp5+z*(Lp6+z*Lp7))))));
	if(k==0) return f-(hfsq-s*(hfsq+R)); else
		 return k*ln2_hi-((hfsq-(s*(hfsq+R)+(k*ln2_lo+c)))-f);
}

#endif

