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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/include/arch/stack.h for arm64.
 */

/* Stack for the (kernel) thread. */

#ifndef __JET_ARM64_STACK_H__
#define __JET_ARM64_STACK_H__

#include <stdint.h>

/*
 * Any stack returned by pok_stack_alloc() is 16-byte aligned (as
 * required by AAPCS64) and has a zeroed frame record (fp, lr) *above*
 * it, which terminates the frame chain.
 */
typedef uintptr_t jet_stack_t;

#endif /* __JET_ARM64_STACK_H__ */
