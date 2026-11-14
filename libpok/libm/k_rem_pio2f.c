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

/* k_rem_pio2f.c -- float version of k_rem_pio2.c
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
 * `__kernel_rem_pio2f()` of libpok's math library (fdlibm, from Sun
 * Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"

/* In the float version, the input parameter x contains 8 bit
   integers, not 24 bit integers.  113 bit precision is not supported.  */

static const int init_jk[] = {4,7,9}; /* initial value for jk */

static const float PIo2[] = {
  1.5703125000e+00, /* 0x3fc90000 */
  4.5776367188e-04, /* 0x39f00000 */
  2.5987625122e-05, /* 0x37da0000 */
  7.5437128544e-08, /* 0x33a20000 */
  6.0026650317e-11, /* 0x2e840000 */
  7.3896444519e-13, /* 0x2b500000 */
  5.3845816694e-15, /* 0x27c20000 */
  5.6378512969e-18, /* 0x22d00000 */
  8.3009228831e-20, /* 0x1fc40000 */
  3.2756352257e-22, /* 0x1bc60000 */
  6.3331015649e-25, /* 0x17440000 */
};

static const float
zero   = 0.0,
one    = 1.0,
two8   =  2.5600000000e+02, /* 0x43800000 */
twon8  =  3.9062500000e-03; /* 0x3b800000 */

