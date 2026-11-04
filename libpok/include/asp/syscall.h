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

#ifndef LIBJET_ASP_SYSCALL_H
#define LIBJET_ASP_SYSCALL_H

#include <config.h>

#include <uapi/syscall_types.h>

#include <arch/syscall.h>

#ifndef LIBJET_ARCH_DECLARE_SYSCALL

/* 
 * Call syscall with given id and number of arguments.
 * 
 * If arch defines macro 'LIBJET_ARCH_DECLARE_SYSCALL', it should provide
 * definitions with same usage.
 */
pok_ret_t lja_syscall0  (pok_syscall_id_t syscall_id);

pok_ret_t lja_syscall1  (pok_syscall_id_t syscall_id,
        uint32_t arg1);

pok_ret_t lja_syscall2  (pok_syscall_id_t syscall_id,
        uint32_t         arg1,
        uint32_t         arg2);

pok_ret_t lja_syscall3 (pok_syscall_id_t  syscall_id,
        uint32_t          arg1,
        uint32_t          arg2,
        uint32_t          arg3);

pok_ret_t lja_syscall4 (pok_syscall_id_t  syscall_id,
        uint32_t          arg1,
        uint32_t          arg2,
        uint32_t          arg3,
        uint32_t          arg4);

pok_ret_t lja_syscall5 (pok_syscall_id_t  syscall_id,
        uint32_t arg1,
        uint32_t arg2,
        uint32_t arg3,
        uint32_t arg4,
        uint32_t arg5);

#endif /* LIBJET_ARCH_DECLARE_SYSCALL */

#endif /* LIBJET_ASP_SYSCALL_H */
