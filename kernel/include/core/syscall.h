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
 * Modified by Kirill Brazhnikov, 2026-10-31: syscall arguments are
 * uintptr_t (64-bit support).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed; the core headers that
 * declare the syscall implementations are included, so that the generated
 * syscall map doesn't declare them again (Rule 8.5).
 */

#ifndef POK_SYSCALL_H
#define POK_SYSCALL_H

#include <config.h>

#include <types.h>
#include <errno.h>
#include <common.h>

#include <uapi/syscall_types.h>

/* The syscall implementations called by <uapi/syscall_map_arinc.h>. */
#include <core/thread.h>
#include <core/sched_arinc.h>
#include <core/partition_arinc.h>
#include <core/error_arinc.h>
#include <core/port.h>
#include <core/memblocks.h>

/*
 * Arguments are pointer-sized: user code passes pointers through them.
 *
 * Must match libpok's arch/syscall.h definition.
 */
typedef struct
{
   uint32_t             nargs;
   uintptr_t            arg1;
   uintptr_t            arg2;
   uintptr_t            arg3;
   uintptr_t            arg4;
   uintptr_t            arg5;
} pok_syscall_args_t;

typedef struct
{
   pok_partition_id_t   partition;
   uint32_t             thread;
   uint32_t             base_addr;
}pok_syscall_info_t;

/*
 * MISRA C:2025 Rule 20.1 deviation: the generated header uses the types
 * 'pok_syscall_args_t' and 'pok_syscall_info_t' defined above.
 */
#include <uapi/syscall_map_arinc.h>


/**
 *  Function that performs the syscall. It is called by the 
 *  architecture interruption handler.
 *
 *  @param syscall_id
 *  This param correspond to the syscal which should be performed.
 *  The list of available syscalls is available in
 *  the definition of the pok_syscall_id_t type
 *
 *  @param args
 *  Arguments of the syscall. It corresponds to data useful
 *  to perform the syscall.
 *
 *  @param infos
 *  Informations about the syscall: which partition/thread
 *  initiates the syscall, etc ...
 *
 *  @return
 *  Returns an error code, which is defined in include/errno.h
 */

pok_ret_t	pok_core_syscall (const pok_syscall_id_t     syscall_id,
                              const pok_syscall_args_t*  args,
                              const pok_syscall_info_t*  infos);

/*
 * Initiate syscalls.
 * This part is defined in low-level layers, each architecture/bsp
 * initiate the syscall in a different way.
 */
pok_ret_t pok_syscall_init(void);

#endif /* POK_SYSCALL_H */
