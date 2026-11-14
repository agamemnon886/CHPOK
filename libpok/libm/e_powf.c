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

/* e_powf.c -- float version of e_pow.c.
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
 * `__ieee754_powf()` of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

static const float huge = 1.0e+30, tiny = 1.0e-30;

static const float
bp[] = {1.0, 1.5,},
dp_h[] = { 0.0, 5.84960938e-01,}, /* 0x3f15c000 */
dp_l[] = { 0.0, 1.56322085e-06,}, /* 0x35d1cfdc */
zero    =  0.0,
one	=  1.0,
two	=  2.0,
two24	=  16777216.0,	/* 0x4b800000 */
	/* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
L1  =  6.0000002384e-01, /* 0x3f19999a */
L2  =  4.2857143283e-01, /* 0x3edb6db7 */
L3  =  3.3333334327e-01, /* 0x3eaaaaab */
L4  =  2.7272811532e-01, /* 0x3e8ba305 */
L5  =  2.3066075146e-01, /* 0x3e6c3255 */
L6  =  2.0697501302e-01, /* 0x3e53f142 */
P1   =  1.6666667163e-01, /* 0x3e2aaaab */
P2   = -2.7777778450e-03, /* 0xbb360b61 */
P3   =  6.6137559770e-05, /* 0x388ab355 */
P4   = -1.6533901999e-06, /* 0xb5ddea0e */
P5   =  4.1381369442e-08, /* 0x3331bb4c */
lg2  =  6.9314718246e-01, /* 0x3f317218 */
lg2_h  =  6.93145752e-01, /* 0x3f317200 */
lg2_l  =  1.42860654e-06, /* 0x35bfbe8c */
ovt =  4.2995665694e-08, /* -(128-log2(ovfl+.5ulp)) */
cp    =  9.6179670095e-01, /* 0x3f76384f =2/(3ln2) */
cp_h  =  9.6179199219e-01, /* 0x3f763800 =head of cp */
cp_l  =  4.7017383622e-06, /* 0x369dc3a0 =tail of cp_h */
ivln2    =  1.4426950216e+00, /* 0x3fb8aa3b =1/ln2 */
ivln2_h  =  1.4426879883e+00, /* 0x3fb8aa00 =16b 1/ln2*/
ivln2_l  =  7.0526075433e-06; /* 0x36eca570 =1/ln2 tail*/

