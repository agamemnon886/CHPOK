/*
 *                               POK header
 *
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team
 *
 * Created by julien on Wed Jan 21 23:07:15 2009
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 * Modified by Kirill Brazhnikov, 2026-11-13: struct memory_block is the typedef
 * memory_block_t, enum mb_config_rights is the typedef mb_config_rights_t;
 * Doxygen comments for the file, its types (with their members) and its macros.
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * mb_config_rights_e (was mb_config_rights_t); the Doxygen comment of the file
 * is removed (header files are not documented).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef POK_MEMORY_BLOCKS_H
#define POK_MEMORY_BLOCKS_H

#include <uapi/types.h>
// XXX HACK: should be fixed when move to proptree

/**
 * @macroname `MAX_PID`
 *
 * @description
 * The number of partitions whose access to a memory block the configuration can
 * give. XXX HACK: should be fixed when move to proptree.
 */
#define MAX_PID 32U

/**
 * @typename `mb_config_rights_e`
 *
 * @description
 * The access of a partition to a memory block.
 */
typedef enum {
    MB_CONFIG_NO_ACCESS, /**< No access. */
    MB_CONFIG_READ_ONLY, /**< Read-only. */
    MB_CONFIG_READ_WRITE, /**< Read-write. */
} mb_config_rights_e;

/**
 * @typename `memory_block_t`
 *
 * @description
 * A memory block of the configuration (jet_memory_blocks, in deployment.c).
 */
typedef struct {
    char name[MAX_NAME_LENGTH]; /**< Its name. */
    uint32_t virt_addr; /**< Its address in the partitions. */
    uint32_t size; /**< Its size, in bytes. */
    /**
     * The access of each partition (by index).
     */
    mb_config_rights_e pid_to_rights[MAX_PID];
} memory_block_t;

extern memory_block_t jet_memory_blocks[];
extern size_t jet_memory_blocks_n;
#endif
