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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
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
 * - \hlrid{HLR_ALLOC_JET_MEM_ALLOC_01}: `jet_mem_alloc()` shall give the kernel
 *   memory of a requested size from the kernel heap, aligned as the
 *   architecture requires for an object of that size.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_010}
 *
 * @llr
 * - \llrid{LLR_ALLOC_JET_MEM_ALLOC_01}: `jet_mem_alloc()` shall return the
 *   result of `ja_mem_alloc_aligned()` called with `size` and the alignment
 *   that `ja_mem_get_alignment()` gives for `size`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ALLOC_JET_MEM_ALLOC_01}
 *
 * @design
 * 1. Call `ja_mem_alloc_aligned()` with the arguments `size` and
 *    `ja_mem_get_alignment(size)`, and return its result.
 *    \llrref{LLR_ALLOC_JET_MEM_ALLOC_01}
 *
 * @calls
 * - `ja_mem_alloc_aligned()`
 * - `ja_mem_get_alignment()`
 */
void* jet_mem_alloc (size_t size)
{
    return ja_mem_alloc_aligned(size, ja_mem_get_alignment(size));
}
