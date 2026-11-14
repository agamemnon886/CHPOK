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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * Kernel memory allocation with the alignment the architecture wants for
 * the size.
 */

#include <alloc.h>

/**
 * @funcname `jet_mem_alloc`
 *
 * @description
 * Allocates kernel memory (at initialization), aligned as the architecture
 * wants for an object of that size.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | [in] The size in bytes. |
 *
 * @return The allocated memory.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `ja_mem_alloc_aligned()` with the arguments `size` and
 *    `ja_mem_get_alignment(size)`, and return its result.
 *
 * @calls
 * - `ja_mem_alloc_aligned()`
 * - `ja_mem_get_alignment()`
 */
void* jet_mem_alloc (size_t size)
{
    return ja_mem_alloc_aligned(size, ja_mem_get_alignment(size));
}
