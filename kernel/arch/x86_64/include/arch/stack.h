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
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its types.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

/* Stack for the (kernel) thread. */

#ifndef JET_X86_64_STACK_H
#define JET_X86_64_STACK_H

#include <stdint.h>

/*
 * Any stack returned by pok_stack_alloc() is 16-byte aligned (as
 * required by the System V ABI) and has two zeroed words *above* it.
 *
 * So, whenever %rbp register points to the beginning of the stack,
 * it is treated as the last stack frame.
 */
/**
 * @typename `jet_stack_t`
 *
 * @description
 * The address of a kernel stack. Any stack returned by pok_stack_alloc() is
 * 16-byte aligned (as required by the ABI) and ends a frame chain: on x86_64 it
 * has two zeroed words *above* it, so that %%rbp pointing to its beginning is
 * treated as the last stack frame; on arm64
 * (kernel/arch/arm64/include/arch/stack.h, the same type) it has a zeroed frame
 * record (fp, lr) above it.
 */
typedef uintptr_t jet_stack_t;

#endif /* JET_X86_64_STACK_H */