/**
 * @funcname `__ieee754_powf`
 *
 * @description
 * Computes x raised to the power y, in float: the IEEE 754 core of powf().
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
 * | `L1` | `const float` | A constant of the algorithm |
 * | `L2` | `const float` | A constant of the algorithm |
 * | `L3` | `const float` | A constant of the algorithm |
 * | `L4` | `const float` | A constant of the algorithm |
 * | `L5` | `const float` | A constant of the algorithm |
 * | `L6` | `const float` | A constant of the algorithm |
 * | `P1` | `const float` | A constant of the algorithm |
 * | `P2` | `const float` | A constant of the algorithm |
 * | `P3` | `const float` | A constant of the algorithm |
 * | `P4` | `const float` | A constant of the algorithm |
 * | `P5` | `const float` | A constant of the algorithm |
 * | `bp` | `const float[2]` | A constant of the algorithm |
 * | `cp` | `const float` | A constant of the algorithm |
 * | `cp_h` | `const float` | A constant of the algorithm |
 * | `cp_l` | `const float` | A constant of the algorithm |
 * | `dp_h` | `const float[2]` | A constant of the algorithm |
 * | `dp_l` | `const float[2]` | A constant of the algorithm |
 * | `huge` | `const float` | A constant of the algorithm |
 * | `ivln2` | `const float` | A constant of the algorithm |
 * | `ivln2_h` | `const float` | A constant of the algorithm |
 * | `ivln2_l` | `const float` | A constant of the algorithm |
 * | `lg2` | `const float` | A constant of the algorithm |
 * | `lg2_h` | `const float` | A constant of the algorithm |
 * | `lg2_l` | `const float` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 * | `ovt` | `const float` | A constant of the algorithm |
 * | `tiny` | `const float` | A constant of the algorithm |
 * | `two` | `const float` | A constant of the algorithm |
 * | `two24` | `const float` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ax` | `float` | A temporary of the computation |
 * | `hx` | `int32_t` | The bits of x |
 * | `hy` | `int32_t` | The bits of y |
 * | `i` | `int32_t` | The bits of z without the sign bit |
 * | `is` | `int32_t` | The bits of t1 |
 * | `ix` | `int32_t` | The bits of ax |
 * | `iy` | `int32_t` | The bits of y without the sign bit |
 * | `j` | `int32_t` | The bits of z |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `n` | `int32_t` | A temporary of the computation |
 * | `p_h` | `float` | A temporary of the computation |
 * | `p_l` | `float` | A temporary of the computation |
 * | `r` | `float` | A temporary of the computation |
 * | `s` | `float` | A temporary of the computation |
 * | `s2` | `float` | A temporary of the computation |
 * | `s_h` | `float` | A temporary of the computation |
 * | `s_l` | `float` | A temporary of the computation |
 * | `t` | `float` | A temporary of the computation |
 * | `t1` | `float` | A temporary of the computation |
 * | `t2` | `float` | A temporary of the computation |
 * | `t_h` | `float` | A temporary of the computation |
 * | `t_l` | `float` | A temporary of the computation |
 * | `u` | `float` | A temporary of the computation |
 * | `v` | `float` | A temporary of the computation |
 * | `w` | `float` | A temporary of the computation |
 * | `yisint` | `int32_t` | A temporary of the computation |
 * | `yy1` | `float` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 * | `z_h` | `float` | A temporary of the computation |
 * | `z_l` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `hx` and `x`.
 * 2. Use the macro `GET_FLOAT_WORD()` with the arguments `hy` and `y`.
 * 3. Set `ix` to `hx&0x7fffffff`.
 * 4. Set `iy` to `hy&0x7fffffff`.
 * 5. If `iy` is equal to `0`:
 *    1. Return `one`.
 * 6. If `ix` is greater than `0x7f800000` or `iy` is greater than
 *    `0x7f800000`:
 *    1. Return `x+y`.
 * 7. Set `yisint` to `0`.
 * 8. If `hx` is less than `0`:
 *    1. If `iy` is greater than or equal to `0x4b800000`:
 *       1. Set `yisint` to `2`.
 *    2. Otherwise, if `iy` is greater than or equal to `0x3f800000`:
 *       1. Set `k` to `(iy>>23)-0x7f`.
 *       2. Set `j` to `iy>>(23-k)`.
 *       3. If `(j<<(23-k))` is equal to `iy`:
 *          1. Set `yisint` to `2-(j&1)`.
 * 9. If `iy` is equal to `0x7f800000`:
 *    1. If `ix` is equal to `0x3f800000`:
 *       1. Return `y - y`.
 *    2. Otherwise, if `ix` is greater than `0x3f800000`:
 *       1. Return `y` if `hy` is greater than or equal to `0`, otherwise
 *          `zero`.
 *    3. Otherwise:
 *       1. Return `-y` if `hy` is less than `0`, otherwise `zero`.
 * 10. If `iy` is equal to `0x3f800000`:
 *     1. If `hy` is less than `0`:
 *        1. Return `one/x`.
 *     2. Otherwise:
 *        1. Return `x`.
 * 11. If `hy` is equal to `0x40000000`:
 *     1. Return `x*x`.
 * 12. If `hy` is equal to `0x3f000000`:
 *     1. If `hx` is greater than or equal to `0`:
 *        1. Call `__ieee754_sqrtf()` with the argument `x`, and return its
 *           result.
 * 13. Call `fabsf()` with the argument `x`, and store the result in `ax`.
 * 14. If `ix` is equal to `0x7f800000` or `ix` is equal to `0` or `ix` is equal
 *     to `0x3f800000`:
 *     1. Set `z` to `ax`.
 *     2. If `hy` is less than `0`:
 *        1. Set `z` to `one/z`.
 *     3. If `hx` is less than `0`:
 *        1. If `((ix-0x3f800000)|yisint)` is equal to `0`:
 *           1. Set `z` to `(z-z)/(z-z)`.
 *        2. Otherwise, if `yisint` is equal to `1`:
 *           1. Set `z` to `-z`.
 *     4. Return `z`.
 * 15. If `((((uint32_t)hx>>31)-1)|yisint)` is equal to `0`:
 *     1. Return `(x-x)/(x-x)`.
 * 16. If `iy` is greater than `0x4d000000`:
 *     1. If `ix` is less than `0x3f7ffff8`:
 *        1. Return `huge*huge` if `hy` is less than `0`, otherwise `tiny*tiny`.
 *     2. If `ix` is greater than `0x3f800007`:
 *        1. Return `huge*huge` if `hy` is greater than `0`, otherwise
 *           `tiny*tiny`.
 *     3. Set `t` to `ax-one`.
 *     4. Set `w` to
 *        `(t*t)*((float)0.5-t*((float)0.333333333333-t*(float)0.25))`.
 *     5. Set `u` to `ivln2_h*t`.
 *     6. Set `v` to `t*ivln2_l-w*ivln2`.
 *     7. Set `t1` to `u+v`.
 *     8. Use the macro `GET_FLOAT_WORD()` with the arguments `is` and `t1`.
 *     9. Use the macro `SET_FLOAT_WORD()` with the arguments `t1` and
 *        `is&0xfffff000`.
 *     10. Set `t2` to `v-(t1-u)`.
 * 17. Otherwise:
 *     1. Set `n` to `0`.
 *     2. If `ix` is less than `0x00800000`:
 *        1. Multiply `ax` by `two24`.
 *        2. Subtract `24` from `n`.
 *        3. Use the macro `GET_FLOAT_WORD()` with the arguments `ix` and `ax`.
 *     3. Add `((ix)>>23)-0x7f` to `n`.
 *     4. Set `j` to `ix&0x007fffff`.
 *     5. Set `ix` to `j|0x3f800000`.
 *     6. If `j` is less than or equal to `0x1cc471`:
 *        1. Set `k` to `0`.
 *     7. Otherwise, if `j` is less than `0x5db3d7`:
 *        1. Set `k` to `1`.
 *     8. Otherwise:
 *        1. Set `k` to `0`.
 *        2. Add `1` to `n`.
 *        3. Subtract `0x00800000` from `ix`.
 *     9. Use the macro `SET_FLOAT_WORD()` with the arguments `ax` and `ix`.
 *     10. Set `u` to `ax-bp[k]`.
 *     11. Set `v` to `one/(ax+bp[k])`.
 *     12. Set `s` to `u*v`.
 *     13. Set `s_h` to `s`.
 *     14. Use the macro `GET_FLOAT_WORD()` with the arguments `is` and `s_h`.
 *     15. Use the macro `SET_FLOAT_WORD()` with the arguments `s_h` and
 *         `is&0xfffff000`.
 *     16. Use the macro `SET_FLOAT_WORD()` with the arguments `t_h` and
 *         `((ix>>1)|0x20000000)+0x0040000+(k<<21)`.
 *     17. Set `t_l` to `ax - (t_h-bp[k])`.
 *     18. Set `s_l` to `v*((u-s_h*t_h)-s_h*t_l)`.
 *     19. Set `s2` to `s*s`.
 *     20. Set `r` to `s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))))`.
 *     21. Add `s_l*(s_h+s)` to `r`.
 *     22. Set `s2` to `s_h*s_h`.
 *     23. Set `t_h` to `(float)3.0+s2+r`.
 *     24. Use the macro `GET_FLOAT_WORD()` with the arguments `is` and `t_h`.
 *     25. Use the macro `SET_FLOAT_WORD()` with the arguments `t_h` and
 *         `is&0xfffff000`.
 *     26. Set `t_l` to `r-((t_h-(float)3.0)-s2)`.
 *     27. Set `u` to `s_h*t_h`.
 *     28. Set `v` to `s_l*t_h+t_l*s`.
 *     29. Set `p_h` to `u+v`.
 *     30. Use the macro `GET_FLOAT_WORD()` with the arguments `is` and `p_h`.
 *     31. Use the macro `SET_FLOAT_WORD()` with the arguments `p_h` and
 *         `is&0xfffff000`.
 *     32. Set `p_l` to `v-(p_h-u)`.
 *     33. Set `z_h` to `cp_h*p_h`.
 *     34. Set `z_l` to `cp_l*p_h+p_l*cp+dp_l[k]`.
 *     35. Set `t` to `(float)n`.
 *     36. Set `t1` to `(((z_h+z_l)+dp_h[k])+t)`.
 *     37. Use the macro `GET_FLOAT_WORD()` with the arguments `is` and `t1`.
 *     38. Use the macro `SET_FLOAT_WORD()` with the arguments `t1` and
 *         `is&0xfffff000`.
 *     39. Set `t2` to `z_l-(((t1-t)-dp_h[k])-z_h)`.
 * 18. Set `s` to `one`.
 * 19. If `((((uint32_t)hx>>31)-1)|(yisint-1))` is equal to `0`:
 *     1. Set `s` to `-one`.
 * 20. Use the macro `GET_FLOAT_WORD()` with the arguments `is` and `y`.
 * 21. Use the macro `SET_FLOAT_WORD()` with the arguments `yy1` and
 *     `is&0xfffff000`.
 * 22. Set `p_l` to `(y-yy1)*t1+y*t2`.
 * 23. Set `p_h` to `yy1*t1`.
 * 24. Set `z` to `p_l+p_h`.
 * 25. Use the macro `GET_FLOAT_WORD()` with the arguments `j` and `z`.
 * 26. If `j` is greater than `0x43000000`:
 *     1. Return `s*huge*huge`.
 * 27. Otherwise, if `j` is equal to `0x43000000`:
 *     1. If `p_l+ovt` is greater than `z-p_h`:
 *        1. Return `s*huge*huge`.
 * 28. Otherwise, if `(uint32_t)j` is equal to `0xc3160000`:
 *     1. If `p_l` is less than or equal to `z-p_h`:
 *        1. Return `s*tiny*tiny`.
 * 29. Otherwise, if `(j&0x7fffffff)` is greater than `0x43160000`:
 *     1. Return `s*tiny*tiny`.
 * 30. Set `i` to `j&0x7fffffff`.
 * 31. Set `k` to `(i>>23)-0x7f`.
 * 32. Set `n` to `0`.
 * 33. If `i` is greater than `0x3f000000`:
 *     1. Set `n` to `j+(0x00800000>>(k+1))`.
 *     2. Set `k` to `((n&0x7fffffff)>>23)-0x7f`.
 *     3. Use the macro `SET_FLOAT_WORD()` with the arguments `t` and
 *        `n&~(0x007fffff>>k)`.
 *     4. Set `n` to `((n&0x007fffff)|0x00800000)>>(23-k)`.
 *     5. If `j` is less than `0`:
 *        1. Set `n` to `-n`.
 *     6. Subtract `t` from `p_h`.
 * 34. Set `t` to `p_l+p_h`.
 * 35. Use the macro `GET_FLOAT_WORD()` with the arguments `is` and `t`.
 * 36. Use the macro `SET_FLOAT_WORD()` with the arguments `t` and
 *     `is&0xfffff000`.
 * 37. Set `u` to `t*lg2_h`.
 * 38. Set `v` to `(p_l-(t-p_h))*lg2+t*lg2_l`.
 * 39. Set `z` to `u+v`.
 * 40. Set `w` to `v-(z-u)`.
 * 41. Set `t` to `z*z`.
 * 42. Set `t1` to `z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))))`.
 * 43. Set `r` to `(z*t1)/(t1-two)-(w+z*w)`.
 * 44. Set `z` to `one-(r-z)`.
 * 45. Use the macro `GET_FLOAT_WORD()` with the arguments `j` and `z`.
 * 46. Add `(n<<23)` to `j`.
 * 47. If `(j>>23)` is less than or equal to `0`:
 *     1. Call `scalbnf()` with the arguments `z` and `n`, and store the result
 *        in `z`.
 * 48. Otherwise:
 *     1. Use the macro `SET_FLOAT_WORD()` with the arguments `z` and `j`.
 * 49. Return `s*z`.
 *
 * @calls
 * - `__ieee754_sqrtf()`
 * - `fabsf()`
 * - `scalbnf()`
 */
