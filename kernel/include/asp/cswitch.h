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
 * documented; struct jet_context is declared as the typedef jet_context_t here,
 * and used by that name; struct jet_interrupt_context is the typedef
 * interrupt_frame; Doxygen comments for the file.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef JET_ASP_CSWITCH_H
#define JET_ASP_CSWITCH_H

#include <asp/stack.h>

/* Opaque (kernel thread) context for switch to. */
/*
 * The context of a kernel thread, which each architecture defines
 * (kernel/arch/<arch>/thread.h); the rest of the kernel uses it by pointer.
 */
typedef struct jet_context jet_context_t;

/*
 * The registers saved on an interrupt or exception, which each
 * architecture defines (kernel/arch/<arch>/include/arch/
 * interrupt_frame.h).
 */
typedef struct jet_interrupt_context interrupt_frame;

/*
 * Initialize `context` on the given stack.
 * 
 * Return stack pointer, which can be used by pok_context_switch() to
 * jump into given entry with given stack.
 * 
 * Never returns NULL.
 */
/** \cond */
jet_context_t* ja_context_init(jet_stack_t sp, void (*entry)(void));

/*
 * Switch to context, stored in @new_sp.
 * 
 * Pointer (non-NULL) to the current context will stored in @old_sp.
 */
void ja_context_switch (jet_context_t** old_sp, jet_context_t* new_sp);

/*
 * Jump to context, stored in @new_sp.
 * 
 * Current context will be lost.
 */
void ja_context_jump(jet_context_t* new_sp);

/*
 * Jump to given entry with given stack.
 * 
 * Mainly used for restart current context.
 */
void ja_context_restart(jet_stack_t sp, void (*entry)(void));

/*
 * Jump to given entry with given stack.
 * 
 * Mainly used for restart current context.
 * 
 * Value, pointed by `new_context_p` will be set to the stack pointer,
 * "as if" pok_context_switch() has been called.
 * 
 * This is useful for change special NULL value of context pointer to
 * something else.
 */
void ja_context_restart_and_save(jet_stack_t sp, void (*entry)(void),
        jet_context_t** new_context_p);

/** \endcond */


#endif /* JET_ASP_CSWITCH_H */
