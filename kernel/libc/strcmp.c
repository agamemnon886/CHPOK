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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: strcmp() and
 * strncmp() share one loop with a single exit and a size_t index.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#if defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_PORTS_QUEUEING) || defined (POK_NEEDS_PORTS_SAMPLING)

#include <libc.h>

/*
 * Compare at most 'size' characters (SIZE_MAX: the whole strings); the
 * result is -1, 0 or 1.
 */
static int compare(const char *s1, const char *s2, size_t size)
{
   size_t i = 0U;
   int result = 0;
   bool done = false;

   while ((!done) && (i < size))
   {
      if ((s1[i] == '\0') && (s2[i] == '\0'))
      {
         done = true;
      }
      else if (s1[i] < s2[i])
      {
         result = -1;
         done = true;
      }
      else if (s1[i] > s2[i])
      {
         result = 1;
         done = true;
      }
      else
      {
         i++;
      }
   }
   return result;
}

/*
 * MISRA C:2025 Dir 1.2 deviation: __attribute__((weak)) is a GNU extension;
 * it lets a board or a configuration provide its own implementation.
 */
__attribute__ ((weak))
int strcmp (const char *s1,
            const char *s2)
{
   return compare(s1, s2, SIZE_MAX);
}

__attribute__ ((weak))
int strncmp (const char *s1,
             const char *s2,
             size_t size)
{
   return compare(s1, s2, size);
}

#endif
