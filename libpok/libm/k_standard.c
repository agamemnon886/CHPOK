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
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros are moved to
 * libpok/libm/math_private.h; POK_ERRNO_HUGE_VAL is always the local variable
 * inf of __kernel_standard() (nothing else defines it).
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/* @(#)k_standard.c 5.1 93/09/24 */
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
 * `__kernel_standard()` of libpok's math library (fdlibm, from Sun
 * Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include "math_private.h"
#include <errno.h>

static const double zero = 0.0;	/* used as const */

/*
 * Standard conformance (non-IEEE) on exception cases.
 * Mapping:
 *	1 -- acos(|x|>1)
 *	2 -- asin(|x|>1)
 *	3 -- atan2(+-0,+-0)
 *	4 -- hypot overflow
 *	5 -- cosh overflow
 *	6 -- exp overflow
 *	7 -- exp underflow
 *	8 -- y0(0)
 *	9 -- y0(-ve)
 *	10-- y1(0)
 *	11-- y1(-ve)
 *	12-- yn(0)
 *	13-- yn(-ve)
 *	14-- lgamma(finite) overflow
 *	15-- lgamma(-integer)
 *	16-- log(0)
 *	17-- log(x<0)
 *	18-- log10(0)
 *	19-- log10(x<0)
 *	20-- pow(0.0,0.0)
 *	21-- pow(x,y) overflow
 *	22-- pow(x,y) underflow
 *	23-- pow(0,negative)
 *	24-- pow(neg,non-integral)
 *	25-- sinh(finite) overflow
 *	26-- sqrt(negative)
 *      27-- fmod(x,0)
 *      28-- remainder(x,0)
 *	29-- acosh(x<1)
 *	30-- atanh(|x|>1)
 *	31-- atanh(|x|=1)
 *	32-- scalb overflow
 *	33-- scalb underflow
 *	34-- j0(|x|>X_TLOSS)
 *	35-- y0(x>X_TLOSS)
 *	36-- j1(|x|>X_TLOSS)
 *	37-- y1(x>X_TLOSS)
 *	38-- jn(|x|>X_TLOSS, n)
 *	39-- yn(x>X_TLOSS, n)
 *	40-- gamma(finite) overflow
 *	41-- gamma(-integer)
 *	42-- pow(NaN,0.0)
 *	48-- log2(0)
 *	49-- log2(x<0)
 */


