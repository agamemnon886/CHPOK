/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 */

/* Stack for the (kernel) thread. */

#ifndef JET_ASP_STACK_H
#define JET_ASP_STACK_H

/* 
 * Arch header should define type 'jet_stack_t' as pointer to allocated
 * (kernel) stack.
 */
#include <arch/stack.h>

/*
 * Alloc (kernel) stack of specified size.
 * 
 * Return pointer to the head of the stack, that is when stack is assumed
 * to be empty.
 * 
 * May be called only during OS initialization.
 */
/** \cond */
jet_stack_t pok_stack_alloc(uint32_t stack_size);
/** \endcond */

#endif /* JET_ASP_STACK_H */
