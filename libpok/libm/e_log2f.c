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

/* e_logf.c -- float version of e_log.c.
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
 * `__ieee754_log2f()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

static const float
ln2 = 0.6931471805599452862268, 
two25 =    3.355443200e+07,	/* 0x4c000000 */
Lg1 = 6.6666668653e-01,	/* 3F2AAAAB */
Lg2 = 4.0000000596e-01,	/* 3ECCCCCD */
Lg3 = 2.8571429849e-01, /* 3E924925 */
Lg4 = 2.2222198546e-01, /* 3E638E29 */
Lg5 = 1.8183572590e-01, /* 3E3A3325 */
Lg6 = 1.5313838422e-01, /* 3E1CD04F */
Lg7 = 1.4798198640e-01; /* 3E178897 */

static const float zero   =  0.0;

/**
 * @funcname `__ieee754_log2f`
 *
 * @description
 * Computes the base 2 logarithm of x, in float: the IEEE 754 core of log2f().
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
 * | `Lg1` | `const float` | A constant of the algorithm |
 * | `Lg2` | `const float` | A constant of the algorithm |
 * | `Lg3` | `const float` | A constant of the algorithm |
 * | `Lg4` | `const float` | A constant of the algorithm |
 * | `Lg5` | `const float` | A constant of the algorithm |
 * | `Lg6` | `const float` | A constant of the algorithm |
 * | `Lg7` | `const float` | A constant of the algorithm |
 * | `ln2` | `const float` | A constant of the algorithm |
 * | `two25` | `const float` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `R` | `float` | A temporary of the computation |
 * | `dk` | `float` | A temporary of the computation |
 * | `f` | `float` | A temporary of the computation |
 * | `hfsq` | `float` | A temporary of the computation |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ix` | `int32_t` | The bits of x |
 * | `j` | `int32_t` | A temporary of the computation |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `s` | `float` | A temporary of the computation |
 * | `t1` | `float` | A temporary of the computation |
 * | `t2` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 * 2. Set `k` to `0`.
 * 3. If `ix` is less than `0x00800000`:
 *    1. If `(ix&0x7fffffff)` is equal to `0`:
 *       1. Return `-two25/zero`.
 *    2. If `ix` is less than `0`:
 *       1. Return `(x-x)/zero`.
 *    3. Subtract `25` from `k`.
 *    4. Multiply `x` by `two25`.
 *    5. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `x`.
 * 4. If `ix` is greater than or equal to `0x7f800000`:
 *    1. Return `x+x`.
 * 5. Add `(ix>>23)-127` to `k`.
 * 6. Keep only the bits `0x007fffff` of `ix`.
 * 7. Set `i` to `(ix+(0x95f64<<3))&0x800000`.
 * 8. Use the macro `SET_FLOAT_WORD()` with the arguments `x` and
 *    `ix|(i^0x3f800000)`.
 * 9. Add `(i>>23)` to `k`.
 * 10. Set `dk` to `(float)k`.
 * 11. Set `f` to `x-(float)1.0`.
 * 12. If `(0x007fffff&(15+ix))` is less than `16`:
 *     1. If `f` is equal to `zero`:
 *        1. Return `(dk)`.
 *     2. Set `R` to `f*f*((float)0.5-(float)0.33333333333333333*f)`.
 *     3. Return `(dk-(R-f)/ln2)`.
 * 13. Set `s` to `f/((float)2.0+f)`.
 * 14. Set `z` to `s*s`.
 * 15. Set `i` to `ix-(0x6147a<<3)`.
 * 16. Set `w` to `z*z`.
 * 17. Set `j` to `(0x6b851<<3)-ix`.
 * 18. Set `t1` to `w*(Lg2+w*(Lg4+w*Lg6))`.
 * 19. Set `t2` to `z*(Lg1+w*(Lg3+w*(Lg5+w*Lg7)))`.
 * 20. Set the bits `j` in `i`.
 * 21. Set `R` to `t2+t1`.
 * 22. If `i` is greater than `0`:
 *     1. Set `hfsq` to `(float)0.5*f*f`.
 *     2. Return `(dk-(hfsq-s*(hfsq+R)-f)/ln2)`.
 * 23. Otherwise:
 *     1. Return `(dk-((s*(f-R))-f)/ln2)`.
 *
 * @calls
 * None.
 */
float
__ieee754_log2f(float x)
{
	float hfsq,f,s,z,R,w,t1,t2,dk;
	int32_t k,ix,i,j;

	GET_FLOAT_WORD(ix,x);

	k=0;
	if (ix < 0x00800000) {			/* x < 2**-126  */
	    if ((ix&0x7fffffff)==0)
		return -two25/zero;		/* log(+-0)=-inf */
	    if (ix<0) return (x-x)/zero;	/* log(-#) = NaN */
	    k -= 25; x *= two25; /* subnormal number, scale up x */
	    GET_FLOAT_WORD(ix,x);
	}
	if (ix >= 0x7f800000) return x+x;
	k += (ix>>23)-127;
	ix &= 0x007fffff;
	i = (ix+(0x95f64<<3))&0x800000;
	SET_FLOAT_WORD(x,ix|(i^0x3f800000));	/* normalize x or x/2 */
	k += (i>>23);
	dk = (float)k;
	f = x-(float)1.0;
	if((0x007fffff&(15+ix))<16) {	/* |f| < 2**-20 */
	    if (f==zero) 
		    return (dk);
	    R = f*f*((float)0.5-(float)0.33333333333333333*f);
	    return (dk-(R-f)/ln2);
	}
 	s = f/((float)2.0+f);
	z = s*s;
	i = ix-(0x6147a<<3);
	w = z*z;
	j = (0x6b851<<3)-ix;
	t1= w*(Lg2+w*(Lg4+w*Lg6));
	t2= z*(Lg1+w*(Lg3+w*(Lg5+w*Lg7)));
	i |= j;
	R = t2+t1;
	if(i>0) {
	    hfsq=(float)0.5*f*f;
	    return (dk-(hfsq-s*(hfsq+R)-f)/ln2);
	} else
		return (dk-((s*(f-R))-f)/ln2);
}
#endif
