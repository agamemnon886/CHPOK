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
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 */

#ifndef LIBJET_ARINC_ALLOC_H
#define LIBJET_ARINC_ALLOC_H

/* Allocator of memory for messages and message queues. */

#include <config.h>

#if defined(POK_NEEDS_ARINC653_BUFFER) || defined(POK_NEEDS_ARINC653_BLACKBOARD)

#include <types.h>

// Heap for ARINC messages and message queues. Used in arinc_init().
extern char* arinc_intra_heap;

/* 
 * Allocate memory of given size.
 * 
 * Memory will have alignment suitable for any object which fits into
 * given size.
 */
/** \cond */
void* arinc_alloc(size_t size, size_t alignment);
/** \endcond */

/**
 * @typename `arinc_allocator_state`
 *
 * @description
 * The state of the allocator of the ARINC objects (arinc_alloc()): how much of
 * its memory is used, to be restored with arinc_allocator_reset_state().
 */
typedef size_t arinc_allocator_state;

/*
 * Return current state of the allocator.
 */
/** \cond */
arinc_allocator_state arinc_allocator_get_state(void);

/*
 * Restore state of the arinc allocator.
 * State should be previously obtained with arinc_allocator_get_state().
 */
void arinc_allocator_reset_state(arinc_allocator_state state);
/** \endcond */

#endif /* defined(POK_NEEDS_ARINC653_BUFFER) || defined(POK_NEEDS_ARINC653_BLACKBOARD) */

#endif /* LIBJET_ARINC_ALLOC_H */