float
__ieee754_powf(float x, float y)
{
	float z,ax,z_h,z_l,p_h,p_l;
	float yy1,t1,t2,r,s,t,u,v,w;
	int32_t i,j,k,yisint,n;
	int32_t hx,hy,ix,iy,is;

	GET_FLOAT_WORD(hx,x);
	GET_FLOAT_WORD(hy,y);
	ix = hx&0x7fffffff;  iy = hy&0x7fffffff;

    /* y==zero: x**0 = 1 */
	if(iy==0) return one;

    /* +-NaN return x+y */
	if(ix > 0x7f800000 ||
	   iy > 0x7f800000)
		return x+y;

    /* determine if y is an odd int when x < 0
     * yisint = 0	... y is not an integer
     * yisint = 1	... y is an odd int
     * yisint = 2	... y is an even int
     */
	yisint  = 0;
	if(hx<0) {
	    if(iy>=0x4b800000) yisint = 2; /* even integer y */
	    else if(iy>=0x3f800000) {
		k = (iy>>23)-0x7f;	   /* exponent */
		j = iy>>(23-k);
		if((j<<(23-k))==iy) yisint = 2-(j&1);
	    }
	}

    /* special value of y */
	if (iy==0x7f800000) {	/* y is +-inf */
	    if (ix==0x3f800000)
	        return  y - y;	/* inf**+-1 is NaN */
	    else if (ix > 0x3f800000)/* (|x|>1)**+-inf = inf,0 */
	        return (hy>=0)? y: zero;
	    else			/* (|x|<1)**-,+inf = inf,0 */
	        return (hy<0)?-y: zero;
	}
	if(iy==0x3f800000) {	/* y is  +-1 */
	    if(hy<0) return one/x; else return x;
	}
	if(hy==0x40000000) return x*x; /* y is  2 */
	if(hy==0x3f000000) {	/* y is  0.5 */
	    if(hx>=0)	/* x >= +0 */
	    return __ieee754_sqrtf(x);
	}

	ax   = fabsf(x);
    /* special value of x */
	if(ix==0x7f800000||ix==0||ix==0x3f800000){
	    z = ax;			/*x is +-0,+-inf,+-1*/
	    if(hy<0) z = one/z;	/* z = (1/|x|) */
	    if(hx<0) {
		if(((ix-0x3f800000)|yisint)==0) {
		    z = (z-z)/(z-z); /* (-1)**non-int is NaN */
		} else if(yisint==1)
		    z = -z;		/* (x<0)**odd = -(|x|**odd) */
	    }
	    return z;
	}

    /* (x<0)**(non-int) is NaN */
	if(((((uint32_t)hx>>31)-1)|yisint)==0) return (x-x)/(x-x);

    /* |y| is huge */
	if(iy>0x4d000000) { /* if |y| > 2**27 */
	/* over/underflow if x is not close to one */
	    if(ix<0x3f7ffff8) return (hy<0)? huge*huge:tiny*tiny;
	    if(ix>0x3f800007) return (hy>0)? huge*huge:tiny*tiny;
	/* now |1-x| is tiny <= 2**-20, suffice to compute
	   log(x) by x-x^2/2+x^3/3-x^4/4 */
	    t = ax-one;		/* t has 20 trailing zeros */
	    w = (t*t)*((float)0.5-t*((float)0.333333333333-t*(float)0.25));
	    u = ivln2_h*t;	/* ivln2_h has 16 sig. bits */
	    v = t*ivln2_l-w*ivln2;
	    t1 = u+v;
	    GET_FLOAT_WORD(is,t1);
	    SET_FLOAT_WORD(t1,is&0xfffff000);
	    t2 = v-(t1-u);
	} else {
	    float s2,s_h,s_l,t_h,t_l;
	    n = 0;
	/* take care subnormal number */
	    if(ix<0x00800000)
		{ax *= two24; n -= 24; GET_FLOAT_WORD(ix,ax); }
	    n  += ((ix)>>23)-0x7f;
	    j  = ix&0x007fffff;
	/* determine interval */
	    ix = j|0x3f800000;		/* normalize ix */
	    if(j<=0x1cc471) k=0;	/* |x|<sqrt(3/2) */
	    else if(j<0x5db3d7) k=1;	/* |x|<sqrt(3)   */
	    else {k=0;n+=1;ix -= 0x00800000;}
	    SET_FLOAT_WORD(ax,ix);

	/* compute s = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
	    u = ax-bp[k];		/* bp[0]=1.0, bp[1]=1.5 */
	    v = one/(ax+bp[k]);
	    s = u*v;
	    s_h = s;
	    GET_FLOAT_WORD(is,s_h);
	    SET_FLOAT_WORD(s_h,is&0xfffff000);
	/* t_h=ax+bp[k] High */
	    SET_FLOAT_WORD(t_h,((ix>>1)|0x20000000)+0x0040000+(k<<21));
	    t_l = ax - (t_h-bp[k]);
	    s_l = v*((u-s_h*t_h)-s_h*t_l);
	/* compute log(ax) */
	    s2 = s*s;
	    r = s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))));
	    r += s_l*(s_h+s);
	    s2  = s_h*s_h;
	    t_h = (float)3.0+s2+r;
	    GET_FLOAT_WORD(is,t_h);
	    SET_FLOAT_WORD(t_h,is&0xfffff000);
	    t_l = r-((t_h-(float)3.0)-s2);
	/* u+v = s*(1+...) */
	    u = s_h*t_h;
	    v = s_l*t_h+t_l*s;
	/* 2/(3log2)*(s+...) */
	    p_h = u+v;
	    GET_FLOAT_WORD(is,p_h);
	    SET_FLOAT_WORD(p_h,is&0xfffff000);
	    p_l = v-(p_h-u);
	    z_h = cp_h*p_h;		/* cp_h+cp_l = 2/(3*log2) */
	    z_l = cp_l*p_h+p_l*cp+dp_l[k];
	/* log2(ax) = (s+..)*2/(3*log2) = n + dp_h + z_h + z_l */
	    t = (float)n;
	    t1 = (((z_h+z_l)+dp_h[k])+t);
	    GET_FLOAT_WORD(is,t1);
	    SET_FLOAT_WORD(t1,is&0xfffff000);
	    t2 = z_l-(((t1-t)-dp_h[k])-z_h);
	}

	s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
	if(((((uint32_t)hx>>31)-1)|(yisint-1))==0)
	    s = -one;	/* (-ve)**(odd int) */

    /* split up y into yy1+y2 and compute (yy1+y2)*(t1+t2) */
	GET_FLOAT_WORD(is,y);
	SET_FLOAT_WORD(yy1,is&0xfffff000);
	p_l = (y-yy1)*t1+y*t2;
	p_h = yy1*t1;
	z = p_l+p_h;
	GET_FLOAT_WORD(j,z);
	if (j>0x43000000)				/* if z > 128 */
	    return s*huge*huge;				/* overflow */
	else if (j==0x43000000) {			/* if z == 128 */
	    if(p_l+ovt>z-p_h) return s*huge*huge;	/* overflow */
	}
	else if ((uint32_t)j==0xc3160000){		/* z == -150 */
	    if(p_l<=z-p_h) return s*tiny*tiny;		/* underflow */
	}
	else if ((j&0x7fffffff)>0x43160000)		/* z <= -150 */
	    return s*tiny*tiny;				/* underflow */
    /*
     * compute 2**(p_h+p_l)
     */
	i = j&0x7fffffff;
	k = (i>>23)-0x7f;
	n = 0;
	if(i>0x3f000000) {		/* if |z| > 0.5, set n = [z+0.5] */
	    n = j+(0x00800000>>(k+1));
	    k = ((n&0x7fffffff)>>23)-0x7f;	/* new k for n */
	    SET_FLOAT_WORD(t,n&~(0x007fffff>>k));
	    n = ((n&0x007fffff)|0x00800000)>>(23-k);
	    if(j<0) n = -n;
	    p_h -= t;
	}
	t = p_l+p_h;
	GET_FLOAT_WORD(is,t);
	SET_FLOAT_WORD(t,is&0xfffff000);
	u = t*lg2_h;
	v = (p_l-(t-p_h))*lg2+t*lg2_l;
	z = u+v;
	w = v-(z-u);
	t  = z*z;
	t1  = z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
	r  = (z*t1)/(t1-two)-(w+z*w);
	z  = one-(r-z);
	GET_FLOAT_WORD(j,z);
	j += (n<<23);
	if((j>>23)<=0) z = scalbnf(z,n);	/* subnormal output */
	else SET_FLOAT_WORD(z,j);
	return s*z;
}

#endif
