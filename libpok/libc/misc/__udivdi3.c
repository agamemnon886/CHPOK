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
 */

#include <core/dependencies.h>

#ifdef  POK_CONFIG_NEEDS_FUNC_UDIVDI3

/*
 * MISRA C:2025 Rule 5.10 deviation: __udivdi3() is the 64-bit division
 * helper of the compiler's run-time library (libgcc), which the compiler
 * calls on 32-bit targets; it must have this reserved name and these types.
 * Nothing in C calls it, so it is declared here instead of in a header.
 */
unsigned long long __udivdi3(unsigned long long num,
			     unsigned long long den);

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

