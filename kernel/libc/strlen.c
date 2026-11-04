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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: strlen() and
 * strnlen() return size_t, as in the C standard; strncpy() fills the rest of
 * the n bytes with null characters, as in the C standard (it wrote one null
 * character only).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>
#include <libc.h>

#ifdef POK_NEEDS_DEBUG

size_t strlen (const char* str)
{
   size_t i = 0U;

   while (str[i] != '\0')
   {
      i++;
   }

   return i;
}

// TODO: move into own file?
size_t strnlen (const char* str, size_t n)
{
   size_t i = 0U;

   while ((i < n) && (str[i] != '\0'))
   {
      i++;
   }

   return i;
}

// TODO: move into own file?
char* strncpy(char* dest, const char* src, size_t n)
{
   size_t i = 0U;

   while ((i < n) && (src[i] != '\0')) {
      dest[i] = src[i];
      i++;
   }

   // The rest of the n bytes are filled with null characters.
   while (i < n) {
      dest[i] = '\0';
      i++;
   }

   return dest;
}

#endif
