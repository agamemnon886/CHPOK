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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; internal linkage for file-local definitions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * The allocator of the memory for the messages of buffers and blackboards
 * (ARINC 653 intra-partition communication): memory is taken from a fixed
 * area and never freed, but its state can be saved and restored.
 */

#include "arinc_alloc.h"
#if defined(POK_NEEDS_ARINC653_BUFFER) || defined(POK_NEEDS_ARINC653_BLACKBOARD)

#include <arinc_config.h>
#include <utils.h>

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): memory is carved out of the
 * heap by adding offsets to its start.
 */

static size_t arinc_intra_heap_size_current = 0;

/**
 * @funcname `arinc_alloc`
 *
 * @description
 * Allocates memory of a size from the message memory. The memory has the
 * alignment suitable for any object that fits into the size.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | [in] The size. |
 * | `alignment` | `size_t` | [in] The alignment. |
 *
 * @return The memory, or NULL if there isn't enough.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_config_messages_memory_size` | `size_t` | The size of the memory for messages (configuration) |
 * | `arinc_intra_heap` | `char *` | The memory for the messages of buffers and blackboards |
 * | `arinc_intra_heap_size_current` | `size_t` | The size of the message memory allocated |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size_end` | `size_t` | Its end |
 * | `size_start` | `size_t` | The start of the memory, from the start of the area |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `ALIGN()` with the arguments `arinc_intra_heap_size_current` and
 *    `alignment`, and store the result in `size_start`.
 * 2. Set `size_end` to `size_start + size`.
 * 3. If `size_end` is greater than `arinc_config_messages_memory_size`:
 *    1. Return `NULL`.
 * 4. Set `arinc_intra_heap_size_current` to `size_end`.
 * 5. Return `arinc_intra_heap + size_start`.
 *
 * @calls
 * - `ALIGN()`
 */
void* arinc_alloc(size_t size, size_t alignment)
{
    size_t size_start = ALIGN(arinc_intra_heap_size_current, alignment);
    size_t size_end = size_start + size;

    if(size_end > arinc_config_messages_memory_size) {
        return NULL;
    }

    arinc_intra_heap_size_current = size_end;

    return arinc_intra_heap + size_start;
}

/*
 * State of the arinc allocator.
 */

/**
 * @funcname `arinc_allocator_get_state`
 *
 * @description
 * Returns the current state of the allocator (the size allocated).
 *
 * @return The state.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_intra_heap_size_current` | `size_t` | The size of the message memory allocated |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `arinc_intra_heap_size_current`.
 *
 * @calls
 * None.
 */
arinc_allocator_state arinc_allocator_get_state(void)
{
    return arinc_intra_heap_size_current;
}

/**
 * @funcname `arinc_allocator_reset_state`
 *
 * @description
 * Restores the state of the allocator, previously obtained with
 * arinc_allocator_get_state() (frees what was allocated since).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `state` | `arinc_allocator_state` | [in] The state. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arinc_intra_heap_size_current` | `size_t` | The size of the message memory allocated |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `arinc_intra_heap_size_current` to `state`.
 *
 * @calls
 * None.
 */
void arinc_allocator_reset_state(arinc_allocator_state state)
{
    arinc_intra_heap_size_current = state;
}

#endif /* defined(POK_NEEDS_ARINC653_BUFFER) || defined(POK_NEEDS_ARINC653_BLACKBOARD) */
