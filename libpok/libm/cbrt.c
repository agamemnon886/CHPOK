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

/* @(#)s_cbrt.c 5.1 93/09/24 */
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
 * cbrt() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <types.h>
#include <libm.h>
#include "math_private.h"

/* cbrt(x)
 * Return cube root of x
 */
static const uint32_t
	B1 = 715094163, /* B1 = (682-0.03306235651)*2**20 */
	B2 = 696219795; /* B2 = (664-0.03306235651)*2**20 */

static const double
C =  5.42857142857142815906e-01, /* 19/35     = 0x3FE15F15, 0xF15F15F1 */
D = -7.05306122448979611050e-01, /* -864/1225 = 0xBFE691DE, 0x2532C834 */
E =  1.41428571428571436819e+00, /* 99/70     = 0x3FF6A0EA, 0x0EA0EA0F */
F =  1.60714285714285720630e+00, /* 45/28     = 0x3FF9B6DB, 0x6DB6DB6E */
G =  3.57142857142857150787e-01; /* 5/14      = 0x3FD6DB6D, 0xB6DB6DB7 */

/**
 * @funcname `cbrt`
 *
 * @description
 * Computes the cube root of x (C standard).
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
 * | `B1` | `const uint32_t` | A constant of the algorithm |
 * | `B2` | `const uint32_t` | A constant of the algorithm |
 * | `C` | `const double` | A constant of the algorithm |
 * | `D` | `const double` | A constant of the algorithm |
 * | `E` | `const double` | A constant of the algorithm |
 * | `F` | `const double` | A constant of the algorithm |
 * | `G` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `high` | `uint32_t` | The high word of t |
 * | `hx` | `int32_t` | The high word of x |
 * | `low` | `uint32_t` | The low word of x |
 * | `r` | `double` | A temporary of the computation |
 * | `s` | `double` | A temporary of the computation |
 * | `sign` | `uint32_t` | A temporary of the computation |
 * | `t` | `double` | A temporary of the computation |
 * | `w` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `t` to `0.0`.
 * 2. Use the macro `GET_HIGH_WORD()` with the arguments `hx` and `x`.
 * 3. Set `sign` to `hx&0x80000000`.
 * 4. Toggle the bits `sign` in `hx`.
 * 5. If `hx` is greater than or equal to `0x7ff00000`:
 *    1. Return `(x+x)`.
 * 6. Use the macro `GET_LOW_WORD()` with the arguments `low` and `x`.
 * 7. If `(hx|low)` is equal to `0`:
 *    1. Return `(x)`.
 * 8. Use the macro `SET_HIGH_WORD()` with the arguments `x` and `hx`.
 * 9. If `hx` is less than `0x00100000`:
 *    1. Use the macro `SET_HIGH_WORD()` with the arguments `t` and
 *       `0x43500000`.
 *    2. Multiply `t` by `x`.
 *    3. Use the macro `GET_HIGH_WORD()` with the arguments `high` and `t`.
 *    4. Use the macro `SET_HIGH_WORD()` with the arguments `t` and
 *       `high/3+B2`.
 * 10. Otherwise:
 *     1. Use the macro `SET_HIGH_WORD()` with the arguments `t` and `hx/3+B1`.
 * 11. Set `r` to `t*t/x`.
 * 12. Set `s` to `C+r*t`.
 * 13. Multiply `t` by `G+F/(s+E+D/s)`.
 * 14. Use the macro `GET_HIGH_WORD()` with the arguments `high` and `t`.
 * 15. Use the macro `INSERT_WORDS()` with the arguments `t`, `high+0x00000001`
 *     and `0`.
 * 16. Set `s` to `t*t`.
 * 17. Set `r` to `x/s`.
 * 18. Set `w` to `t+t`.
 * 19. Set `r` to `(r-t)/(w+r)`.
 * 20. Set `t` to `t+t*r`.
 * 21. Use the macro `GET_HIGH_WORD()` with the arguments `high` and `t`.
 * 22. Use the macro `SET_HIGH_WORD()` with the arguments `t` and `high|sign`.
 * 23. Return `(t)`.
 *
 * @calls
 * None.
 */
double
cbrt(double x)
{
	int32_t	hx;
	double r,s,t=0.0,w;
	uint32_t sign;
	uint32_t high,low;

	GET_HIGH_WORD(hx,x);
	sign=hx&0x80000000; 		/* sign= sign(x) */
	hx  ^=sign;
	if(hx>=0x7ff00000) return(x+x); /* cbrt(NaN,INF) is itself */
	GET_LOW_WORD(low,x);
	if((hx|low)==0)
	    return(x);		/* cbrt(0) is itself */

	SET_HIGH_WORD(x,hx);	/* x <- |x| */
    /* rough cbrt to 5 bits */
	if(hx<0x00100000) 		/* subnormal number */
	  {SET_HIGH_WORD(t,0x43500000);	/* set t= 2**54 */
	   t*=x; GET_HIGH_WORD(high,t); SET_HIGH_WORD(t,high/3+B2);
	  }
	else
	  SET_HIGH_WORD(t,hx/3+B1);


    /* new cbrt to 23 bits, may be implemented in single precision */
	r=t*t/x;
	s=C+r*t;
	t*=G+F/(s+E+D/s);

    /* chopped to 20 bits and make it larger than cbrt(x) */
	GET_HIGH_WORD(high,t);
	INSERT_WORDS(t,high+0x00000001,0);


    /* one step newton iteration to 53 bits with error less than 0.667 ulps */
	s=t*t;		/* t*t is exact */
	r=x/s;
	w=t+t;
	r=(r-t)/(w+r);	/* r-s is exact */
	t=t+t*r;

    /* retore the sign bit */
	GET_HIGH_WORD(high,t);
	SET_HIGH_WORD(t,high|sign);
	return(t);
}
#endif
