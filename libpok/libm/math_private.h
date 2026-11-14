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
 * Created by julien on Fri Jan 30 13:44:27 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of
 * libpok/libm/k_standard.c are moved here.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
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

/*
 * from: @(#)fdlibm.h 5.1 93/09/24
 * $NetBSD: math_private.h,v 1.13 2008/04/26 23:49:50 christos Exp $
 */

#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#ifndef _MATH_PRIVATE_H_
#define _MATH_PRIVATE_H_

#include <types.h>

/**
 * @macroname `_IEEE_LIBM`
 *
 * @description
 * The library follows IEEE 754 only (no SVID or POSIX error handling).
 */
#define _IEEE_LIBM 1

/**
 * @macroname `_LIB_VERSION`
 *
 * @description
 * The error handling of the library: _POSIX_.
 */
#define _LIB_VERSION   1

/**
 * @macroname `_POSIX_`
 *
 * @description
 * The POSIX error handling (_LIB_VERSION).
 */
#define _POSIX_        1

/**
 * @macroname `_SVID_`
 *
 * @description
 * The SVID error handling (_LIB_VERSION).
 */
#define _SVID_         2

/**
 * @macroname `_IEEE_`
 *
 * @description
 * The IEEE 754 error handling (_LIB_VERSION).
 */
#define _IEEE_         3

extern int signgam;

/**
 * @macroname `__P`
 *
 * @description
 * The prototype `protos` as it is (a full-blown ANSI C compiler).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `protos` | The parameter list of a prototype. |
 */
#define __P(protos)     protos


/* The original fdlibm code used statements like:
	n0 = ((*(int*)&one)>>29)^1;		* index of high word *
	ix0 = *(n0+(int*)&x);			* high word of x *
	ix1 = *((1-n0)+(int*)&x);		* low word of x *
   to dig two 32 bit words out of the 64 bit IEEE floating point
   value.  That is non-ANSI, and, moreover, the gcc instruction
   scheduler gets it wrong.  We instead use the following macros.
   Unlike the original code, we determine the endianness at compile
   time, not at run time; I don't see much benefit to selecting
   endianness at run time.  */

/* A union which permits us to convert between a double and two 32 bit
   ints.  */

/*
 * The ARM ports are little endian except for the FPA word order which is
 * big endian.
 */
/*
#if (BYTE_ORDER == BIG_ENDIAN) || (defined(__arm__) && !defined(__VFP_FP__))

typedef union
{
  double value;
  struct
  {
    uint32_t msw;
    uint32_t lsw;
  } parts;
} ieee_double_shape_type;

#endif
*/
#if (BYTE_ORDER == LITTLE_ENDIAN) && \
    !(defined(__arm__) && !defined(__VFP_FP__))

/**
 * @typename `ieee_double_shape_type`
 *
 * @description
 * A double and its two 32-bit words (on a little endian machine), to get or set
 * its bits without non-ANSI pointer casts.
 */
typedef union
{
  double value; /**< The double. */
  struct
  {
    uint32_t lsw;
    uint32_t msw;
  } parts;
  /**<
   * Its words: lsw, the less significant, and msw, the more significant.
   */
} ieee_double_shape_type;
#endif

/**
 * @macroname `EXTRACT_WORDS`
 *
 * @description
 * Gets the two 32-bit words of a double.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `ix0` | Receives the more significant word. |
 * | `ix1` | Receives the less significant word. |
 * | `d` | The double. |
 */
#define EXTRACT_WORDS(ix0,ix1,d)				\
do {								\
  ieee_double_shape_type ew_u;					\
  ew_u.value = (d);						\
  (ix0) = ew_u.parts.msw;					\
  (ix1) = ew_u.parts.lsw;					\
} while (/*CONSTCOND*/0)

/**
 * @macroname `GET_HIGH_WORD`
 *
 * @description
 * Gets the more significant 32-bit word of a double.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `i` | Receives the word. |
 * | `d` | The double. |
 */
#define GET_HIGH_WORD(i,d)					\
do {								\
  ieee_double_shape_type gh_u;					\
  gh_u.value = (d);						\
  (i) = gh_u.parts.msw;						\
} while (/*CONSTCOND*/0)

