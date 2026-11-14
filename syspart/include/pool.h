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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; deviations are documented where a
 * guideline can't be followed; the element count and the free index are
 * int32_t, like the -1 that marks an empty pool.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the types are typedefs without a
 * tag, used by their typedef names, and are documented with their macros.
 */


#ifndef SYSPART_POOL_H
#define SYSPART_POOL_H
#include <stdint.h>

/**
 * @typename `pool_elem_t`
 *
 * @description
 * An element of a pool: its state, followed by its data.
 */
typedef struct {
    int32_t idx; /**< Its index in the pool. */
    int32_t is_free; /**< Whether it is free. */
    /**
     * The index of the next free element (-1: none).
     */
    int32_t next_free_idx;
    int32_t data_len; /**< The length of its data. */
    /* MISRA C:2025 Rule 18.7 deviation: the element's data follows it,
     * elem_size bytes (see jet_pool_create()). */
    char data[]; /**< Its data. */
} pool_elem_t;

/**
 * @typename `pool_t`
 *
 * @description
 * A pool of elements of the same size, in one array.
 */
typedef struct {
    size_t elem_size; /**< The size of the data of an element. */
    int32_t num; /**< The number of elements. */
    size_t stride; /**< The distance between two elements. */
    int32_t free_elem_idx; /**< -1: no free element. */
    char* data; /**< The elements. */
} pool_t;

/** \cond */
pool_t *jet_pool_create(size_t elem_size, int32_t num);


pool_elem_t * jet_pool_get_free_elem(pool_t *pool);

void jet_pool_free_elem(pool_t *pool, pool_elem_t *elem);
/** \endcond */

#endif
