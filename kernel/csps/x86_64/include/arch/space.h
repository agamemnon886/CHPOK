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
 * kernel/arch/x86/include/arch/space.h to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its types.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/x86_64/include/arch/space.h (the CPU support packages are in
 * kernel/csps); the comments name the new paths.
 */

/* (Memory) space. */

#ifndef JET_X86_64_SPACE_H
#define JET_X86_64_SPACE_H

#include <stdint.h>

/**
 * @typename `jet_ustack_t`
 *
 * @description
 * The address of a user stack (0: none). The same type on arm64
 * (kernel/csps/arm64/include/arch/space.h).
 */
typedef uintptr_t jet_ustack_t;

#endif /* JET_X86_64_SPACE_H */
