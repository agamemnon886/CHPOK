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
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/include/arch/stack.h to x86_64 (64-bit).
 */

/* Stack for the (kernel) thread. */

#ifndef __JET_X86_64_STACK_H__
#define __JET_X86_64_STACK_H__

#include <stdint.h>

/*
 * Any stack returned by pok_stack_alloc() is 16-byte aligned (as
 * required by the System V ABI) and has two zeroed words *above* it.
 *
 * So, whenever %rbp register points to the beginning of the stack,
 * it is treated as the last stack frame.
 */
typedef uintptr_t jet_stack_t;

#endif /* __JET_X86_64_STACK_H__ */
