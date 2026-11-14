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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the declaration of __pok_end of
 * kernel/arch/x86_64/alloc.c and kernel/arch/arm64/alloc.c (the same on both)
 * is moved here.
 */

#ifndef JET_ASP_ALLOC_H
#define JET_ASP_ALLOC_H

#include <types.h>

/* 
 * Allocate memory chunk of given size and with given alignment.
 * 
 * May be called only during OS initialization.
 * 
 * 'alignment' should be power of 2.
 * 
 * Note: Failure to allocate memory is treated as error
 * (that is function never returns NULL).
 */
/** \cond */
void* ja_mem_alloc_aligned(size_t size, uint32_t alignment);

/* 
 * Returned alignment suitable for any type which fits into the
 * requested size.
 */
uint32_t ja_mem_get_alignment(size_t size);
/** \endcond */

/* The end of the kernel image (a symbol of the linker script). */
extern char __pok_end[];

#endif /* JET_ASP_ALLOC_H */
