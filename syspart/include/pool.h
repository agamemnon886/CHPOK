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
 */


#ifndef SYSPART_POOL_H
#define SYSPART_POOL_H
#include <stdint.h>

struct pool_elem {
    int32_t idx;
    int32_t is_free;
    int32_t next_free_idx;
    int32_t data_len;
    /* MISRA C:2025 Rule 18.7 deviation: the element's data follows it,
     * elem_size bytes (see jet_pool_create()). */
    char data[];
};

struct pool {
    size_t elem_size;
    int32_t num;
    size_t stride;
    int32_t free_elem_idx; /* -1: no free element */
    char* data;
};

struct pool *jet_pool_create(size_t elem_size, int32_t num);


struct pool_elem * jet_pool_get_free_elem(struct pool *pool);

void jet_pool_free_elem(struct pool *pool, struct pool_elem *elem);

#endif
