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
 * Created by julien on Sat Jan 31 15:49:24 2009 
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

/**
 * @file
 * `__fpclassifyf()` and `__fpclassifyd()` of libpok's math library (fdlibm,
 * from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>
#include <types.h>
#include "math_private.h"

/**
 * @funcname `__fpclassifyf`
 *
 * @description
 * Returns the class of a float: FP_NAN, FP_INFINITE, FP_ZERO, FP_SUBNORMAL or
 * FP_NORMAL.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 *
 * @return The class.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `w` | `uint32_t` | The bits of x |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `GET_FLOAT_WORD()` with the arguments `w` and `x`.
 * 2. If `w` is equal to `0x00000000` or `w` is equal to `0x80000000`:
 *    1. Return `FP_ZERO`.
 * 3. Otherwise, if (`w` is greater than or equal to `0x00800000` and `w` is
 *    less than or equal to `0x7f7fffff`) or (`w` is greater than or equal to
 *    `0x80800000` and `w` is less than or equal to `0xff7fffff`):
 *    1. Return `FP_NORMAL`.
 * 4. Otherwise, if (`w` is greater than or equal to `0x00000001` and `w` is
 *    less than or equal to `0x007fffff`) or (`w` is greater than or equal to
 *    `0x80000001` and `w` is less than or equal to `0x807fffff`):
 *    1. Return `FP_SUBNORMAL`.
 * 5. Otherwise, if `w` is equal to `0x7f800000` or `w` is equal to
 *    `0xff800000`:
 *    1. Return `FP_INFINITE`.
 * 6. Otherwise:
 *    1. Return `FP_NAN`.
 *
 * @calls
 * None.
 */
int
__fpclassifyf (float x)
{
  uint32_t w;

  GET_FLOAT_WORD(w,x);
  
  if (w == 0x00000000 || w == 0x80000000)
    return FP_ZERO;
  else if ((w >= 0x00800000 && w <= 0x7f7fffff) ||
           (w >= 0x80800000 && w <= 0xff7fffff))
    return FP_NORMAL;
  else if ((w >= 0x00000001 && w <= 0x007fffff) ||
           (w >= 0x80000001 && w <= 0x807fffff))
    return FP_SUBNORMAL;
  else if (w == 0x7f800000 || w == 0xff800000)
    return FP_INFINITE;
  else
    return FP_NAN;
}

/**
 * @funcname `__fpclassifyd`
 *
 * @description
 * Returns the class of a double: FP_NAN, FP_INFINITE, FP_ZERO, FP_SUBNORMAL or
 * FP_NORMAL.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `double` | [in] The argument. |
 *
 * @return The class.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lsw` | `uint32_t` | The low word of x |
 * | `msw` | `uint32_t` | The high word of x |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `EXTRACT_WORDS()` with the arguments `msw`, `lsw` and `x`.
 * 2. If (`msw` is equal to `0x00000000` and `lsw` is equal to `0x00000000`) or
 *    (`msw` is equal to `0x80000000` and `lsw` is equal to `0x00000000`):
 *    1. Return `FP_ZERO`.
 * 3. Otherwise, if (`msw` is greater than or equal to `0x00100000` and `msw` is
 *    less than or equal to `0x7fefffff`) or (`msw` is greater than or equal to
 *    `0x80100000` and `msw` is less than or equal to `0xffefffff`):
 *    1. Return `FP_NORMAL`.
 * 4. Otherwise, if `msw` is less than or equal to `0x000fffff` or (`msw` is
 *    greater than or equal to `0x80000000` and `msw` is less than or equal to
 *    `0x800fffff`):
 *    1. Return `FP_SUBNORMAL`.
 * 5. Otherwise, if (`msw` is equal to `0x7ff00000` and `lsw` is equal to
 *    `0x00000000`) or (`msw` is equal to `0xfff00000` and `lsw` is equal to
 *    `0x00000000`):
 *    1. Return `FP_INFINITE`.
 * 6. Otherwise:
 *    1. Return `FP_NAN`.
 *
 * @calls
 * None.
 */
int
__fpclassifyd (double x)
{
  uint32_t msw, lsw;

  EXTRACT_WORDS(msw,lsw,x);

  if ((msw == 0x00000000 && lsw == 0x00000000) ||
      (msw == 0x80000000 && lsw == 0x00000000))
    return FP_ZERO;
  else if ((msw >= 0x00100000 && msw <= 0x7fefffff) ||
           (msw >= 0x80100000 && msw <= 0xffefffff))
    return FP_NORMAL;
  /*
  else if ((msw >= 0x00000000 && msw <= 0x000fffff) ||
bugfix : msw is an uint32_t (unsigned) value, always positive
  */
  else if ((msw <= 0x000fffff) ||
           (msw >= 0x80000000 && msw <= 0x800fffff))
    /* zero is already handled above */
    return FP_SUBNORMAL;
  else if ((msw == 0x7ff00000 && lsw == 0x00000000) ||
           (msw == 0xfff00000 && lsw == 0x00000000))
    return FP_INFINITE;
  else
    return FP_NAN;
}


#endif
