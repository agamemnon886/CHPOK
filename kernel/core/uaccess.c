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
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 */

/*
 * Access to user space from kernel space.
 */

#include <core/uaccess.h>
#include <core/partition.h>

/* 
 * Return kernel address which can be used in the kernel for
 * r/w from/to current partition.
 * 
 * If given area cannot be write by the user space, returns NULL.
 */
void* JET_KUSER jet_user_to_kernel(void* JET_USER addr, size_t size)
{
    return ja_user_to_kernel_space(addr, size, current_partition->space_id);
}

/* 
 * Return kernel address which can be used in the kernel for
 * read from current partition.
 * 
 * If given area cannot be read by the user space, returns NULL.
 */
const void* JET_KUSER jet_user_to_kernel_ro(const void* JET_USER addr,
                                            size_t size)
{
    return ja_user_to_kernel_ro_space(addr, size, current_partition->space_id);
}

/*
 * Return true if address *may* contain instruction, which can be
 * executed by the user in the current partition.
 * 
 * Used mainly for additional checks.
 * Kernel doesn't perform r/w to given address.
 */
pok_bool_t jet_check_access_exec(void* JET_USER addr)
{
    return ja_check_access_exec(addr, current_partition->space_id);
}

void pok_copy_name_to_user(char* to, const char* name)
{
    pok_bool_t null_found = FALSE;

    for(int32_t i = 0; !null_found && (i < MAX_NAME_LENGTH); i++)
    {
        to[i] = name[i];
        if(name[i] == '\0') {
            null_found = TRUE;
        }
    }
}
