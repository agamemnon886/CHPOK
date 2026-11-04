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
 */

#ifndef LIBJET_ALLOC_H
#define LIBJET_ALLOC_H

#include <types.h>
#include <asp/alloc.h>

/* 
 * Allocate memory region of given size and alignment.
 * 
 * May be used only in init mode.
 */
void* smalloc_aligned(size_t size, size_t alignment);

/* 
 * Allocate memory region of given size and alignment, suitable for any
 * object of size less-or-equal than given one.
 * 
 * May be used only in init mode.
 */
void* smalloc(size_t size);

/* 
 * Allocate memory region for array with given number of elements,
 * which element has given size and alignment, suitable for any
 * object of size less-or-equal than given one.
 * 
 * May be used only in init mode.
 */
void* scalloc(size_t nmemb, size_t size);


// free() is not supported.

#define libjet_mem_get_alignment libja_mem_get_alignment

// Set by the init, used by the alloc implementation.
extern char* heap_current;

#endif /* LIBJET_ALLOC_H */

