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
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; declared before its definition; deviations are
 * documented where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: one declarator per
 * declaration; the parameters aren't modified. The modifications are
 * distributed under the POK licence (BSD-2-Clause, see POK-LICENSE). The
 * deviation comment names only Rule 5.10: the declaration above the definition
 * satisfies Rule 8.4.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-13: the declaration of __udivdi3() is
 * moved to libpok/include/arch.h.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * `__udivdi3()`: the unsigned 64-bit division helper of the compiler's
 * run-time library (POK_CONFIG_NEEDS_FUNC_UDIVDI3).
 */

#include <core/dependencies.h>
#include <arch.h>

#ifdef  POK_CONFIG_NEEDS_FUNC_UDIVDI3

/*
 * MISRA C:2025 Rule 5.10 deviation: __udivdi3() is the 64-bit division
 * helper of the compiler's run-time library (libgcc), which the compiler
 * calls on 32-bit targets; it must have this reserved name and these types
 * (declared in arch.h).
 */

/**
 * @funcname `__udivdi3`
 *
 * @description
 * Divides two unsigned 64-bit numbers by shifting and subtracting (the helper
 * that the compiler calls on 32-bit targets).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `num` | `unsigned long long` | [in] The dividend. |
 * | `den` | `unsigned long long` | [in] The divisor. |
 *
 * @return The quotient (0 for a division by 0).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `div` | `unsigned long long` | The divisor, shifted |
 * | `qbit` | `unsigned long long` | The bit of the quotient |
 * | `quot` | `unsigned long long` | The quotient so far |
 * | `rest` | `unsigned long long` | The remainder so far |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `quot` to `0U`.
 * 2. Set `qbit` to `1U`.
 * 3. Set `rest` to `num`.
 * 4. Set `div` to `den`.
 * 5. If `div` is equal to `0U`:
 *    1. Return `0`.
 * 6. While `(long long) div` is greater than or equal to `0`:
 *    1. Shift `div` left by `1` bits.
 *    2. Shift `qbit` left by `1` bits.
 * 7. While `qbit` is not equal to `0U`:
 *    1. If `div` is less than or equal to `rest`:
 *       1. Subtract `div` from `rest`.
 *       2. Add `qbit` to `quot`.
 *    2. Shift `div` right by `1` bits.
 *    3. Shift `qbit` right by `1` bits.
 * 8. Return `quot`.
 *
 * @calls
 * None.
 */
unsigned long long __udivdi3(unsigned long long num,
			     unsigned long long den)
{
  unsigned long long quot = 0U;
  unsigned long long qbit = 1U;
  unsigned long long rest = num;
  unsigned long long div = den;

  if (div == 0U) {
    return 0;
  }

  while ((long long) div >= 0)
  {
    div <<= 1;
    qbit <<= 1;
  }

  while (qbit != 0U)
  {
    if (div <= rest)
    {
      rest -= div;
      quot += qbit;
    }
    div >>= 1;
    qbit >>= 1;
  }

  return quot;
}
#endif

