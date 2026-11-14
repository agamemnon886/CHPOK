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
 * Created by julien on Thu Jan 15 23:34:13 2009
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: name
 * parameters are const (they are only read), so callers can pass string
 * literals.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#ifndef JET_ARINC653_MEMBLOCKS
#define JET_ARINC653_MEMBLOCKS

#include <arinc653/types.h>

/**
 * @typename `MEMORY_BLOCK_NAME_TYPE`
 *
 * @description
 * The name of a memory block.
 */
typedef NAME_TYPE MEMORY_BLOCK_NAME_TYPE;

/**
 * @typename `MEMORY_BLOCK_MODE_TYPE`
 *
 * @description
 * The access of the partition to a memory block.
 */
typedef enum {
    MB_READ, /**< Read only. */
    MB_READ_WRITE /**< Read and write. */
} MEMORY_BLOCK_MODE_TYPE;

/**
 * @typename `MEMORY_BLOCK_SIZE_TYPE`
 *
 * @description
 * The size of a memory block.
 */
typedef APEX_INTEGER MEMORY_BLOCK_SIZE_TYPE;

/**
 * @typename `MEMORY_BLOCK_STATUS_TYPE`
 *
 * @description
 * The status of a memory block (GET_MEMORY_BLOCK_STATUS()).
 */
typedef struct {
    /**
     * The address of the block in the partition.
     */
    MESSAGE_ADDR_TYPE MEMORY_BLOCK_ADDR;
    /**
     * The access of the partition.
     */
    MEMORY_BLOCK_MODE_TYPE MEMORY_BLOCK_MODE;
    MEMORY_BLOCK_SIZE_TYPE MEMORY_BLOCK_SIZE; /**< The size of the block. */
} MEMORY_BLOCK_STATUS_TYPE;

/** \cond */
void GET_MEMORY_BLOCK_STATUS (
        /*in */ const MEMORY_BLOCK_NAME_TYPE   MEMORY_BLOCK_NAME,
        /*out*/ MEMORY_BLOCK_STATUS_TYPE *MEMORY_BLOCK_STATUS,
        /*out*/ RETURN_CODE_TYPE         *RETURN_CODE);
/** \endcond */

#endif
