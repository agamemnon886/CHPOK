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
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file; the
 * types and macros defined in the same way for x86_64 are documented with their
 * x86_64 definitions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/include/arch/stack.h (the CPU support packages are in
 * kernel/csps); the comments name the new paths.
 */

/* Stack for the (kernel) thread. */

#ifndef JET_ARM64_STACK_H
#define JET_ARM64_STACK_H

#include <stdint.h>

/*
 * Any stack returned by pok_stack_alloc() is 16-byte aligned (as
 * required by AAPCS64) and has a zeroed frame record (fp, lr) *above*
 * it, which terminates the frame chain.
 *
 * Documented with its x86_64 definition:
 * kernel/csps/x86_64/include/arch/stack.h.
 */
typedef uintptr_t jet_stack_t;

#endif /* JET_ARM64_STACK_H */
