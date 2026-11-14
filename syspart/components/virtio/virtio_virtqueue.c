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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: discarded
 * results are cast to void; U suffixes for unsigned constants; operands
 * of mixed precedence are parenthesized.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the struct types are used by their
 * typedef names.
 */

/**
 * @file
 * Virtio virtqueues: their memory and initial state.
 */

#include "virtio_virtqueue.h"

#include <smalloc.h>
#include <string.h>

/**
 * @funcname `virtio_virtqueue_setup`
 *
 * @description
 * Sets up a virtqueue of a size: allocates and zeroes its ring memory, lays the
 * rings out in it and links all descriptors into the free list.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vq` | `virtio_virtqueue_t *` | [out] The virtqueue. |
 * | `size` | `uint16_t` | [in] The number of descriptors. |
 * | `alignment` | `size_t` | [in] The alignment of the used ring. |
 *
 * @return The ring memory.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint16_t` | The index of a descriptor |
 * | `mem` | `void *` | The ring memory |
 * | `mem_size` | `size_t` | The size of the ring memory |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `vring_size()` with the arguments `size` and `alignment`, and store
 *    the result in `mem_size`.
 * 2. Call `smalloc_aligned()` with the arguments `mem_size` and `alignment`,
 *    and store the result in `mem`.
 * 3. Call `memset()` with the arguments `mem`, `0` and `mem_size`.
 * 4. Call `vring_init()` with the arguments the address of `vq->vring`, `size`,
 *    `mem` and `alignment`.
 * 5. Set `vq->free_index` to `0`.
 * 6. Set `vq->num_free` to `size`.
 * 7. Set `vq->last_seen_used` to `0`.
 * 8. Set `i` to `0U`; then while `i` is less than `(size - 1U)`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `vq->vring.desc[i].next` to `(uint16_t) (i + 1U)`.
 * 9. Return `mem`.
 *
 * @calls
 * - `vring_size()`
 * - `smalloc_aligned()`
 * - `memset()`
 * - `vring_init()`
 */
void* virtio_virtqueue_setup(virtio_virtqueue_t *vq, uint16_t size,
    size_t alignment)
{
    size_t mem_size = vring_size(size, alignment);

    void *mem = smalloc_aligned(mem_size, alignment);
    (void) memset(mem, 0, mem_size);

    vring_init(&vq->vring, size, mem, alignment);

    vq->free_index = 0;
    vq->num_free = size;
    vq->last_seen_used = 0;

    // establish linked list
    uint16_t i;
    for (i = 0U; i < (size - 1U); i++) {
        vq->vring.desc[i].next = (uint16_t) (i + 1U);
    }

    return mem;
}
