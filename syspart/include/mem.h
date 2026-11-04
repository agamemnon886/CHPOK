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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants;
 * deviations are documented where a guideline can't be followed.
 */

#ifndef POK_SYSNET_MEM_H
#define POK_SYSNET_MEM_H

#include <core/syscall.h>

#define ALIGN_UP(addr,size) (((addr)+((size)-1U))&(~((size)-1U)))

/*
 * MISRA C:2025 Rule 11.6 deviation: addresses are passed to and returned by
 * the kernel as integers (syscall arguments).
 */
static inline uintptr_t pok_virt_to_phys(void * virt) {
   return pok_syscall1(POK_SYSCALL_MEM_VIRT_TO_PHYS, (uintptr_t) virt);
}

static inline void* pok_phys_to_virt(uintptr_t phys) {
   return (void *) pok_syscall1(POK_SYSCALL_MEM_PHYS_TO_VIRT, phys);
}

#endif