/**
 * @funcname `__kernel_rem_pio2f`
 *
 * @description
 * Reduces a large argument, given as 8-bit pieces, modulo pi/2 (Payne-Hanek).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float *` | [in] The pieces of the argument. |
 * | `y` | `float *` | [out] Where the result is stored (head and tail). |
 * | `e0` | `int` | [in] The exponent of x[0]. |
 * | `nx` | `int` | [in] The number of pieces. |
 * | `prec` | `int` | [in] The precision (0: float, 1: double, 2: extended, 3: quad). |
 * | `ipio2` | `const int *` | [in] The bits of 2/pi. |
 *
 * @return The last three bits of the quotient.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `PIo2` | `const float[11]` | A constant of the algorithm |
 * | `init_jk` | `const int[3]` | A constant of the algorithm |
 * | `one` | `const float` | A constant of the algorithm |
 * | `two8` | `const float` | A constant of the algorithm |
 * | `twon8` | `const float` | A constant of the algorithm |
 * | `zero` | `const float` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `carry` | `int32_t` | A temporary of the computation |
 * | `f` | `float[20]` | A temporary of the computation |
 * | `fq` | `float[20]` | A temporary of the computation |
 * | `fw` | `float` | A temporary of the computation |
 * | `i` | `int32_t` | A temporary of the computation |
 * | `ih` | `int32_t` | A temporary of the computation |
 * | `iq` | `int32_t[20]` | A temporary of the computation |
 * | `j` | `int32_t` | A temporary of the computation |
 * | `jk` | `int32_t` | A temporary of the computation |
 * | `jp` | `int32_t` | A temporary of the computation |
 * | `jv` | `int32_t` | A temporary of the computation |
 * | `jx` | `int32_t` | A temporary of the computation |
 * | `jz` | `int32_t` | A temporary of the computation |
 * | `k` | `int32_t` | A temporary of the computation |
 * | `m` | `int32_t` | A temporary of the computation |
 * | `n` | `int32_t` | A temporary of the computation |
 * | `q` | `float[20]` | A temporary of the computation |
 * | `q0` | `int32_t` | A temporary of the computation |
 * | `z` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `jk` to `init_jk[prec]`.
 * 2. Set `jp` to `jk`.
 * 3. Set `jx` to `nx-1`.
 * 4. Set `jv` to `(e0-3)/8`.
 * 5. If `jv` is less than `0`:
 *    1. Set `jv` to `0`.
 * 6. Set `q0` to `e0-8*(jv+1)`.
 * 7. Set `j` to `jv-jx`.
 * 8. Set `m` to `jx+jk`.
 * 9. Set `i` to `0`; then while `i` is less than or equal to `m`, repeat
 *    (after each pass: increment `i`. Increment `j`):
 *    1. Set `f[i]` to `zero` if `j` is less than `0`, otherwise
 *       `(float) ipio2[j]`.
 * 10. Set `i` to `0`; then while `i` is less than or equal to `jk`, repeat
 *     (after each pass: increment `i`):
 *     1. Set `j` to `0`. Set `fw` to `0.0`; then while `j` is less than or
 *        equal to `jx`, repeat (after each pass: increment `j`):
 *        1. Add `x[j]*f[jx+i-j]` to `fw`.
 *     2. Set `q[i]` to `fw`.
 * 11. Set `jz` to `jk`.
 * 12. Label `recompute`:
 * 13. Set `i` to `0`. Set `j` to `jz`. Set `z` to `q[jz]`; then while `j` is
 *     greater than `0`, repeat (after each pass: increment `i`. Decrement `j`):
 *     1. Set `fw` to `(float)((int32_t)(twon8* z))`.
 *     2. Set `iq[i]` to `(int32_t)(z-two8*fw)`.
 *     3. Set `z` to `q[j-1]+fw`.
 * 14. Call `scalbnf()` with the arguments `z` and `q0`, and store the result in
 *     `z`.
 * 15. Subtract `(float)8.0*floorf(z*(float)0.125)` from `z`.
 * 16. Set `n` to `(int32_t) z`.
 * 17. Subtract `(float)n` from `z`.
 * 18. Set `ih` to `0`.
 * 19. If `q0` is greater than `0`:
 *     1. Set `i` to `(iq[jz-1]>>(8-q0))`.
 *     2. Add `i` to `n`.
 *     3. Subtract `i<<(8-q0)` from `iq[jz-1]`.
 *     4. Set `ih` to `iq[jz-1]>>(7-q0)`.
 * 20. Otherwise, if `q0` is equal to `0`:
 *     1. Set `ih` to `iq[jz-1]>>8`.
 * 21. Otherwise, if `z` is greater than or equal to `(float)0.5`:
 *     1. Set `ih` to `2`.
 * 22. If `ih` is greater than `0`:
 *     1. Add `1` to `n`.
 *     2. Set `carry` to `0`.
 *     3. Set `i` to `0`; then while `i` is less than `jz`, repeat (after each
 *        pass: increment `i`):
 *        1. Set `j` to `iq[i]`.
 *        2. If `carry` is equal to `0`:
 *           1. If `j` is not equal to `0`:
 *              1. Set `carry` to `1`.
 *              2. Set `iq[i]` to `0x100- j`.
 *        3. Otherwise:
 *           1. Set `iq[i]` to `0xff - j`.
 *     4. If `q0` is greater than `0`:
 *        1. Depending on `q0`:
 *           1. Case `1`:
 *              1. Keep only the bits `0x7f` of `iq[jz-1]`.
 *              2. Leave the switch.
 *           2. Case `2`:
 *              1. Keep only the bits `0x3f` of `iq[jz-1]`.
 *              2. Leave the switch.
 *     5. If `ih` is equal to `2`:
 *        1. Set `z` to `one - z`.
 *        2. If `carry` is not equal to `0`:
 *           1. Subtract `scalbnf(one,q0)` from `z`.
 * 23. If `z` is equal to `zero`:
 *     1. Set `j` to `0`.
 *     2. Set `i` to `jz-1`; then while `i` is greater than or equal to `jk`,
 *        repeat (after each pass: decrement `i`):
 *        1. Set the bits `iq[i]` in `j`.
 *     3. If `j` is equal to `0`:
 *        1. Set `k` to `1`; then while `iq[jk-k]` is equal to `0`, repeat
 *           (after each pass: increment `k`):
 *           1. Do nothing.
 *        2. Set `i` to `jz+1`; then while `i` is less than or equal to `jz+k`,
 *           repeat (after each pass: increment `i`):
 *           1. Set `f[jx+i]` to `(float) ipio2[jv+i]`.
 *           2. Set `j` to `0`. Set `fw` to `0.0`; then while `j` is less than
 *              or equal to `jx`, repeat (after each pass: increment `j`):
 *              1. Add `x[j]*f[jx+i-j]` to `fw`.
 *           3. Set `q[i]` to `fw`.
 *        3. Add `k` to `jz`.
 *        4. Go to the label `recompute`.
 * 24. If `z` is equal to `(float)0.0`:
 *     1. Subtract `1` from `jz`.
 *     2. Subtract `8` from `q0`.
 *     3. While `iq[jz]` is equal to `0`:
 *        1. Decrement `jz`.
 *        2. Subtract `8` from `q0`.
 * 25. Otherwise:
 *     1. Call `scalbnf()` with the arguments `z` and `-q0`, and store the
 *        result in `z`.
 *     2. If `z` is greater than or equal to `two8`:
 *        1. Set `fw` to `(float)((int32_t)(twon8*z))`.
 *        2. Set `iq[jz]` to `(int32_t)(z-two8*fw)`.
 *        3. Add `1` to `jz`.
 *        4. Add `8` to `q0`.
 *        5. Set `iq[jz]` to `(int32_t) fw`.
 *     3. Otherwise:
 *        1. Set `iq[jz]` to `(int32_t) z`.
 * 26. Call `scalbnf()` with the arguments `one` and `q0`, and store the result
 *     in `fw`.
 * 27. Set `i` to `jz`; then while `i` is greater than or equal to `0`, repeat
 *     (after each pass: decrement `i`):
 *     1. Set `q[i]` to `fw*(float)iq[i]`.
 *     2. Multiply `fw` by `twon8`.
 * 28. Set `i` to `jz`; then while `i` is greater than or equal to `0`, repeat
 *     (after each pass: decrement `i`):
 *     1. Set `fw` to `0.0`. Set `k` to `0`; then while `k` is less than or
 *        equal to `jp` and `k` is less than or equal to `jz-i`, repeat (after
 *        each pass: increment `k`):
 *        1. Add `PIo2[k]*q[i+k]` to `fw`.
 *     2. Set `fq[jz-i]` to `fw`.
 * 29. Depending on `prec`:
 *     1. Case `0`:
 *        1. Set `fw` to `0.0`.
 *        2. Set `i` to `jz`; then while `i` is greater than or equal to `0`,
 *           repeat (after each pass: decrement `i`):
 *           1. Add `fq[i]` to `fw`.
 *        3. Set `y[0]` to `fw` if `ih` is equal to `0`, otherwise `-fw`.
 *        4. Leave the switch.
 *     2. Case `1` or `2`:
 *        1. Set `fw` to `0.0`.
 *        2. Set `i` to `jz`; then while `i` is greater than or equal to `0`,
 *           repeat (after each pass: decrement `i`):
 *           1. Add `fq[i]` to `fw`.
 *        3. Set `y[0]` to `fw` if `ih` is equal to `0`, otherwise `-fw`.
 *        4. Set `fw` to `fq[0]-fw`.
 *        5. Set `i` to `1`; then while `i` is less than or equal to `jz`,
 *           repeat (after each pass: increment `i`):
 *           1. Add `fq[i]` to `fw`.
 *        6. Set `y[1]` to `fw` if `ih` is equal to `0`, otherwise `-fw`.
 *        7. Leave the switch.
 *     3. Case `3`:
 *        1. Set `i` to `jz`; then while `i` is greater than `0`, repeat (after
 *           each pass: decrement `i`):
 *           1. Set `fw` to `fq[i-1]+fq[i]`.
 *           2. Add `fq[i-1]-fw` to `fq[i]`.
 *           3. Set `fq[i-1]` to `fw`.
 *        2. Set `i` to `jz`; then while `i` is greater than `1`, repeat (after
 *           each pass: decrement `i`):
 *           1. Set `fw` to `fq[i-1]+fq[i]`.
 *           2. Add `fq[i-1]-fw` to `fq[i]`.
 *           3. Set `fq[i-1]` to `fw`.
 *        3. Set `fw` to `0.0`. Set `i` to `jz`; then while `i` is greater than
 *           or equal to `2`, repeat (after each pass: decrement `i`):
 *           1. Add `fq[i]` to `fw`.
 *        4. If `ih` is equal to `0`:
 *           1. Set `y[0]` to `fq[0]`.
 *           2. Set `y[1]` to `fq[1]`.
 *           3. Set `y[2]` to `fw`.
 *        5. Otherwise:
 *           1. Set `y[0]` to `-fq[0]`.
 *           2. Set `y[1]` to `-fq[1]`.
 *           3. Set `y[2]` to `-fw`.
 * 30. Return `n&7`.
 *
 * @calls
 * - `scalbnf()`
 * - `floorf()`
 */
