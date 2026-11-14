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

/* e_hypotf.c -- float version of e_hypot.c.
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
 * `__ieee754_hypotf()` of libpok's math library (fdlibm, from Sun
 * Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include "math_private.h"

/**
 * @funcname `__ieee754_hypotf`
 *
 * @description
 * Computes sqrt(x*x + y*y), without undue overflow or underflow, in float: the
 * IEEE 754 core of hypotf().
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
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `a` | `float` | A temporary of the computation |
 * | `b` | `float` | A temporary of the computation |
 * | `ha` | `int32_t` | The bits of x |
 * | `hb` | `int32_t` | The bits of y |
 * | `j` | `int32_t` | A temporary of the computation |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `t1` | `float` | A temporary of the computation |
 * | `t2` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 * | `y2` | `float` | A temporary of the computation |
 * | `yy1` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `a` to `x`.
 * 2. Set `b` to `y`.
 * 3. Use the macro `GET_FLOAT_WORD()` with the arguments `ha` and `x`.
 * 4. Keep only the bits `0x7fffffff` of `ha`.
 * 5. Use the macro `GET_FLOAT_WORD()` with the arguments `hb` and `y`.
 * 6. Keep only the bits `0x7fffffff` of `hb`.
 * 7. If `hb` is greater than `ha`:
 *    1. Set `a` to `y`.
 *    2. Set `b` to `x`.
 *    3. Set `j` to `ha`.
 *    4. Set `ha` to `hb`.
 *    5. Set `hb` to `j`.
 * 8. Otherwise:
 *    1. Set `a` to `x`.
 *    2. Set `b` to `y`.
 * 9. Use the macro `SET_FLOAT_WORD()` with the arguments `a` and `ha`.
 * 10. Use the macro `SET_FLOAT_WORD()` with the arguments `b` and `hb`.
 * 11. If `(ha-hb)` is greater than `0xf000000`:
 *     1. Return `a+b`.
 * 12. Set `k` to `0`.
 * 13. If `ha` is greater than `0x58800000`:
 *     1. If `ha` is greater than or equal to `0x7f800000`:
 *        1. Set `w` to `a+b`.
 *        2. If `ha` is equal to `0x7f800000`:
 *           1. Set `w` to `a`.
 *        3. If `hb` is equal to `0x7f800000`:
 *           1. Set `w` to `b`.
 *        4. Return `w`.
 *     2. Subtract `0x5d800000` from `ha`.
 *     3. Subtract `0x5d800000` from `hb`.
 *     4. Add `60` to `k`.
 *     5. Use the macro `SET_FLOAT_WORD()` with the arguments `a` and `ha`.
 *     6. Use the macro `SET_FLOAT_WORD()` with the arguments `b` and `hb`.
 * 14. If `hb` is less than `0x26800000`:
 *     1. If `hb` is less than or equal to `0x007fffff`:
 *        1. If `hb` is equal to `0`:
 *           1. Return `a`.
 *        2. Use the macro `SET_FLOAT_WORD()` with the arguments `t1` and
 *           `0x3f000000`.
 *        3. Multiply `b` by `t1`.
 *        4. Multiply `a` by `t1`.
 *        5. Subtract `126` from `k`.
 *     2. Otherwise:
 *        1. Add `0x5d800000` to `ha`.
 *        2. Add `0x5d800000` to `hb`.
 *        3. Subtract `60` from `k`.
 *        4. Use the macro `SET_FLOAT_WORD()` with the arguments `a` and `ha`.
 *        5. Use the macro `SET_FLOAT_WORD()` with the arguments `b` and `hb`.
 * 15. Set `w` to `a-b`.
 * 16. If `w` is greater than `b`:
 *     1. Use the macro `SET_FLOAT_WORD()` with the arguments `t1` and
 *        `ha&0xfffff000`.
 *     2. Set `t2` to `a-t1`.
 *     3. Call `__ieee754_sqrtf()` with the argument `t1*t1-(b*(-b)-t2*(a+t1))`,
 *        and store the result in `w`.
 * 17. Otherwise:
 *     1. Set `a` to `a+a`.
 *     2. Use the macro `SET_FLOAT_WORD()` with the arguments `yy1` and
 *        `hb&0xfffff000`.
 *     3. Set `y2` to `b - yy1`.
 *     4. Use the macro `SET_FLOAT_WORD()` with the arguments `t1` and
 *        `ha+0x00800000`.
 *     5. Set `t2` to `a - t1`.
 *     6. Call `__ieee754_sqrtf()` with the argument
 *        `t1*yy1-(w*(-w)-(t1*y2+t2*b))`, and store the result in `w`.
 * 18. If `k` is not equal to `0`:
 *     1. Use the macro `SET_FLOAT_WORD()` with the arguments `t1` and
 *        `0x3f800000+(k<<23)`.
 *     2. Return `t1*w`.
 * 19. Otherwise:
 *     1. Return `w`.
 *
 * @calls
 * - `__ieee754_sqrtf()`
 */
float
__ieee754_hypotf(float x, float y)
{
	float a=x,b=y,t1,t2,yy1,y2,w;
	int32_t j,k,ha,hb;

	GET_FLOAT_WORD(ha,x);
	ha &= 0x7fffffff;
	GET_FLOAT_WORD(hb,y);
	hb &= 0x7fffffff;
	if(hb > ha) {a=y;b=x;j=ha; ha=hb;hb=j;} else {a=x;b=y;}
	SET_FLOAT_WORD(a,ha);	/* a <- |a| */
	SET_FLOAT_WORD(b,hb);	/* b <- |b| */
	if((ha-hb)>0xf000000) {return a+b;} /* x/y > 2**30 */
	k=0;
	if(ha > 0x58800000) {	/* a>2**50 */
	   if(ha >= 0x7f800000) {	/* Inf or NaN */
	       w = a+b;			/* for sNaN */
	       if(ha == 0x7f800000) w = a;
	       if(hb == 0x7f800000) w = b;
	       return w;
	   }
	   /* scale a and b by 2**-60 */
	   ha -= 0x5d800000; hb -= 0x5d800000;	k += 60;
	   SET_FLOAT_WORD(a,ha);
	   SET_FLOAT_WORD(b,hb);
	}
	if(hb < 0x26800000) {	/* b < 2**-50 */
	    if(hb <= 0x007fffff) {	/* subnormal b or 0 */
	        if(hb==0) return a;
		SET_FLOAT_WORD(t1,0x3f000000);	/* t1=2^126 */
		b *= t1;
		a *= t1;
		k -= 126;
	    } else {		/* scale a and b by 2^60 */
	        ha += 0x5d800000; 	/* a *= 2^60 */
		hb += 0x5d800000;	/* b *= 2^60 */
		k -= 60;
		SET_FLOAT_WORD(a,ha);
		SET_FLOAT_WORD(b,hb);
	    }
	}
    /* medium size a and b */
	w = a-b;
	if (w>b) {
	    SET_FLOAT_WORD(t1,ha&0xfffff000);
	    t2 = a-t1;
	    w  = __ieee754_sqrtf(t1*t1-(b*(-b)-t2*(a+t1)));
	} else {
	    a  = a+a;
	    SET_FLOAT_WORD(yy1,hb&0xfffff000);
	    y2 = b - yy1;
	    SET_FLOAT_WORD(t1,ha+0x00800000);
	    t2 = a - t1;
	    w  = __ieee754_sqrtf(t1*yy1-(w*(-w)-(t1*y2+t2*b)));
	}
	if(k!=0) {
	    SET_FLOAT_WORD(t1,0x3f800000+(k<<23));
	    return t1*w;
	} else return w;
}
#endif