/**
 * @macroname `GET_LOW_WORD`
 *
 * @description
 * Gets the less significant 32-bit word of a double.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `i` | Receives the word. |
 * | `d` | The double. |
 */
#define GET_LOW_WORD(i,d)					\
do {								\
  ieee_double_shape_type gl_u;					\
  gl_u.value = (d);						\
  (i) = gl_u.parts.lsw;						\
} while (/*CONSTCOND*/0)

/**
 * @macroname `INSERT_WORDS`
 *
 * @description
 * Sets a double from two 32-bit words.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `d` | Receives the double. |
 * | `ix0` | The more significant word. |
 * | `ix1` | The less significant word. |
 */
#define INSERT_WORDS(d,ix0,ix1)					\
do {								\
  ieee_double_shape_type iw_u;					\
  iw_u.parts.msw = (ix0);					\
  iw_u.parts.lsw = (ix1);					\
  (d) = iw_u.value;						\
} while (/*CONSTCOND*/0)

/**
 * @macroname `SET_HIGH_WORD`
 *
 * @description
 * Sets the more significant 32-bit word of a double.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `d` | The double changed. |
 * | `v` | The word. |
 */
#define SET_HIGH_WORD(d,v)					\
do {								\
  ieee_double_shape_type sh_u;					\
  sh_u.value = (d);						\
  sh_u.parts.msw = (v);						\
  (d) = sh_u.value;						\
} while (/*CONSTCOND*/0)

/**
 * @macroname `SET_LOW_WORD`
 *
 * @description
 * Sets the less significant 32-bit word of a double.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `d` | The double changed. |
 * | `v` | The word. |
 */
#define SET_LOW_WORD(d,v)					\
do {								\
  ieee_double_shape_type sl_u;					\
  sl_u.value = (d);						\
  sl_u.parts.lsw = (v);						\
  (d) = sl_u.value;						\
} while (/*CONSTCOND*/0)

/**
 * @typename `ieee_float_shape_type`
 *
 * @description
 * A float and its 32-bit word, to get or set its bits.
 */
typedef union
{
  float value; /**< The float. */
  uint32_t word; /**< Its bits. */
} ieee_float_shape_type;

/**
 * @macroname `GET_FLOAT_WORD`
 *
 * @description
 * Gets the 32-bit word of a float.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `i` | Receives the word. |
 * | `d` | The float. |
 */
#define GET_FLOAT_WORD(i,d)					\
do {								\
  ieee_float_shape_type gf_u;					\
  gf_u.value = (d);						\
  (i) = gf_u.word;						\
} while (/*CONSTCOND*/0)

/**
 * @macroname `SET_FLOAT_WORD`
 *
 * @description
 * Sets a float from a 32-bit word.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `d` | Receives the float. |
 * | `i` | The word. |
 */
#define SET_FLOAT_WORD(d,i)					\
do {								\
  ieee_float_shape_type sf_u;					\
  sf_u.word = (i);						\
  (d) = sf_u.value;						\
} while (/*CONSTCOND*/0)

/* ieee style elementary functions */
/** \cond */
extern double __ieee754_sqrt __P((double));
extern double __ieee754_acos __P((double));
extern double __ieee754_acosh __P((double));
extern double __ieee754_log __P((double));
extern double __ieee754_atanh __P((double));
extern double __ieee754_asin __P((double));
extern double __ieee754_atan2 __P((double,double));
extern double __ieee754_exp __P((double));
extern double __ieee754_cosh __P((double));
extern double __ieee754_fmod __P((double,double));
extern double __ieee754_pow __P((double,double));
extern double __ieee754_lgamma_r __P((double,int *));
extern double __ieee754_gamma_r __P((double,int *));
extern double __ieee754_lgamma __P((double));
extern double __ieee754_gamma __P((double));
extern double __ieee754_log10 __P((double));
extern double __ieee754_log2 __P((double));
extern double __ieee754_sinh __P((double));
extern double __ieee754_hypot __P((double,double));
extern double __ieee754_j0 __P((double));
extern double __ieee754_j1 __P((double));
extern double __ieee754_y0 __P((double));
extern double __ieee754_y1 __P((double));
extern double __ieee754_jn __P((int,double));
extern double __ieee754_yn __P((int,double));
extern double __ieee754_remainder __P((double,double));
extern int32_t __ieee754_rem_pio2 __P((double,double*));
extern double __ieee754_scalb __P((double,double));

