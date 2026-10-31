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
 */


#ifndef __SYSPART_POOL_H__
#define __SYSPART_POOL_H__
#include <stdint.h>

struct pool_elem {
    int32_t idx;
    int32_t is_free;
    int32_t next_free_idx;
    int32_t data_len;
    char data[];
};

struct pool {
    size_t elem_size;
    uint32_t num;
    size_t stride;
    uint32_t free_elem_idx;
    char* data;
};

struct pool *jet_pool_create(size_t elem_size, int32_t num);


struct pool_elem * jet_pool_get_free_elem(struct pool *pool);

void jet_pool_free_elem(struct pool *pool, struct pool_elem *elem);

#endif
