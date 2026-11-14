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
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of
 * kernel/arch/x86_64/gdb.c are moved here; the Doxygen comment of the file is
 * removed (header files are not documented).
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

/**
 * @macroname `JA_GDB_NUM_REGS`
 *
 * @description
 * The number of registers in the g packet, numbered as in GDB's description of
 * the architecture: 57 on x86_64, 68 on arm64 (see the tables in the comments
 * of kernel/arch/x86_64/include/arch/gdb.h and
 * kernel/arch/arm64/include/arch/gdb.h).
 */
#define JA_GDB_NUM_REGS 57U

/**
 * @macroname `JA_GDB_REGISTER_SIZE_MAX`
 *
 * @description
 * Size of the largest register (xmm on x86_64, v0-v31 on arm64: 16 bytes on
 * both).
 */
#define JA_GDB_REGISTER_SIZE_MAX 16U

/**
 * @macroname `JA_GDB_REG_FP`
 *
 * @description
 * Registers sent with every stop reply: the frame pointer (rbp, register 6, on
 * x86_64; x29, register 29, on arm64).
 */
#define JA_GDB_REG_FP 6U

/**
 * @macroname `JA_GDB_REG_SP`
 *
 * @description
 * Registers sent with every stop reply: the stack pointer (rsp, register 7, on
 * x86_64; sp, register 31, on arm64).
 */
#define JA_GDB_REG_SP 7U

/**
 * @macroname `JA_GDB_REG_PC`
 *
 * @description
 * Registers sent with every stop reply: the PC (rip, register 16, on x86_64;
 * pc, register 32, on arm64).
 */
#define JA_GDB_REG_PC 16U

/**
 * @macroname `JA_GDB_BREAKPOINT_SIZE`
 *
 * @description
 * Size of the breakpoint instruction: 1 byte on x86_64 (int3), 4 bytes on arm64
 * (brk #0).
 */
#define JA_GDB_BREAKPOINT_SIZE 1U

/**
 * @macroname `JA_GDB_HW_SLOTS`
 *
 * @description
 * Hardware breakpoints and watchpoints: debug address registers DR0-DR3.
 */
#define JA_GDB_HW_SLOTS 4U

/**
 * @name RFLAGS bits
 * RFLAGS: trap flag (single step), interrupts enabled, resume flag, and the
 * bits GDB may change.
 * @{
 */

/**
 * @macroname `RFLAGS_TF`
 *
 * @description
 * The trap flag: a debug exception after each instruction (single step).
 */
#define RFLAGS_TF           0x100U

/**
 * @macroname `RFLAGS_IF`
 *
 * @description
 * Interrupts are enabled.
 */
#define RFLAGS_IF           0x200U

/**
 * @macroname `RFLAGS_RF`
 *
 * @description
 * The resume flag: no instruction breakpoint exception for the next instruction
 * (resuming after a hardware breakpoint).
 */
#define RFLAGS_RF           0x10000U

/**
 * @macroname `RFLAGS_GDB_WRITABLE`
 *
 * @description
 * RFLAGS bits GDB may change: CF, PF, AF, ZF, SF, DF and OF.
 */
#define RFLAGS_GDB_WRITABLE 0xCD5U

/** @} */

/**
 * @name DR6 and DR7 bits
 * DR6: hits of DR0-DR3, single step; value with no event recorded. DR7: local
 * exact breakpoints.
 * @{
 */

/**
 * @macroname `DR6_HITS`
 *
 * @description
 * The breakpoint conditions of DR0 to DR3 that were met (one bit each).
 */
#define DR6_HITS  0xFU

/**
 * @macroname `DR6_BS`
 *
 * @description
 * The debug exception was a single step.
 */
#define DR6_BS    0x4000U

/**
 * @macroname `DR6_CLEAR`
 *
 * @description
 * The value of DR6 with no event recorded.
 */
#define DR6_CLEAR 0xFFFF0FF0U

/**
 * @macroname `DR7_LE`
 *
 * @description
 * DR7: local exact breakpoints (needed for data breakpoints on old CPUs).
 */
#define DR7_LE    0x100U

/** @} */

/**
 * @name GDB register numbers
 * GDB's numbers of the registers (see arch/gdb.h) that the code uses: the first
 * of each series and the special ones.
 * @{
 */

/**
 * @macroname `REG_RIP`
 *
 * @description
 * GDB's number of rip.
 */
#define REG_RIP    16U

/**
 * @macroname `REG_EFLAGS`
 *
 * @description
 * GDB's number of eflags.
 */
#define REG_EFLAGS 17U

/**
 * @macroname `REG_SS`
 *
 * @description
 * GDB's number of ss (the segment registers follow cs).
 */
#define REG_SS     19U

/**
 * @macroname `REG_ST0`
 *
 * @description
 * GDB's number of st0 (st0 to st7 follow).
 */
#define REG_ST0    24U