/**
 * @funcname `__kernel_standard`
 *
 * @description
 * Handles an error of a wrapper as the selected standard (SVID, X/OPEN, POSIX
 * or IEEE) requires: the result and errno for the error type.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The first argument of the wrapper. |
 * | `y` | `double` | [in] The second argument of the wrapper. |
 * | `type` | `int` | [in] The error type (the number of the wrapper's error). |
 *
 * @return The result for the error.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `errno` | `uint32_t` | The error number of the C library |
 * | `zero` | `const double` | A constant of the algorithm |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `exc` | `struct exception` | A temporary of the computation |
 * | `inf` | `double` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `inf` to `0.0`.
 * 2. Use the macro `SET_HIGH_WORD()` with the arguments `inf` and `0x7ff00000`.
 * 3. Set `exc.arg1` to `x`.
 * 4. Set `exc.arg2` to `y`.
 * 5. Depending on `type`:
 *    1. Case `1` or `101`:
 *       1. Set `exc.type` to `DOMAIN`.
 *       2. Set `exc.name` to `"acos"` if `type` is less than `100`, otherwise
 *          `"acosf"`.
 *       3. Set `exc.retval` to `zero`.
 *       4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *          1. Set `errno` to `POK_ERRNO_EDOM`.
 *       5. Otherwise, if `matherr(&exc)` is false:
 *          1. If `_LIB_VERSION` is equal to `_SVID_`:
 *             1. Use the macro `WRITE2()` with the arguments
 *                `"acos: DOMAIN error\n"` and `19`.
 *          2. Set `errno` to `POK_ERRNO_EDOM`.
 *       6. Leave the switch.
 *    2. Case `2` or `102`:
 *       1. Set `exc.type` to `DOMAIN`.
 *       2. Set `exc.name` to `"asin"` if `type` is less than `100`, otherwise
 *          `"asinf"`.
 *       3. Set `exc.retval` to `zero`.
 *       4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *          1. Set `errno` to `POK_ERRNO_EDOM`.
 *       5. Otherwise, if `matherr(&exc)` is false:
 *          1. If `_LIB_VERSION` is equal to `_SVID_`:
 *             1. Use the macro `WRITE2()` with the arguments
 *                `"asin: DOMAIN error\n"` and `19`.
 *          2. Set `errno` to `POK_ERRNO_EDOM`.
 *       6. Leave the switch.
 *    3. Case `3` or `103`:
 *       1. Set `exc.arg1` to `y`.
 *       2. Set `exc.arg2` to `x`.
 *       3. Set `exc.type` to `DOMAIN`.
 *       4. Set `exc.name` to `"atan2"` if `type` is less than `100`, otherwise
 *          `"atan2f"`.
 *       5. Set `exc.retval` to `zero`.
 *       6. If `_LIB_VERSION` is equal to `_POSIX_`:
 *          1. Set `errno` to `POK_ERRNO_EDOM`.
 *       7. Otherwise, if `matherr(&exc)` is false:
 *          1. If `_LIB_VERSION` is equal to `_SVID_`:
 *             1. Use the macro `WRITE2()` with the arguments
 *                `"atan2: DOMAIN error\n"` and `20`.
 *          2. Set `errno` to `POK_ERRNO_EDOM`.
 *       8. Leave the switch.
 *    4. Case `4` or `104`:
 *       1. Set `exc.type` to `OVERFLOW`.
 *       2. Set `exc.name` to `"hypot"` if `type` is less than `100`, otherwise
 *          `"hypotf"`.
 *       3. If `_LIB_VERSION` is equal to `_SVID_`:
 *          1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *       4. Otherwise:
 *          1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *       5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *          1. Set `errno` to `POK_ERRNO_ERANGE`.
 *       6. Otherwise, if `matherr(&exc)` is false:
 *          1. Set `errno` to `POK_ERRNO_ERANGE`.
 *       7. Leave the switch.
 *    5. Case `5` or `105`:
 *       1. Set `exc.type` to `OVERFLOW`.
 *       2. Set `exc.name` to `"cosh"` if `type` is less than `100`, otherwise
 *          `"coshf"`.
 *       3. If `_LIB_VERSION` is equal to `_SVID_`:
 *          1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *       4. Otherwise:
 *          1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *       5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *          1. Set `errno` to `POK_ERRNO_ERANGE`.
 *       6. Otherwise, if `matherr(&exc)` is false:
 *          1. Set `errno` to `POK_ERRNO_ERANGE`.
 *       7. Leave the switch.
 *    6. Case `6` or `106`:
 *       1. Set `exc.type` to `OVERFLOW`.
 *       2. Set `exc.name` to `"exp"` if `type` is less than `100`, otherwise
 *          `"expf"`.
 *       3. If `_LIB_VERSION` is equal to `_SVID_`:
 *          1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *       4. Otherwise:
 *          1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *       5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *          1. Set `errno` to `POK_ERRNO_ERANGE`.
 *       6. Otherwise, if `matherr(&exc)` is false:
 *          1. Set `errno` to `POK_ERRNO_ERANGE`.
 *       7. Leave the switch.
 *    7. Case `7` or `107`:
 *       1. Set `exc.type` to `UNDERFLOW`.
 *       2. Set `exc.name` to `"exp"` if `type` is less than `100`, otherwise
 *          `"expf"`.
 *       3. Set `exc.retval` to `zero`.
 *       4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *          1. Set `errno` to `POK_ERRNO_ERANGE`.
 *       5. Otherwise, if `matherr(&exc)` is false:
 *          1. Set `errno` to `POK_ERRNO_ERANGE`.
 *       6. Leave the switch.
 *    8. Case `8` or `108`:
 *       1. Set `exc.type` to `DOMAIN`.
 *       2. Set `exc.name` to `"y0"` if `type` is less than `100`, otherwise
 *          `"y0f"`.
 *       3. If `_LIB_VERSION` is equal to `_SVID_`:
 *          1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *       4. Otherwise:
 *          1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *       5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *          1. Set `errno` to `POK_ERRNO_EDOM`.
 *       6. Otherwise, if `matherr(&exc)` is false:
 *          1. If `_LIB_VERSION` is equal to `_SVID_`:
 *             1. Use the macro `WRITE2()` with the arguments
 *                `"y0: DOMAIN error\n"` and `17`.
 *          2. Set `errno` to `POK_ERRNO_EDOM`.
 *       7. Leave the switch.
 *    9. Case `9` or `109`:
 *       1. Set `exc.type` to `DOMAIN`.
 *       2. Set `exc.name` to `"y0"` if `type` is less than `100`, otherwise
 *          `"y0f"`.
 *       3. If `_LIB_VERSION` is equal to `_SVID_`:
 *          1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *       4. Otherwise:
 *          1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *       5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *          1. Set `errno` to `POK_ERRNO_EDOM`.
 *       6. Otherwise, if `matherr(&exc)` is false:
 *          1. If `_LIB_VERSION` is equal to `_SVID_`:
 *             1. Use the macro `WRITE2()` with the arguments
 *                `"y0: DOMAIN error\n"` and `17`.
 *          2. Set `errno` to `POK_ERRNO_EDOM`.
 *       7. Leave the switch.
 *    10. Case `10` or `110`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"y1"` if `type` is less than `100`, otherwise
 *           `"y1f"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"y1: DOMAIN error\n"` and `17`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    11. Case `11` or `111`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"y1"` if `type` is less than `100`, otherwise
 *           `"y1f"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"y1: DOMAIN error\n"` and `17`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    12. Case `12` or `112`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"yn"` if `type` is less than `100`, otherwise
 *           `"ynf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"yn: DOMAIN error\n"` and `17`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    13. Case `13` or `113`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"yn"` if `type` is less than `100`, otherwise
 *           `"ynf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"yn: DOMAIN error\n"` and `17`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    14. Case `14` or `114`:
 *        1. Set `exc.type` to `OVERFLOW`.
 *        2. Set `exc.name` to `"lgamma"` if `type` is less than `100`,
 *           otherwise `"lgammaf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        7. Leave the switch.
 *    15. Case `15` or `115`:
 *        1. Set `exc.type` to `SING`.
 *        2. Set `exc.name` to `"lgamma"` if `type` is less than `100`,
 *           otherwise `"lgammaf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"lgamma: SING error\n"` and `19`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    16. Case `16` or `116`:
 *        1. Set `exc.type` to `SING`.
 *        2. Set `exc.name` to `"log"` if `type` is less than `100`, otherwise
 *           `"logf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"log: SING error\n"` and `16`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    17. Case `17` or `117`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"log"` if `type` is less than `100`, otherwise
 *           `"logf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"log: DOMAIN error\n"` and `18`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    18. Case `18` or `118`:
 *        1. Set `exc.type` to `SING`.
 *        2. Set `exc.name` to `"log10"` if `type` is less than `100`, otherwise
 *           `"log10f"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"log10: SING error\n"` and `18`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    19. Case `19` or `119`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"log10"` if `type` is less than `100`, otherwise
 *           `"log10f"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"log10: DOMAIN error\n"` and `20`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    20. Case `20` or `120`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"pow"` if `type` is less than `100`, otherwise
 *           `"powf"`.
 *        3. Set `exc.retval` to `zero`.
 *        4. If `_LIB_VERSION` is not equal to `_SVID_`:
 *           1. Set `exc.retval` to `1.0`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. Use the macro `WRITE2()` with the arguments
 *              `"pow(0,0): DOMAIN error\n"` and `23`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Leave the switch.
 *    21. Case `21` or `121`:
 *        1. Set `exc.type` to `OVERFLOW`.
 *        2. Set `exc.name` to `"pow"` if `type` is less than `100`, otherwise
 *           `"powf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *           2. Multiply `y` by `0.5`.
 *           3. If `x` is less than `zero` and `rint(y)` is not equal to `y`:
 *              1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *           2. Multiply `y` by `0.5`.
 *           3. If `x` is less than `zero` and `rint(y)` is not equal to `y`:
 *              1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        7. Leave the switch.
 *    22. Case `22` or `122`:
 *        1. Set `exc.type` to `UNDERFLOW`.
 *        2. Set `exc.name` to `"pow"` if `type` is less than `100`, otherwise
 *           `"powf"`.
 *        3. Set `exc.retval` to `zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Leave the switch.
 *    23. Case `23` or `123`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"pow"` if `type` is less than `100`, otherwise
 *           `"powf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `zero`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"pow(0,neg): DOMAIN error\n"` and `25`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    24. Case `24` or `124`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"pow"` if `type` is less than `100`, otherwise
 *           `"powf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `zero`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `zero/zero`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"neg**non-integral: DOMAIN error\n"` and `32`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    25. Case `25` or `125`:
 *        1. Set `exc.type` to `OVERFLOW`.
 *        2. Set `exc.name` to `"sinh"` if `type` is less than `100`, otherwise
 *           `"sinhf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to
 *              `( (x>zero) ? POK_ERRNO_HUGE_VAL : -POK_ERRNO_HUGE_VAL)`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to
 *              `( (x>zero) ? POK_ERRNO_HUGE_VAL : -POK_ERRNO_HUGE_VAL)`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        7. Leave the switch.
 *    26. Case `26` or `126`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"sqrt"` if `type` is less than `100`, otherwise
 *           `"sqrtf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `zero`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `zero/zero`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"sqrt: DOMAIN error\n"` and `19`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    27. Case `27` or `127`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"fmod"` if `type` is less than `100`, otherwise
 *           `"fmodf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `x`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `zero/zero`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"fmod: DOMAIN error\n"` and `20`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    28. Case `28` or `128`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"remainder"` if `type` is less than `100`,
 *           otherwise `"remainderf"`.
 *        3. Set `exc.retval` to `zero/zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"remainder: DOMAIN error\n"` and `24`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Leave the switch.
 *    29. Case `29` or `129`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"acosh"` if `type` is less than `100`, otherwise
 *           `"acoshf"`.
 *        3. Set `exc.retval` to `zero/zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"acosh: DOMAIN error\n"` and `20`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Leave the switch.
 *    30. Case `30` or `130`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"atanh"` if `type` is less than `100`, otherwise
 *           `"atanhf"`.
 *        3. Set `exc.retval` to `zero/zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"atanh: DOMAIN error\n"` and `20`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Leave the switch.
 *    31. Case `31` or `131`:
 *        1. Set `exc.type` to `SING`.
 *        2. Set `exc.name` to `"atanh"` if `type` is less than `100`, otherwise
 *           `"atanhf"`.
 *        3. Set `exc.retval` to `x/zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"atanh: SING error\n"` and `18`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Leave the switch.
 *    32. Case `32` or `132`:
 *        1. Set `exc.type` to `OVERFLOW`.
 *        2. Set `exc.name` to `"scalb"` if `type` is less than `100`, otherwise
 *           `"scalbf"`.
 *        3. Set `exc.retval` to `POK_ERRNO_HUGE_VAL` if `x` is greater than
 *           `zero`, otherwise `-POK_ERRNO_HUGE_VAL`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Leave the switch.
 *    33. Case `33` or `133`:
 *        1. Set `exc.type` to `UNDERFLOW`.
 *        2. Set `exc.name` to `"scalb"` if `type` is less than `100`, otherwise
 *           `"scalbf"`.
 *        3. Call `copysign()` with the arguments `zero` and `x`, and store the
 *           result in `exc.retval`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Leave the switch.
 *    34. Case `34` or `134`:
 *        1. Set `exc.type` to `TLOSS`.
 *        2. Set `exc.name` to `"j0"` if `type` is less than `100`, otherwise
 *           `"j0f"`.
 *        3. Set `exc.retval` to `zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments `exc.name` and
 *                 `2`.
 *              2. Use the macro `WRITE2()` with the arguments
 *                 `": TLOSS error\n"` and `14`.
 *           2. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Leave the switch.
 *    35. Case `35` or `135`:
 *        1. Set `exc.type` to `TLOSS`.
 *        2. Set `exc.name` to `"y0"` if `type` is less than `100`, otherwise
 *           `"y0f"`.
 *        3. Set `exc.retval` to `zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments `exc.name` and
 *                 `2`.
 *              2. Use the macro `WRITE2()` with the arguments
 *                 `": TLOSS error\n"` and `14`.
 *           2. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Leave the switch.
 *    36. Case `36` or `136`:
 *        1. Set `exc.type` to `TLOSS`.
 *        2. Set `exc.name` to `"j1"` if `type` is less than `100`, otherwise
 *           `"j1f"`.
 *        3. Set `exc.retval` to `zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments `exc.name` and
 *                 `2`.
 *              2. Use the macro `WRITE2()` with the arguments
 *                 `": TLOSS error\n"` and `14`.
 *           2. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Leave the switch.
 *    37. Case `37` or `137`:
 *        1. Set `exc.type` to `TLOSS`.
 *        2. Set `exc.name` to `"y1"` if `type` is less than `100`, otherwise
 *           `"y1f"`.
 *        3. Set `exc.retval` to `zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments `exc.name` and
 *                 `2`.
 *              2. Use the macro `WRITE2()` with the arguments
 *                 `": TLOSS error\n"` and `14`.
 *           2. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Leave the switch.
 *    38. Case `38` or `138`:
 *        1. Set `exc.type` to `TLOSS`.
 *        2. Set `exc.name` to `"jn"` if `type` is less than `100`, otherwise
 *           `"jnf"`.
 *        3. Set `exc.retval` to `zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments `exc.name` and
 *                 `2`.
 *              2. Use the macro `WRITE2()` with the arguments
 *                 `": TLOSS error\n"` and `14`.
 *           2. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Leave the switch.
 *    39. Case `39` or `139`:
 *        1. Set `exc.type` to `TLOSS`.
 *        2. Set `exc.name` to `"yn"` if `type` is less than `100`, otherwise
 *           `"ynf"`.
 *        3. Set `exc.retval` to `zero`.
 *        4. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments `exc.name` and
 *                 `2`.
 *              2. Use the macro `WRITE2()` with the arguments
 *                 `": TLOSS error\n"` and `14`.
 *           2. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Leave the switch.
 *    40. Case `40` or `140`:
 *        1. Set `exc.type` to `OVERFLOW`.
 *        2. Set `exc.name` to `"gamma"` if `type` is less than `100`, otherwise
 *           `"gammaf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        7. Leave the switch.
 *    41. Case `41` or `141`:
 *        1. Set `exc.type` to `SING`.
 *        2. Set `exc.name` to `"gamma"` if `type` is less than `100`, otherwise
 *           `"gammaf"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"gamma: SING error\n"` and `18`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    42. Case `42` or `142`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"pow"` if `type` is less than `100`, otherwise
 *           `"powf"`.
 *        3. Set `exc.retval` to `x`.
 *        4. If `_LIB_VERSION` is equal to `_IEEE_` or `_LIB_VERSION` is equal
 *           to `_POSIX_`:
 *           1. Set `exc.retval` to `1.0`.
 *        5. Otherwise, if `matherr(&exc)` is false:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Leave the switch.
 *    43. Case `48` or `148`:
 *        1. Set `exc.type` to `SING`.
 *        2. Set `exc.name` to `"log2"` if `type` is less than `100`, otherwise
 *           `"log2f"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_ERANGE`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"log2: SING error\n"` and `18`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    44. Case `49` or `149`:
 *        1. Set `exc.type` to `DOMAIN`.
 *        2. Set `exc.name` to `"log2"` if `type` is less than `100`, otherwise
 *           `"log2f"`.
 *        3. If `_LIB_VERSION` is equal to `_SVID_`:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        4. Otherwise:
 *           1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        5. If `_LIB_VERSION` is equal to `_POSIX_`:
 *           1. Set `errno` to `POK_ERRNO_EDOM`.
 *        6. Otherwise, if `matherr(&exc)` is false:
 *           1. If `_LIB_VERSION` is equal to `_SVID_`:
 *              1. Use the macro `WRITE2()` with the arguments
 *                 `"log2: DOMAIN error\n"` and `20`.
 *           2. Set `errno` to `POK_ERRNO_EDOM`.
 *        7. Leave the switch.
 *    45. Otherwise (default):
 *        1. Set `exc.retval` to `-POK_ERRNO_HUGE_VAL`.
 *        2. Leave the switch.
 * 6. Return `exc.retval`.
 *
 * @calls
 * - `matherr()`
 * - `printf()`
 * - `rint()`
 * - `copysign()`
 */
