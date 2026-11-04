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
 * kernel/arch/x86/include/arch/gdb.h to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 */

#ifndef JET_X86_64_GDB_H
#define JET_X86_64_GDB_H

/*
 * The gdb server (kernel/libc/gdbserver.c) supports only 32-bit
 * registers, so it is disabled on x86_64 (POK_DISABLE_GDB is set in
 * misc/SConscript) and no register description is provided here.
 *
 * QEMU's own gdb stub ('scons rundbg' + 'scons debug') can be used
 * instead.
 */
#ifndef POK_DISABLE_GDB
#error "GDB server is not supported on x86_64"
#endif

#endif /* JET_X86_64_GDB_H */
