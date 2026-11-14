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
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file, its
 * types and its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * jet_memory_block_mode_e (was jet_memory_block_mode_t); the Doxygen comment of
 * the file is removed (header files are not documented).
 */

#ifndef JET_UAPI_MEMBLOCK_TYPES_H
#define JET_UAPI_MEMBLOCK_TYPES_H

#include <uapi/types.h>

/**
 * @typename `jet_memory_block_mode_e`
 *
 * @description
 * The access of a partition to a memory block.
 */
typedef enum {
    JET_MEMORY_BLOCK_READ, /**< Read-only. */
    JET_MEMORY_BLOCK_READ_WRITE /**< Read-write. */
} jet_memory_block_mode_e;

/**
 * @typename `jet_memory_block_status_t`
 *
 * @description
 * The status of a memory block as a partition sees it
 * (GET_MEMORY_BLOCK_STATUS): its address in the partition, its access and its
 * size.
 */
typedef struct {
    uintptr_t addr; /**< Its address in the partition. */
    jet_memory_block_mode_e mode; /**< The partition's access to it. */
    size_t size; /**< Its size, in bytes. */
} jet_memory_block_status_t;


#endif /* JET_UAPI_MEMBLOCK_TYPES_H */