double
__kernel_standard(double x, double y, int type)
{
	struct exception exc;
	double inf = 0.0;

	SET_HIGH_WORD(inf,0x7ff00000);	/* set inf to infinite */

#ifdef _USE_WRITE
	(void) fflush(stdout);
#endif
	exc.arg1 = x;
	exc.arg2 = y;
	switch(type) {
	    case 1:
	    case 101:
		/* acos(|x|>1) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "acos" : "acosf";
		exc.retval = zero;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if(_LIB_VERSION == _SVID_) {
		    WRITE2("acos: DOMAIN error\n", 19);
		  }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 2:
	    case 102:
		/* asin(|x|>1) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "asin" : "asinf";
		exc.retval = zero;
		if(_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if(_LIB_VERSION == _SVID_) {
		    	WRITE2("asin: DOMAIN error\n", 19);
		  }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 3:
	    case 103:
		/* atan2(+-0,+-0) */
		exc.arg1 = y;
		exc.arg2 = x;
		exc.type = DOMAIN;
		exc.name = type < 100 ? "atan2" : "atan2f";
		exc.retval = zero;
		if(_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if(_LIB_VERSION == _SVID_) {
			WRITE2("atan2: DOMAIN error\n", 20);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 4:
	    case 104:
		/* hypot(finite,finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "hypot" : "hypotf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 5:
	    case 105:
		/* cosh(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "cosh" : "coshf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 6:
	    case 106:
		/* exp(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "exp" : "expf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 7:
	    case 107:
		/* exp(finite) underflow */
		exc.type = UNDERFLOW;
		exc.name = type < 100 ? "exp" : "expf";
		exc.retval = zero;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 8:
	    case 108:
		/* y0(0) = -inf */
		exc.type = DOMAIN;	/* should be SING for IEEE */
		exc.name = type < 100 ? "y0" : "y0f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("y0: DOMAIN error\n", 17);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 9:
	    case 109:
		/* y0(x<0) = NaN */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "y0" : "y0f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("y0: DOMAIN error\n", 17);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 10:
	    case 110:
		/* y1(0) = -inf */
		exc.type = DOMAIN;	/* should be SING for IEEE */
		exc.name = type < 100 ? "y1" : "y1f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("y1: DOMAIN error\n", 17);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 11:
	    case 111:
		/* y1(x<0) = NaN */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "y1" : "y1f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("y1: DOMAIN error\n", 17);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 12:
	    case 112:
		/* yn(n,0) = -inf */
		exc.type = DOMAIN;	/* should be SING for IEEE */
		exc.name = type < 100 ? "yn" : "ynf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("yn: DOMAIN error\n", 17);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 13:
	    case 113:
		/* yn(x<0) = NaN */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "yn" : "ynf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("yn: DOMAIN error\n", 17);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 14:
	    case 114:
		/* lgamma(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "lgamma" : "lgammaf";
                if (_LIB_VERSION == _SVID_)
		            exc.retval = POK_ERRNO_HUGE_VAL;
                else
                  exc.retval = POK_ERRNO_HUGE_VAL;
                if (_LIB_VERSION == _POSIX_)
			errno = POK_ERRNO_ERANGE;
                else if (!matherr(&exc)) {
                        errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 15:
	    case 115:
		/* lgamma(-integer) or lgamma(0) */
		exc.type = SING;
		exc.name = type < 100 ? "lgamma" : "lgammaf";
                if (_LIB_VERSION == _SVID_)
		            exc.retval = POK_ERRNO_HUGE_VAL;
                else
                  exc.retval = POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("lgamma: SING error\n", 19);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 16:
	    case 116:
		/* log(0) */
		exc.type = SING;
		exc.name = type < 100 ? "log" : "logf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("log: SING error\n", 16);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 17:
	    case 117:
		/* log(x<0) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "log" : "logf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("log: DOMAIN error\n", 18);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 18:
	    case 118:
		/* log10(0) */
		exc.type = SING;
		exc.name = type < 100 ? "log10" : "log10f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("log10: SING error\n", 18);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 19:
	    case 119:
		/* log10(x<0) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "log10" : "log10f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("log10: DOMAIN error\n", 20);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 20:
	    case 120:
		/* pow(0.0,0.0) */
		/* error only if _LIB_VERSION == _SVID_ */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "pow" : "powf";
		exc.retval = zero;
		if (_LIB_VERSION != _SVID_) exc.retval = 1.0;
		else if (!matherr(&exc)) {
			WRITE2("pow(0,0): DOMAIN error\n", 23);
			errno = POK_ERRNO_EDOM;
		}
		break;
	    case 21:
	    case 121:
		/* pow(x,y) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "pow" : "powf";
		if (_LIB_VERSION == _SVID_) {
		  exc.retval = POK_ERRNO_HUGE_VAL;
		  y *= 0.5;
		  if(x<zero&&rint(y)!=y) exc.retval = -POK_ERRNO_HUGE_VAL;
		} else {
		  exc.retval = POK_ERRNO_HUGE_VAL;
		  y *= 0.5;
		  if(x<zero&&rint(y)!=y) exc.retval = -POK_ERRNO_HUGE_VAL;
		}
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 22:
	    case 122:
		/* pow(x,y) underflow */
		exc.type = UNDERFLOW;
		exc.name = type < 100 ? "pow" : "powf";
		exc.retval =  zero;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 23:
	    case 123:
		/* 0**neg */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "pow" : "powf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = zero;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("pow(0,neg): DOMAIN error\n", 25);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 24:
	    case 124:
		/* neg**non-integral */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "pow" : "powf";
		if (_LIB_VERSION == _SVID_)
		    exc.retval = zero;
		else
		    exc.retval = zero/zero;	/* X/Open allow NaN */
		if (_LIB_VERSION == _POSIX_)
		   errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("neg**non-integral: DOMAIN error\n", 32);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 25:
	    case 125:
		/* sinh(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "sinh" : "sinhf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = ( (x>zero) ? POK_ERRNO_HUGE_VAL : -POK_ERRNO_HUGE_VAL);
		else
		  exc.retval = ( (x>zero) ? POK_ERRNO_HUGE_VAL : -POK_ERRNO_HUGE_VAL);
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 26:
	    case 126:
		/* sqrt(x<0) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "sqrt" : "sqrtf";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = zero;
		else
		  exc.retval = zero/zero;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("sqrt: DOMAIN error\n", 19);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
            case 27:
	    case 127:
                /* fmod(x,0) */
                exc.type = DOMAIN;
                exc.name = type < 100 ? "fmod" : "fmodf";
                if (_LIB_VERSION == _SVID_)
                    exc.retval = x;
		else
		    exc.retval = zero/zero;
                if (_LIB_VERSION == _POSIX_)
                  errno = POK_ERRNO_EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    WRITE2("fmod:  DOMAIN error\n", 20);
                  }
                  errno = POK_ERRNO_EDOM;
                }
                break;
            case 28:
	    case 128:
                /* remainder(x,0) */
                exc.type = DOMAIN;
                exc.name = type < 100 ? "remainder" : "remainderf";
                exc.retval = zero/zero;
                if (_LIB_VERSION == _POSIX_)
                  errno = POK_ERRNO_EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    WRITE2("remainder: DOMAIN error\n", 24);
                  }
                  errno = POK_ERRNO_EDOM;
                }
                break;
            case 29:
	    case 129:
                /* acosh(x<1) */
                exc.type = DOMAIN;
                exc.name = type < 100 ? "acosh" : "acoshf";
                exc.retval = zero/zero;
                if (_LIB_VERSION == _POSIX_)
                  errno = POK_ERRNO_EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    WRITE2("acosh: DOMAIN error\n", 20);
                  }
                  errno = POK_ERRNO_EDOM;
                }
                break;
            case 30:
	    case 130:
                /* atanh(|x|>1) */
                exc.type = DOMAIN;
                exc.name = type < 100 ? "atanh" : "atanhf";
                exc.retval = zero/zero;
                if (_LIB_VERSION == _POSIX_)
                  errno = POK_ERRNO_EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    WRITE2("atanh: DOMAIN error\n", 20);
                  }
                  errno = POK_ERRNO_EDOM;
                }
                break;
            case 31:
	    case 131:
                /* atanh(|x|=1) */
                exc.type = SING;
                exc.name = type < 100 ? "atanh" : "atanhf";
		exc.retval = x/zero;	/* sign(x)*inf */
                if (_LIB_VERSION == _POSIX_)
                  errno = POK_ERRNO_EDOM;
                else if (!matherr(&exc)) {
                  if (_LIB_VERSION == _SVID_) {
                    WRITE2("atanh: SING error\n", 18);
                  }
                  errno = POK_ERRNO_EDOM;
                }
                break;
	    case 32:
	    case 132:
		/* scalb overflow; SVID also returns +-POK_ERRNO_HUGE_VAL */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "scalb" : "scalbf";
		exc.retval = x > zero ? POK_ERRNO_HUGE_VAL : -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 33:
	    case 133:
		/* scalb underflow */
		exc.type = UNDERFLOW;
		exc.name = type < 100 ? "scalb" : "scalbf";
		exc.retval = copysign(zero,x);
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_ERANGE;
		}
		break;
	    case 34:
	    case 134:
		/* j0(|x|>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "j0" : "j0f";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = POK_ERRNO_ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                WRITE2(exc.name, 2);
                                WRITE2(": TLOSS error\n", 14);
                        }
                        errno = POK_ERRNO_ERANGE;
                }
		break;
	    case 35:
	    case 135:
		/* y0(x>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "y0" : "y0f";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = POK_ERRNO_ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                WRITE2(exc.name, 2);
                                WRITE2(": TLOSS error\n", 14);
                        }
                        errno = POK_ERRNO_ERANGE;
                }
		break;
	    case 36:
	    case 136:
		/* j1(|x|>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "j1" : "j1f";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = POK_ERRNO_ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                WRITE2(exc.name, 2);
                                WRITE2(": TLOSS error\n", 14);
                        }
                        errno = POK_ERRNO_ERANGE;
                }
		break;
	    case 37:
	    case 137:
		/* y1(x>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "y1" : "y1f";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = POK_ERRNO_ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                WRITE2(exc.name, 2);
                                WRITE2(": TLOSS error\n", 14);
                        }
                        errno = POK_ERRNO_ERANGE;
                }
		break;
	    case 38:
	    case 138:
		/* jn(|x|>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "jn" : "jnf";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = POK_ERRNO_ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                WRITE2(exc.name, 2);
                                WRITE2(": TLOSS error\n", 14);
                        }
                        errno = POK_ERRNO_ERANGE;
                }
		break;
	    case 39:
	    case 139:
		/* yn(x>X_TLOSS) */
                exc.type = TLOSS;
                exc.name = type < 100 ? "yn" : "ynf";
                exc.retval = zero;
                if (_LIB_VERSION == _POSIX_)
                        errno = POK_ERRNO_ERANGE;
                else if (!matherr(&exc)) {
                        if (_LIB_VERSION == _SVID_) {
                                WRITE2(exc.name, 2);
                                WRITE2(": TLOSS error\n", 14);
                        }
                        errno = POK_ERRNO_ERANGE;
                }
		break;
	    case 40:
	    case 140:
		/* gamma(finite) overflow */
		exc.type = OVERFLOW;
		exc.name = type < 100 ? "gamma" : "gammaf";
                if (_LIB_VERSION == _SVID_)
		            exc.retval = POK_ERRNO_HUGE_VAL;
                else
                  exc.retval = POK_ERRNO_HUGE_VAL;
                if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
                else if (!matherr(&exc)) {
                  errno = POK_ERRNO_ERANGE;
                }
		break;
	    case 41:
	    case 141:
		/* gamma(-integer) or gamma(0) */
		exc.type = SING;
		exc.name = type < 100 ? "gamma" : "gammaf";
                if (_LIB_VERSION == _SVID_)
		            exc.retval = POK_ERRNO_HUGE_VAL;
                else
                  exc.retval = POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("gamma: SING error\n", 18);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 42:
	    case 142:
		/* pow(NaN,0.0) */
		/* error only if _LIB_VERSION == _SVID_ & _XOPEN_ */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "pow" : "powf";
		exc.retval = x;
		if (_LIB_VERSION == _IEEE_ ||
		    _LIB_VERSION == _POSIX_) exc.retval = 1.0;
		else if (!matherr(&exc)) {
			errno = POK_ERRNO_EDOM;
		}
		break;
	    case 48:
	    case 148:
		/* log2(0) */
		exc.type = SING;
		exc.name = type < 100 ? "log2" : "log2f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_ERANGE;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("log2: SING error\n", 18);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;
	    case 49:
	    case 149:
		/* log2(x<0) */
		exc.type = DOMAIN;
		exc.name = type < 100 ? "log2" : "log2f";
		if (_LIB_VERSION == _SVID_)
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		else
		  exc.retval = -POK_ERRNO_HUGE_VAL;
		if (_LIB_VERSION == _POSIX_)
		  errno = POK_ERRNO_EDOM;
		else if (!matherr(&exc)) {
		  if (_LIB_VERSION == _SVID_) {
			WRITE2("log2: DOMAIN error\n", 20);
		      }
		  errno = POK_ERRNO_EDOM;
		}
		break;

       default:
		  exc.retval = -POK_ERRNO_HUGE_VAL;
        break;
        /*
         * Default case introduced for POK to avoid a warning
         */
	}
	return exc.retval;
}

#endif
