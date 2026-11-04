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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: U suffixes for
 * unsigned constants; the i386 assembly version is removed; the copy loop
 * uses an index.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#include <libc.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the standard memcpy() takes
 * and returns pointers to void; the bytes are accessed through pointers to
 * unsigned char.
 */

void* memcpy (void*        to,
              const void*  from,
              size_t       n)
{
  char *cto = (char *)to;
  const char *cfrom = (const char *)from;

  for (size_t i = 0U; i < n; i++)
  {
       cto[i] = cfrom[i];
  }
  return (to);
}