int
__kernel_rem_pio2f(float *x, float *y, int e0, int nx, int prec, const int *ipio2)
{
	int32_t jz,jx,jv,jp,jk,carry,n,iq[20],i,j,k,m,q0,ih;
	float z,fw,f[20],fq[20],q[20];

    /* initialize jk*/
	jk = init_jk[prec];
	jp = jk;

    /* determine jx,jv,q0, note that 3>q0 */
	jx =  nx-1;
	jv = (e0-3)/8; if(jv<0) jv=0;
	q0 =  e0-8*(jv+1);

    /* set up f[0] to f[jx+jk] where f[jx+jk] = ipio2[jv+jk] */
	j = jv-jx; m = jx+jk;
	for(i=0;i<=m;i++,j++) f[i] = (j<0)? zero : (float) ipio2[j];

    /* compute q[0],q[1],...q[jk] */
	for (i=0;i<=jk;i++) {
	    for(j=0,fw=0.0;j<=jx;j++)
            fw += x[j]*f[jx+i-j];
        q[i] = fw;
	}

	jz = jk;
recompute:
    /* distill q[] into iq[] reversingly */
	for(i=0,j=jz,z=q[jz];j>0;i++,j--) {
	    fw    =  (float)((int32_t)(twon8* z));
	    iq[i] =  (int32_t)(z-two8*fw);
	    z     =  q[j-1]+fw;
	}

    /* compute n */
	z  = scalbnf(z,q0);		/* actual value of z */
	z -= (float)8.0*floorf(z*(float)0.125);	/* trim off integer >= 8 */
	n  = (int32_t) z;
	z -= (float)n;
	ih = 0;
	if(q0>0) {	/* need iq[jz-1] to determine n */
	    i  = (iq[jz-1]>>(8-q0)); n += i;
	    iq[jz-1] -= i<<(8-q0);
	    ih = iq[jz-1]>>(7-q0);
	}
	else if(q0==0) ih = iq[jz-1]>>8;
	else if(z>=(float)0.5) ih=2;

	if(ih>0) {	/* q > 0.5 */
	    n += 1; carry = 0;
	    for(i=0;i<jz ;i++) {	/* compute 1-q */
		j = iq[i];
		if(carry==0) {
		    if(j!=0) {
			carry = 1; iq[i] = 0x100- j;
		    }
		} else  iq[i] = 0xff - j;
	    }
	    if(q0>0) {		/* rare case: chance is 1 in 12 */
	        switch(q0) {
	        case 1:
	    	   iq[jz-1] &= 0x7f; break;
	    	case 2:
	    	   iq[jz-1] &= 0x3f; break;
	        }
	    }
	    if(ih==2) {
		z = one - z;
		if(carry!=0) z -= scalbnf(one,q0);
	    }
	}

    /* check if recomputation is needed */
	if(z==zero) {
	    j = 0;
	    for (i=jz-1;i>=jk;i--) j |= iq[i];
	    if(j==0) { /* need recomputation */
		for(k=1;iq[jk-k]==0;k++);   /* k = no. of terms needed */

		for(i=jz+1;i<=jz+k;i++) {   /* add q[jz+1] to q[jz+k] */
		    f[jx+i] = (float) ipio2[jv+i];
		    for(j=0,fw=0.0;j<=jx;j++) fw += x[j]*f[jx+i-j];
		    q[i] = fw;
		}
		jz += k;
		goto recompute;
	    }
	}

    /* chop off zero terms */
	if(z==(float)0.0) {
	    jz -= 1; q0 -= 8;
	    while(iq[jz]==0) { jz--; q0-=8;}
	} else { /* break z into 8-bit if necessary */
	    z = scalbnf(z,-q0);
	    if(z>=two8) {
		fw = (float)((int32_t)(twon8*z));
		iq[jz] = (int32_t)(z-two8*fw);
		jz += 1; q0 += 8;
		iq[jz] = (int32_t) fw;
	    } else iq[jz] = (int32_t) z ;
	}

    /* convert integer "bit" chunk to floating-point value */
	fw = scalbnf(one,q0);
	for(i=jz;i>=0;i--) {
	    q[i] = fw*(float)iq[i]; fw*=twon8;
	}

    /* compute PIo2[0,...,jp]*q[jz,...,0] */
	for(i=jz;i>=0;i--) {
	    for(fw=0.0,k=0;k<=jp&&k<=jz-i;k++) fw += PIo2[k]*q[i+k];
	    fq[jz-i] = fw;
	}

    /* compress fq[] into y[] */
	switch(prec) {
	    case 0:
		fw = 0.0;
		for (i=jz;i>=0;i--) fw += fq[i];
		y[0] = (ih==0)? fw: -fw;
		break;
	    case 1:
	    case 2:
		fw = 0.0;
		for (i=jz;i>=0;i--) fw += fq[i];
		y[0] = (ih==0)? fw: -fw;
		fw = fq[0]-fw;
		for (i=1;i<=jz;i++) fw += fq[i];
		y[1] = (ih==0)? fw: -fw;
		break;
	    case 3:	/* painful */
		for (i=jz;i>0;i--) {
		    fw      = fq[i-1]+fq[i];
		    fq[i]  += fq[i-1]-fw;
		    fq[i-1] = fw;
		}
		for (i=jz;i>1;i--) {
		    fw      = fq[i-1]+fq[i];
		    fq[i]  += fq[i-1]-fw;
		    fq[i-1] = fw;
		}
		for (fw=0.0,i=jz;i>=2;i--) fw += fq[i];
		if(ih==0) {
		    y[0] =  fq[0]; y[1] =  fq[1]; y[2] =  fw;
		} else {
		    y[0] = -fq[0]; y[1] = -fq[1]; y[2] = -fw;
		}
	}
	return n&7;
}

#endif
