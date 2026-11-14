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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: name
 * parameters are const (they are only read), so callers can pass string
 * literals.
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: the kernel's memory block
 * mode is converted to MEMORY_BLOCK_MODE_TYPE explicitly.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * The ARINC 653 service GET_MEMORY_BLOCK_STATUS.
 */

#include <config.h>
#include <arinc653/memblocks.h>
#include <core/syscall.h>
#include <uapi/memblock_types.h>



/**
 * @funcname `GET_MEMORY_BLOCK_STATUS`
 *
 * @description
 * Returns the status of a memory block by name (ARINC 653 service): its
 * address, access mode and size.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `MEMORY_BLOCK_NAME` | `const MEMORY_BLOCK_NAME_TYPE` | [in] The name. |
 * | `MEMORY_BLOCK_STATUS` | `MEMORY_BLOCK_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_CONFIG. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `mb_status` | `jet_memory_block_status_t` | The kernel's status of the block |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_memory_block_get_status()` with the arguments
 *    `MEMORY_BLOCK_NAME` and the address of `mb_status`, and store the result
 *    in `core_ret`.
 * 2. If `core_ret` is not equal to `POK_ERRNO_OK`:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 * 3. Otherwise:
 *    1. Set `*RETURN_CODE` to `NO_ERROR`.
 *    2. Set `MEMORY_BLOCK_STATUS->MEMORY_BLOCK_ADDR` to
 *       `(void *)mb_status.addr`.
 *    3. If `mb_status.mode` is equal to `JET_MEMORY_BLOCK_READ_WRITE`:
 *       1. Set `MEMORY_BLOCK_STATUS->MEMORY_BLOCK_MODE` to `MB_READ_WRITE`.
 *    4. Otherwise:
 *       1. Set `MEMORY_BLOCK_STATUS->MEMORY_BLOCK_MODE` to `MB_READ`.
 *    5. Set `MEMORY_BLOCK_STATUS->MEMORY_BLOCK_SIZE` to `mb_status.size`.
 *
 * @calls
 * - `pok_memory_block_get_status()`
 */
void GET_MEMORY_BLOCK_STATUS (
        /*in */ const MEMORY_BLOCK_NAME_TYPE   MEMORY_BLOCK_NAME,
        /*out*/ MEMORY_BLOCK_STATUS_TYPE *MEMORY_BLOCK_STATUS,
        /*out*/ RETURN_CODE_TYPE         *RETURN_CODE)
{
    pok_ret_e core_ret;
    jet_memory_block_status_t mb_status;


    core_ret = pok_memory_block_get_status(MEMORY_BLOCK_NAME,
        &mb_status);




    if (core_ret != POK_ERRNO_OK) {
        *RETURN_CODE = INVALID_CONFIG;
    } else {
        *RETURN_CODE = NO_ERROR;
        /* MISRA C:2025 Rule 11.6 deviation: the kernel reports the address of
         * the block as an integer. */
        MEMORY_BLOCK_STATUS->MEMORY_BLOCK_ADDR = (void *)mb_status.addr;
        if (mb_status.mode == JET_MEMORY_BLOCK_READ_WRITE) {
            MEMORY_BLOCK_STATUS->MEMORY_BLOCK_MODE = MB_READ_WRITE;
        } else {
            MEMORY_BLOCK_STATUS->MEMORY_BLOCK_MODE = MB_READ;
        }
        MEMORY_BLOCK_STATUS->MEMORY_BLOCK_SIZE = mb_status.size;
    }

}
