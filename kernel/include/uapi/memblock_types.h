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

#ifndef JET_UAPI_MEMBLOCK_TYPES_H
#define JET_UAPI_MEMBLOCK_TYPES_H

#include <uapi/types.h>

typedef enum {
    JET_MEMORY_BLOCK_READ,
    JET_MEMORY_BLOCK_READ_WRITE
} jet_memory_block_mode_t;

typedef struct {
    uintptr_t addr;
    jet_memory_block_mode_t mode;
    size_t size;
} jet_memory_block_status_t;


#endif /* JET_UAPI_MEMBLOCK_TYPES_H */
