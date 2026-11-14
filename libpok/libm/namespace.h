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
 */

/* $NetBSD: namespace.h,v 1.2 2007/08/20 16:01:39 drochner Exp $ */

/*
 * #define atan2 _atan2
 * #define atan2f _atan2f
 */

/**
 * @macroname `hypot`
 *
 * @description
 * Renames hypot() to _hypot() (the NetBSD namespace of the library).
 */
#define hypot _hypot

/**
 * @macroname `hypotf`
 *
 * @description
 * Renames hypotf() to _hypotf() (the NetBSD namespace of the library).
 */
#define hypotf _hypotf

/**
 * @macroname `exp`
 *
 * @description
 * Renames exp() to _exp() (the NetBSD namespace of the library).
 */
#define exp _exp

/**
 * @macroname `expf`
 *
 * @description
 * Renames expf() to _expf() (the NetBSD namespace of the library).
 */
#define expf _expf

/**
 * @macroname `log`
 *
 * @description
 * Renames log() to _log() (the NetBSD namespace of the library).
 */
#define log _log

/**
 * @macroname `logf`
 *
 * @description
 * Renames logf() to _logf() (the NetBSD namespace of the library).
 */
#define logf _logf

#if 0 /* not yet - need to review use in machdep code first */

/**
 * @macroname `sin`
 *
 * @description
 * Renames sin() to _sin() (the NetBSD namespace of the library).
 */
#define sin _sin

/**
 * @macroname `sinf`
 *
 * @description
 * Renames sinf() to _sinf() (the NetBSD namespace of the library).
 */
#define sinf _sinf

/**
 * @macroname `cos`
 *
 * @description
 * Renames cos() to _cos() (the NetBSD namespace of the library).
 */
#define cos _cos

/**
 * @macroname `cosf`
 *
 * @description
 * Renames cosf() to _cosf() (the NetBSD namespace of the library).
 */
#define cosf _cosf
#endif /* notyet */

/**
 * @macroname `sinh`
 *
 * @description
 * Renames sinh() to _sinh() (the NetBSD namespace of the library).
 */
#define sinh _sinh

/**
 * @macroname `sinhf`
 *
 * @description
 * Renames sinhf() to _sinhf() (the NetBSD namespace of the library).
 */
#define sinhf _sinhf

/**
 * @macroname `cosh`
 *
 * @description
 * Renames cosh() to _cosh() (the NetBSD namespace of the library).
 */
#define cosh _cosh

/**
 * @macroname `coshf`
 *
 * @description
 * Renames coshf() to _coshf() (the NetBSD namespace of the library).
 */
#define coshf _coshf

/**
 * @macroname `asin`
 *
 * @description
 * Renames asin() to _asin() (the NetBSD namespace of the library).
 */
#define asin _asin

/**
 * @macroname `asinf`
 *
 * @description
 * Renames asinf() to _asinf() (the NetBSD namespace of the library).
 */
#define asinf _asinf

/**
 * @macroname `casin`
 *
 * @description
 * Renames casin() to _casin() (the NetBSD namespace of the library).
 */
#define casin _casin

/**
 * @macroname `casinf`
 *
 * @description
 * Renames casinf() to _casinf() (the NetBSD namespace of the library).
 */
#define casinf _casinf

/**
 * @macroname `catan`
 *
 * @description
 * Renames catan() to _catan() (the NetBSD namespace of the library).
 */
#define catan _catan

/**
 * @macroname `catanf`
 *
 * @description
 * Renames catanf() to _catanf() (the NetBSD namespace of the library).
 */
#define catanf _catanf