/* fdlibm kernel function */
extern double __kernel_standard __P((double,double,int));
extern double __kernel_sin __P((double,double,int));
extern double __kernel_cos __P((double,double));
extern double __kernel_tan __P((double,double,int));
extern int    __kernel_rem_pio2 __P((double*,double*,int,int,int,const int*));


/* ieee style elementary float functions */
extern float __ieee754_sqrtf __P((float));
extern float __ieee754_acosf __P((float));
extern float __ieee754_acoshf __P((float));
extern float __ieee754_logf __P((float));
extern float __ieee754_atanhf __P((float));
extern float __ieee754_asinf __P((float));
extern float __ieee754_atan2f __P((float,float));
extern float __ieee754_expf __P((float));
extern float __ieee754_coshf __P((float));
extern float __ieee754_fmodf __P((float,float));
extern float __ieee754_powf __P((float,float));
extern float __ieee754_lgammaf_r __P((float,int *));
extern float __ieee754_gammaf_r __P((float,int *));
extern float __ieee754_lgammaf __P((float));
extern float __ieee754_gammaf __P((float));
extern float __ieee754_log10f __P((float));
extern float __ieee754_log2f __P((float));
extern float __ieee754_sinhf __P((float));
extern float __ieee754_hypotf __P((float,float));
extern float __ieee754_j0f __P((float));
extern float __ieee754_j1f __P((float));
extern float __ieee754_y0f __P((float));
extern float __ieee754_y1f __P((float));
extern float __ieee754_jnf __P((int,float));
extern float __ieee754_ynf __P((int,float));
extern float __ieee754_remainderf __P((float,float));
extern int32_t   __ieee754_rem_pio2f __P((float,float*));
extern float __ieee754_scalbf __P((float,float));

/* float versions of fdlibm kernel functions */
extern float __kernel_sinf __P((float,float,int));
extern float __kernel_cosf __P((float,float));
extern float __kernel_tanf __P((float,float,int));
extern int   __kernel_rem_pio2f __P((float*,float*,int,int,int,const int*));
/** \endcond */

#ifndef _USE_WRITE
#ifdef POK_NEEDS_LIBC_STDIO
#include <libc/stdio.h>

/**
 * @macroname `WRITE2`
 *
 * @description
 * Writes a message of libm to the console: with printf(), which ignores its
 * length (POK_NEEDS_LIBC_STDIO, without _USE_WRITE).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `u` | The message. |
 * | `v` | Its length. |
 */
#define	WRITE2(u,v)	printf (u)
#else

/**
 * @macroname `WRITE2`
 *
 * @description
 * Writes a message of libm: does nothing (without POK_NEEDS_LIBC_STDIO and
 * _USE_WRITE).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `u` | The message. |
 * | `v` | Its length. |
 */
#define	WRITE2(u,v)
#endif /* POK_NEEDS_STDIO */
#else	/* !defined(_USE_WRITE) */
#include <unistd.h>			/* write */
/**
 * @macroname `WRITE2`
 *
 * @description
 * Writes a message of libm to the standard error output with write()
 * (_USE_WRITE).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `u` | The message. |
 * | `v` | Its length. |
 */
#define	WRITE2(u,v)	write(2, u, v)
#undef fflush
#endif	/* !defined(_USE_WRITE) */

/**
 * @macroname `POK_ERRNO_HUGE_VAL`
 *
 * @description
 * The result of __kernel_standard() for a value too large: an infinity, its
 * local variable inf, which it sets. This is the only routine that uses
 * POK_ERRNO_HUGE_VAL.
 */
#define POK_ERRNO_HUGE_VAL inf

#endif /* _MATH_PRIVATE_H_ */

#endif