/**
 * @macroname `REG_FCTRL`
 *
 * @description
 * GDB's number of fctrl: the x87 control word.
 */
#define REG_FCTRL  32U

/**
 * @macroname `REG_FSTAT`
 *
 * @description
 * GDB's number of fstat: the x87 status word.
 */
#define REG_FSTAT  33U

/**
 * @macroname `REG_FTAG`
 *
 * @description
 * GDB's number of ftag: the x87 tag word.
 */
#define REG_FTAG   34U

/**
 * @macroname `REG_FISEG`
 *
 * @description
 * GDB's number of fiseg: the segment of the last x87 instruction.
 */
#define REG_FISEG  35U

/**
 * @macroname `REG_FIOFF`
 *
 * @description
 * GDB's number of fioff: the offset of the last x87 instruction.
 */
#define REG_FIOFF  36U

/**
 * @macroname `REG_FOSEG`
 *
 * @description
 * GDB's number of foseg: the segment of the last x87 operand.
 */
#define REG_FOSEG  37U

/**
 * @macroname `REG_FOOFF`
 *
 * @description
 * GDB's number of fooff: the offset of the last x87 operand.
 */
#define REG_FOOFF  38U

/**
 * @macroname `REG_FOP`
 *
 * @description
 * GDB's number of fop: the opcode of the last x87 instruction.
 */
#define REG_FOP    39U

/**
 * @macroname `REG_XMM0`
 *
 * @description
 * GDB's number of xmm0 (xmm0 to xmm15 follow).
 */
#define REG_XMM0   40U

/**
 * @macroname `REG_MXCSR`
 *
 * @description
 * GDB's number of mxcsr.
 */
#define REG_MXCSR  56U

/** @} */

/**
 * @name FXSAVE area
 * Offsets in the FXSAVE (64-bit) area of a thread (jet_fp_store_t), and the
 * sizes of its registers.
 * @{
 */

/**
 * @macroname `FX_FCW`
 *
 * @description
 * The x87 control word.
 */
#define FX_FCW        0U

/**
 * @macroname `FX_FSW`
 *
 * @description
 * The x87 status word.
 */
#define FX_FSW        2U

/**
 * @macroname `FX_FTW`
 *
 * @description
 * The abridged x87 tag word (one bit per register: not empty).
 */
#define FX_FTW        4U

/**
 * @macroname `FX_FOP`
 *
 * @description
 * The opcode of the last x87 instruction.
 */
#define FX_FOP        6U

/**
 * @macroname `FX_FIP`
 *
 * @description
 * The address of the last x87 instruction.
 */
#define FX_FIP        8U

/**
 * @macroname `FX_FDP`
 *
 * @description
 * The address of the last x87 operand.
 */
#define FX_FDP        16U

/**
 * @macroname `FX_MXCSR`
 *
 * @description
 * MXCSR.
 */
#define FX_MXCSR      24U

/**
 * @macroname `FX_MXCSR_MASK`
 *
 * @description
 * The bits of MXCSR that the processor supports (0: MXCSR_MASK_DEFAULT).
 */
#define FX_MXCSR_MASK 28U

/**
 * @macroname `FX_ST0`
 *
 * @description
 * st0 (st0 to st7 follow, FX_STRIDE bytes apart).
 */
#define FX_ST0        32U

/**
 * @macroname `FX_XMM0`
 *
 * @description
 * xmm0 (xmm0 to xmm15 follow, FX_STRIDE bytes apart).
 */
#define FX_XMM0       160U

/**
 * @macroname `FX_STRIDE`
 *
 * @description
 * st0-st7 and xmm0-xmm15 take 16 bytes each in the area.
 */
#define FX_STRIDE     16U

/**
 * @macroname `ST_SIZE`
 *
 * @description
 * The size of an x87 register (80 bits) in its slot.
 */
#define ST_SIZE       10U

/** @} */

/**
 * @macroname `MXCSR_MASK_DEFAULT`
 *
 * @description
 * MXCSR bits which may be set if the processor reports no mask.
 */
#define MXCSR_MASK_DEFAULT 0xFFBFU

/**
 * @name x87 tag values
 * x87 tag values: the state of a register in the full tag word that GDB reads
 * (ftag).
 * @{
 */

/**
 * @macroname `FP_TAG_VALID`
 *
 * @description
 * A valid number.
 */
#define FP_TAG_VALID   0U

/**
 * @macroname `FP_TAG_ZERO`
 *
 * @description
 * Zero.
 */
#define FP_TAG_ZERO    1U

/**
 * @macroname `FP_TAG_SPECIAL`
 *
 * @description
 * A special value (NaN, infinity, denormal).
 */
#define FP_TAG_SPECIAL 2U

/**
 * @macroname `FP_TAG_EMPTY`
 *
 * @description
 * An empty register.
 */
#define FP_TAG_EMPTY   3U

/** @} */

#endif /* JET_X86_64_GDB_H */
