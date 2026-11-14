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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; the include guard defines its macro
 * (it was missing).
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * Modified by Kirill Brazhnikov, 2026-11-14: the declaration of __udivdi3() of
 * libpok/libc/misc/__udivdi3.c is moved here.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef LIBPOK_ARCH_H
#define LIBPOK_ARCH_H

#include <types.h>
#include <core/dependencies.h>

/** \cond */
void pok_arch_idle (void);

void pok_arch_yield (void);
/** \endcond */

#ifdef POK_CONFIG_NEEDS_FUNC_UDIVDI3

/*
 * MISRA C:2025 Rule 5.10 deviation: __udivdi3() is the 64-bit division
 * helper of the compiler's run-time library (libgcc), which the compiler
 * calls on 32-bit targets; it must have this reserved name and these types.
 */
/** \cond */
unsigned long long __udivdi3(unsigned long long num,
                             unsigned long long den);
/** \endcond */

#endif /* POK_CONFIG_NEEDS_FUNC_UDIVDI3 */

#endif /* LIBPOK_ARCH_H */
