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
 * Created by julien on Tue Sep  8 07:39:08 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: syscall arguments are
 * passed as uintptr_t (64-bit support).
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * pok_port_virtual_destination() of the old POK interface.
 */


#include <core/dependencies.h>

#ifdef POK_NEEDS_PORTS_VIRTUAL

#include <types.h>
#include <errno.h>
#include <libc/stdio.h>
#include <core/syscall.h>
#include <middleware/port.h>

/**
 * @funcname `pok_port_virtual_destination`
 *
 * @description
 * Returns a destination of a virtual port (a syscall).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `const pok_port_id_t` | [in] The port. |
 * | `n` | `uint32_t` | [in] The number of the destination. |
 * | `result` | `uint32_t *` | [out] Where it is stored. |
 *
 * @return The result of the syscall.
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
 * 1. Do nothing.
 *
 * @calls
 * None.
 */
pok_ret_e pok_port_virtual_destination (const pok_port_id_t id, uint32_t n,
    uint32_t* result)
{
   return (pok_syscall3 (POK_SYSCALL_MIDDLEWARE_VIRTUAL_DESTINATION, (uintptr_t) id, (uintptr_t) n , (uintptr_t) result));
}

#endif
