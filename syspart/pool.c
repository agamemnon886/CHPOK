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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; operands of mixed precedence are
 * parenthesized; deviations are documented where a guideline can't be
 * followed; the element count and the free index are int32_t, like the -1
 * that marks an empty pool.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the struct types are used by their
 * typedef names.
 */

/**
 * @file
 * Pools of elements of a fixed size, allocated at initialization, with a
 * free list.
 */

#include <mem.h>
#include <smalloc.h>
#include <pool.h>


/**
 * @funcname `get_pool_elem`
 *
 * @description
 * Returns the element of a pool at an index (elements are stride bytes apart in
 * the pool's byte array).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pool` | `pool_t *` | [in] The pool. |
 * | `idx` | `int32_t` | [in] The index. |
 *
 * @return The element.
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
 * 1. Return `(pool_elem_t *) (&pool->data[(size_t) idx * pool->stride])`.
 *
 * @calls
 * None.
 */
static pool_elem_t *get_pool_elem(pool_t *pool, int32_t idx)
{
    /* MISRA C:2025 Rule 11.3 deviation: the elements are laid out in a
     * byte array, stride bytes apart (aligned to uintptr_t). */
    return (pool_elem_t *) (&pool->data[(size_t) idx * pool->stride]);
}

/**
 * @funcname `jet_pool_create`
 *
 * @description
 * Creates a pool of elements of a size, all free.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `elem_size` | `size_t` | [in] The size of an element. |
 * | `num` | `int32_t` | [in] The number of elements. |
 *
 * @return The pool.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `elem` | `pool_elem_t *` | An element |
 * | `i` | `int32_t` | The index of an element |
 * | `pool` | `pool_t *` | The pool |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `smalloc()` with the argument `sizeof(*pool)`, and store the result
 *    in `pool`.
 * 2. Set `pool->elem_size` to `elem_size`.
 * 3. Set `pool->num` to `num`.
 * 4. Set `pool->stride` to
 *    `ALIGN_UP(pool->elem_size + sizeof(pool_elem_t), sizeof(uintptr_t))`.
 * 5. Call `smalloc()` with the argument `pool->stride * (size_t) num`, and
 *    store the result in `pool->data`.
 * 6. Set `pool->free_elem_idx` to `0`.
 * 7. Set `i` to `0`; then while `i` is less than `(num - 1)`, repeat (after
 *    each pass: increment `i`):
 *    1. Call `get_pool_elem()` with the arguments `pool` and `i`, and store the
 *       result in `elem`.
 *    2. Set `elem->next_free_idx` to `i + 1`.
 *    3. Set `elem->idx` to `i`.
 * 8. Call `get_pool_elem()` with the arguments `pool` and `num - 1`, and store
 *    the result in `elem`.
 * 9. Set `elem->next_free_idx` to `-1`.
 * 10. Return `pool`.
 *
 * @calls
 * - `smalloc()`
 * - `get_pool_elem()`
 */
pool_t *jet_pool_create(size_t elem_size, int32_t num)
{
    pool_t *pool;
    pool_elem_t *elem;

    pool = smalloc(sizeof(*pool));

    pool->elem_size = elem_size;
    pool->num = num;
    pool->stride = ALIGN_UP(pool->elem_size + sizeof(pool_elem_t),
                            sizeof(uintptr_t));
    pool->data = smalloc(pool->stride * (size_t) num);
    pool->free_elem_idx = 0;

    for (int32_t i = 0; i < (num - 1); i++) {
       elem = get_pool_elem(pool, i);
       elem->next_free_idx = i + 1;
       elem->idx = i;
    }
    elem = get_pool_elem(pool, num - 1);
    elem->next_free_idx = -1;

    return pool;
}

/**
 * @funcname `jet_pool_get_free_elem`
 *
 * @description
 * Takes a free element of a pool.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pool` | `pool_t *` | [in,out] The pool. |
 *
 * @return The element, or NULL if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `elem` | `pool_elem_t *` | The element |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `pool->free_elem_idx` is equal to `-1`:
 *    1. Return `NULL`.
 * 2. Call `get_pool_elem()` with the arguments `pool` and
 *    `pool->free_elem_idx`, and store the result in `elem`.
 * 3. Set `elem->is_free` to `0`.
 * 4. Set `pool->free_elem_idx` to `elem->next_free_idx`.
 * 5. Return `elem`.
 *
 * @calls
 * - `get_pool_elem()`
 */
pool_elem_t * jet_pool_get_free_elem(pool_t *pool)
{
    if (pool->free_elem_idx == -1) {
        return NULL;
    }

    pool_elem_t *elem = get_pool_elem(pool, pool->free_elem_idx);
    elem->is_free = 0;
    pool->free_elem_idx = elem->next_free_idx;
    return elem;
}

/**
 * @funcname `jet_pool_free_elem`
 *
 * @description
 * Returns an element to its pool.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pool` | `pool_t *` | [in,out] The pool. |
 * | `elem` | `pool_elem_t *` | [in,out] The element. |
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
 * 1. Set `elem->is_free` to `1`.
 * 2. Set `elem->next_free_idx` to `pool->free_elem_idx`.
 * 3. Set `pool->free_elem_idx` to `elem->idx`.
 *
 * @calls
 * None.
 */
void jet_pool_free_elem(pool_t *pool, pool_elem_t *elem)
{
    elem->is_free = 1;
    elem->next_free_idx = pool->free_elem_idx;
    pool->free_elem_idx = elem->idx;
}

