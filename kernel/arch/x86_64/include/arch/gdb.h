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
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: the GDB server is ported to
 * x86_64: the header describes GDB's amd64 registers, the breakpoint
 * instruction and the debug address registers instead of stopping the
 * build.
 */

#ifndef JET_X86_64_GDB_H
#define JET_X86_64_GDB_H

/*
 * Registers of the GDB server (kernel/libc/gdbserver.c), numbered as in
 * GDB's default x86-64 description, which is also the layout of the 'g'
 * packet:
 *
 *   0-15   rax rbx rcx rdx rsi rdi rbp rsp r8-r15   8 bytes each
 *   16     rip                                      8 bytes
 *   17     eflags                                   4 bytes
 *   18-23  cs ss ds es fs gs                        4 bytes each
 *   24-31  st0-st7                                  10 bytes each
 *   32-39  fctrl fstat ftag fiseg fioff foseg fooff fop  4 bytes each
 *   40-55  xmm0-xmm15                               16 bytes each
 *   56     mxcsr                                    4 bytes
 */
#define JA_GDB_NUM_REGS 57U

/* Size of the largest register (xmm). */
#define JA_GDB_REGISTER_SIZE_MAX 16U

/* Registers sent with every stop reply: frame pointer, stack pointer, PC. */
#define JA_GDB_REG_FP 6U
#define JA_GDB_REG_SP 7U
#define JA_GDB_REG_PC 16U

/* Size of the breakpoint instruction (int3). */
#define JA_GDB_BREAKPOINT_SIZE 1U

/* Hardware breakpoints and watchpoints: debug address registers DR0-DR3. */
#define JA_GDB_HW_SLOTS 4U

#endif /* JET_X86_64_GDB_H */
