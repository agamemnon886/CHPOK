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
 * guard isn't a reserved identifier; __user/__kuser are renamed
 * JET_USER/JET_KUSER; essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 */

#ifndef JET_ASP_UACCESS_H
#define JET_ASP_UACCESS_H

#include <types.h>
#include <common.h>
#include <asp/space.h>

/* 
 * Return kernel address which can be used in the kernel for
 * r/w from/to given user space area.
 * 
 * If given area cannot be written by the user space, returns NULL.
 */
void* JET_KUSER ja_user_to_kernel_space(void* JET_USER addr, size_t size,
    jet_space_id space_id);

/* 
 * Return kernel address which can be used in the kernel for
 * read from given user space area.
 * 
 * If given area cannot be read by the user space, returns NULL.
 */
const void* JET_KUSER ja_user_to_kernel_ro_space(const void* JET_USER addr,
                                                 size_t size,
    jet_space_id space_id);

/*
 * Return true if address *may* contain instruction, which can be
 * executed by the user in the given space.
 * 
 * Used mainly for additional checks.
 * Kernel doesn't perform r/w to given address.
 */
pok_bool_t ja_check_access_exec(void* JET_USER addr, jet_space_id space_id);

#ifdef POK_NEEDS_GDB

/* 
 * Return address which can be used by GDB for
 * r/w from/to given kernel or user space area.
 * 
 * Access is garanteed only if switched to given space.
 * 
 * If given area cannot be written, returns NULL.
 */
void* ja_addr_to_gdb(void* addr, size_t size,
    jet_space_id space_id);

/* 
 * Return address which can be used by GDB for
 * read from given kernel or user space area.
 * 
 * Access is garanteed only if switched to given space.
 * 
 * If given area cannot be read by the user space, returns NULL.
 */
const void* ja_addr_to_gdb_ro(const void* addr, size_t size,
    jet_space_id space_id);

#endif /* POK_NEEDS_GDB */

#endif /* JET_ASP_UACCESS_H */
