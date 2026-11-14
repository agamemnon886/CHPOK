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


/* @(#)e_log.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/**
 * @file
 * `__ieee754_log2()` of libpok's math library (fdlibm, from Sun Microsystems).
 */


#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

static const double
ln2 = 0.6931471805599452862268,
two54   =  1.80143985094819840000e+16,  /* 43500000 00000000 */
Lg1 = 6.666666666666735130e-01,  /* 3FE55555 55555593 */
Lg2 = 3.999999999940941908e-01,  /* 3FD99999 9997FA04 */
Lg3 = 2.857142874366239149e-01,  /* 3FD24924 94229359 */
Lg4 = 2.222219843214978396e-01,  /* 3FCC71C5 1D8E78AF */
Lg5 = 1.818357216161805012e-01,  /* 3FC74664 96CB03DE */
Lg6 = 1.531383769920937332e-01,  /* 3FC39A09 D078C69F */
Lg7 = 1.479819860511658591e-01;  /* 3FC2F112 DF3E5244 */

static const double zero   =  0.0;

/**
 * @funcname `__ieee754_log2`
 *
 * @description
 * Computes the base 2 logarithm of x: the IEEE 754 core of log2().
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
 * | `Lg1` | `const double` | A constant of the algorithm |
 * | `Lg2` | `const double` | A constant of the algorithm |
 * | `Lg3` | `const double` | A constant of the algorithm |
 * | `Lg4` | `const double` | A constant of the algorithm |
 * | `Lg5` | `const double` | A constant of the algorithm |
 * | `Lg6` | `const double` | A constant of the algorithm |
 * | `Lg7` | `const double` | A constant of the algorithm |
 * | `ln2` | `const double` | A constant of the algorithm |
 * | `two54` | `const double` | A constant of the algorithm |
 * | `zero` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `R` | `double` | A temporary of the computation |
 * | `dk` | `double` | A temporary of the computation |
 * | `f` | `double` | A temporary of the computation |
 * | `hfsq` | `double` | A temporary of the computation |
 * | `hx` | `int32_t` | The high word of x |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `j` | `int32_t` | A temporary of the computation |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `lx` | `uint32_t` | The low word of x |
 * | `s` | `double` | A temporary of the computation |
 * | `t1` | `double` | A temporary of the computation |
 * | `t2` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 * | `z` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `hx`, `lx` and `x`.
 * 2. Set `k` to `0`.
 * 3. If `hx` is less than `0x00100000`:
 *    1. If `((hx&0x7fffffff)|lx)` is equal to `0`:
 *       1. Return `-two54/zero`.
 *    2. If `hx` is less than `0`:
 *       1. Return `(x-x)/zero`.
 *    3. Subtract `54` from `k`.
 *    4. Multiply `x` by `two54`.
 *    5. Use the macro `GET_HIGH_WORD()` with the arguments `hx` and `x`.
 * 4. If `hx` is greater than or equal to `0x7ff00000`:
 *    1. Return `x+x`.
 * 5. Add `(hx>>20)-1023` to `k`.
 * 6. Keep only the bits `0x000fffff` of `hx`.
 * 7. Set `i` to `(hx+0x95f64)&0x100000`.
 * 8. Use the macro `SET_HIGH_WORD()` with the arguments `x` and
 *    `hx|(i^0x3ff00000)`.
 * 9. Add `(i>>20)` to `k`.
 * 10. Set `f` to `x-1.0`.
 * 11. Set `dk` to `(double)k`.
 * 12. If `(0x000fffff&(2+hx))` is less than `3`:
 *     1. If `f` is equal to `zero`:
 *        1. Return `(dk)`.
 *     2. Set `R` to `f*f*(0.5-0.33333333333333333*f)`.
 *     3. Return `(dk-(R-f)/ln2)`.
 * 13. Set `s` to `f/(2.0+f)`.
 * 14. Set `z` to `s*s`.
 * 15. Set `i` to `hx-0x6147a`.
 * 16. Set `w` to `z*z`.
 * 17. Set `j` to `0x6b851-hx`.
 * 18. Set `t1` to `w*(Lg2+w*(Lg4+w*Lg6))`.
 * 19. Set `t2` to `z*(Lg1+w*(Lg3+w*(Lg5+w*Lg7)))`.
 * 20. Set the bits `j` in `i`.
 * 21. Set `R` to `t2+t1`.
 * 22. If `i` is greater than `0`:
 *     1. Set `hfsq` to `0.5*f*f`.
 *     2. Return `(dk-(hfsq-s*(hfsq+R)-f)/ln2)`.
 * 23. Otherwise:
 *     1. Return `(dk-((s*(f-R))-f)/ln2)`.
 *
 * @calls
 * None.
 */
double
__ieee754_log2(double x)
{
	double hfsq,f,s,z,R,w,t1,t2,dk;
	int32_t k,hx,i,j;
	uint32_t lx;

	EXTRACT_WORDS(hx,lx,x);

	k=0;
	if (hx < 0x00100000) {			/* x < 2**-1022  */
	    if (((hx&0x7fffffff)|lx)==0) 
		return -two54/zero;		/* log(+-0)=-inf */
	    if (hx<0) return (x-x)/zero;	/* log(-#) = NaN */
	    k -= 54; x *= two54; /* subnormal number, scale up x */
	    GET_HIGH_WORD(hx,x);
	} 
	if (hx >= 0x7ff00000) return x+x;
	k += (hx>>20)-1023;
	hx &= 0x000fffff;
	i = (hx+0x95f64)&0x100000;
	SET_HIGH_WORD(x,hx|(i^0x3ff00000));	/* normalize x or x/2 */
	k += (i>>20);
	f = x-1.0;
	dk = (double)k;
	if((0x000fffff&(2+hx))<3) {	/* |f| < 2**-20 */
	    if (f==zero) 
		    return (dk);
	    R = f*f*(0.5-0.33333333333333333*f);
	    return (dk-(R-f)/ln2);
	}
 	s = f/(2.0+f); 
	z = s*s;
	i = hx-0x6147a;
	w = z*z;
	j = 0x6b851-hx;
	t1= w*(Lg2+w*(Lg4+w*Lg6)); 
	t2= z*(Lg1+w*(Lg3+w*(Lg5+w*Lg7))); 
	i |= j;
	R = t2+t1;
	if(i>0) {
	    hfsq=0.5*f*f;
	    return (dk-(hfsq-s*(hfsq+R)-f)/ln2);
	} else
		return (dk-((s*(f-R))-f)/ln2);
}
#endif
