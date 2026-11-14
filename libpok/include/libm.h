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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#include <config.h>
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#ifndef __POK_LIBM_H__
#define __POK_LIBM_H__


#include <types.h>

/**
 * @typename `struct exception`
 *
 * @description
 * An exception of a mathematical function, given to matherr() (the SVID
 * interface, which needs the tag).
 */
struct exception {
        /**
         * Its type (DOMAIN, SING, OVERFLOW, UNDERFLOW, TLOSS or PLOSS).
         */
        int type;
        char *name; /**< The name of the function. */
        double arg1; /**< Its first argument. */
        double arg2; /**< Its second argument. */
        /**
         * The value the function returns (matherr() may change it).
         */
        double retval;
};

/**
 * @macroname `FP_NAN`
 *
 * @description
 * fpclassify(): not a number.
 */
#define FP_NAN          1

/**
 * @macroname `FP_INFINITE`
 *
 * @description
 * fpclassify(): an infinity.
 */
#define FP_INFINITE     2

/**
 * @macroname `FP_NORMAL`
 *
 * @description
 * fpclassify(): a normal number.
 */
#define FP_NORMAL       3

/**
 * @macroname `FP_SUBNORMAL`
 *
 * @description
 * fpclassify(): a subnormal number.
 */
#define FP_SUBNORMAL    4

/**
 * @macroname `FP_ZERO`
 *
 * @description
 * fpclassify(): zero.
 */
#define FP_ZERO         5

/**
 * @macroname `DOMAIN`
 *
 * @description
 * The type of a struct exception: an argument out of the domain of the
 * function.
 */
#define DOMAIN          1

/**
 * @macroname `SING`
 *
 * @description
 * The type of a struct exception: a singularity.
 */
#define SING            2

/**
 * @macroname `OVERFLOW`
 *
 * @description
 * The type of a struct exception: an overflow.
 */
#define OVERFLOW        3

/**
 * @macroname `UNDERFLOW`
 *
 * @description
 * The type of a struct exception: an underflow.
 */
#define UNDERFLOW       4

/**
 * @macroname `TLOSS`
 *
 * @description
 * The type of a struct exception: a total loss of significance.
 */
#define TLOSS           5

/**
 * @macroname `PLOSS`
 *
 * @description
 * The type of a struct exception: a partial loss of significance.
 */
#define PLOSS           6


/**
 * @macroname `fpclassify`
 *
 * @description
 * The class of a floating point number (FP_NAN, FP_INFINITE, FP_NORMAL,
 * FP_SUBNORMAL or FP_ZERO): __fpclassifyf() for a float, __fpclassifyd()
 * otherwise.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `x` | The number (float or double). |
 */
#define fpclassify(x) (sizeof (x) == sizeof(float )  ?  __fpclassifyf((float)(x)) : __fpclassifyd((double)(x)))

/** \cond */
extern int __fpclassifyf(float      );
extern int __fpclassifyd(double     );
extern int __fpclassify (long double);


double   acos(double x);
float    acosf(float x);
double   acosh(double x);
float    acoshf(float x);
double   asin(double x);
float    asinf(float x);
double   asinh(double x);
float    asinhf(float x);
double   atan(double x);
float    atanf(float x);
double   atan2(double y, double x);
float    atan2f(float y, float x);
double   atanh(double x);
float    atanhf(float x);
double   cbrt(double x);
float    cbrtf(float x);
double   ceil(double x);
float    ceilf(float x);
double   copysign(double x, double y);
float    copysignf(float x, float y);
double   cos(double x);
float    cosf(float x);
double   cosh(double x);
float    coshf(float x);
double   drem(double x, double y);
float    dremf(float x, float y);
double   erf(double x);
float    erff(float x);
double   exp(double x);
float    expf(float x);
double   expm1(double x);
float    expm1f(float x);
double   fabs(double x);
float    fabsf(float x);
int      finite(double x);
int      finitef(float x);
double   floor(double x);
float    floorf(float x);
double   frexp(double x, int *eptr);
float    frexpf(float x, int *eptr);
double   gamma(double x);
float    gammaf(float x);
double   gamma_r(double x, int *signgamp);
float    gammaf_r(float x, int *signgamp);
double   hypot(double x, double y);
float    hypotf(float x, float y);
int      ilogb(double x);
int      ilogbf(float x);
int      isinf(double x);
int      isinff(float x);
int      isnan(double x);
int      isnanf(float x);
double   j0(double x);
float    j0f(float x);
double   j1(double x);
float    j1f(float x);
double   jn(int n, double x);
float    jnf(int n, float x);
double   ldexp(double value, int exp0);
float    ldexpf(float value, int exp0);
double   lgamma(double x);
float    lgammaf(float x);
double   lgamma_r(double x, int *signgamp);
float    lgammaf_r(float x, int *signgamp);
double   log(double x);
float    logf(float x);
double   log10(double x);
float    log10f(float x);
double   log2(double x);
float    log2f(float x);
double   logb(double x);
float    logbf(float x);
double   log1p(double x);
float    log1pf(float x);
double   ldexp(double value, int exp0);
float    ldexpf(float value, int exp0);
int      matherr(struct exception *x);
float    modff(float x, float *iptr);
double   modf(double x, double *iptr);
double   nextafter(double x, double y);
float    nextafterf(float x, float y);
double   pow(double x, double y);
float    powf(float x, float y);
double   remainder(double x, double y);
float    remainderf(float x, float y);
/** \endcond */
#ifdef _SCALB_INT
/** \cond */
double   scalb(double x, int fn);
/** \endcond */
#else
/** \cond */
double   scalb(double x, double fn);
/** \endcond */
#endif
#ifdef _SCALB_INT
/** \cond */
float    scalbf(float x, int fn);
/** \endcond */
#else
/** \cond */
float    scalbf(float x, float fn);
/** \endcond */
#endif
/** \cond */
double   rint(double x);
float    rintf(float x);
double   round(double x);
float    roundf(float x);
double   scalbn(double x, int n);
float    scalbnf(float x, int n);
double   significand(double x);
float    significandf(float x);
double   sin(double x);
float    sinf(float x);
double   sinh(double x);
float    sinhf(float x);
double   sqrt(double x);
float    sqrtf(float x);
double   tan(double x);
float    tanf(float x);
double   tanh(double x);
float    tanhf(float x);
double   trunc(double x);
float    truncf(float x);
/** \endcond */

#endif

#endif /* POK_NEEDS_LIBMATH */
