/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: the GDB server is ported to
 * arm64: the header describes GDB's AArch64 registers, the breakpoint
 * instruction and the debug registers instead of stopping the build.
 */

#ifndef JET_ARM64_GDB_H
#define JET_ARM64_GDB_H

/*
 * Registers of the GDB server (kernel/libc/gdbserver.c), numbered as in
 * GDB's default AArch64 description, which is also the layout of the 'g'
 * packet:
 *
 *   0-30   x0-x30                                   8 bytes each
 *   31     sp                                       8 bytes
 *   32     pc                                       8 bytes
 *   33     cpsr                                     4 bytes
 *   34-65  v0-v31                                   16 bytes each
 *   66     fpsr                                     4 bytes
 *   67     fpcr                                     4 bytes
 */
#define JA_GDB_NUM_REGS 68U

/* Size of the largest register (v0-v31). */
#define JA_GDB_REGISTER_SIZE_MAX 16U

/* Registers sent with every stop reply: frame pointer (x29), sp, pc. */
#define JA_GDB_REG_FP 29U
#define JA_GDB_REG_SP 31U
#define JA_GDB_REG_PC 32U

/* Size of the breakpoint instruction (brk #0). */
#define JA_GDB_BREAKPOINT_SIZE 4U

/*
 * Hardware breakpoints and watchpoints: the debug registers of the
 * processor (Cortex-A53, as in QEMU, has 6 and 4). The numbers the
 * processor has are read from ID_AA64DFR0_EL1; the GDB server uses no more
 * than these.
 */
#define JA_GDB_HW_BREAKPOINTS 6U
#define JA_GDB_HW_WATCHPOINTS 4U

#endif /* JET_ARM64_GDB_H */
