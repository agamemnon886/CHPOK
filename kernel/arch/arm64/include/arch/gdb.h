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
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its macros; the types and macros defined in the same way for x86_64 are
 * documented with their x86_64 definitions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros and the type
 * gdb_step_state_t of kernel/arch/arm64/gdb.c are moved here; the enum types
 * are named with _e: gdb_step_state_e (was gdb_step_state_t); the Doxygen
 * comment of the file is removed (header files are not documented).
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

/*
 * The number of registers in the g packet (see the table above).
 *
 * Documented with its x86_64 definition: kernel/arch/x86_64/include/arch/gdb.h.
 */
#define JA_GDB_NUM_REGS 68U

/*
 * Size of the largest register (v0-v31).
 *
 * Documented with its x86_64 definition: kernel/arch/x86_64/include/arch/gdb.h.
 */
#define JA_GDB_REGISTER_SIZE_MAX 16U

/*
 * Registers sent with every stop reply: the frame pointer (x29).
 *
 * Documented with its x86_64 definition: kernel/arch/x86_64/include/arch/gdb.h.
 */
#define JA_GDB_REG_FP 29U

/*
 * Registers sent with every stop reply: sp.
 *
 * Documented with its x86_64 definition: kernel/arch/x86_64/include/arch/gdb.h.
 */
#define JA_GDB_REG_SP 31U

/*
 * Registers sent with every stop reply: pc.
 *
 * Documented with its x86_64 definition: kernel/arch/x86_64/include/arch/gdb.h.
 */
#define JA_GDB_REG_PC 32U

/*
 * Size of the breakpoint instruction (brk #0).
 *
 * Documented with its x86_64 definition: kernel/arch/x86_64/include/arch/gdb.h.
 */
#define JA_GDB_BREAKPOINT_SIZE 4U

/**
 * @name Hardware breakpoints
 * Hardware breakpoints and watchpoints: the debug registers of the processor
 * (Cortex-A53, as in QEMU, has 6 and 4). The numbers the processor has are read
 * from ID_AA64DFR0_EL1; the GDB server uses no more than these.
 * @{
 */

/**
 * @macroname `JA_GDB_HW_BREAKPOINTS`
 *
 * @description
 * The number of hardware breakpoints the GDB server uses at most (the
 * processor's are read from ID_AA64DFR0_EL1).
 */
#define JA_GDB_HW_BREAKPOINTS 6U

/**
 * @macroname `JA_GDB_HW_WATCHPOINTS`
 *
 * @description
 * The number of watchpoints the GDB server uses at most.
 */
#define JA_GDB_HW_WATCHPOINTS 4U

/** @} */

/**
 * @name SPSR_EL1 bits
 * SPSR_EL1 (the saved state of the interrupted context): debug exceptions
 * masked, IRQs masked, software step, the mode, and the flags GDB may change.
 * @{
 */

/**
 * @macroname `SPSR_D`
 *
 * @description
 * Debug exceptions masked.
 */
#define SPSR_D       (1ULL << 9)

/**
 * @macroname `SPSR_I`
 *
 * @description
 * IRQs masked.
 */
#define SPSR_I       (1ULL << 7)

/**
 * @macroname `SPSR_SS`
 *
 * @description
 * Software step: the context steps after the exception return (with MDSCR_SS).
 */
#define SPSR_SS      (1ULL << 21)

/**
 * @macroname `SPSR_M_MASK`
 *
 * @description
 * SPSR_EL1.M[4:0], the mode: it is 0 for EL0 (AArch64).
 */
#define SPSR_M_MASK  0x1FULL

/**
 * @macroname `SPSR_NZCV`
 *
 * @description
 * The flags GDB may change: N, Z, C and V.
 */
#define SPSR_NZCV    0xF0000000ULL

/** @} */

/**
 * @macroname `DAIF_D`
 *
 * @description
 * DAIF: debug exceptions masked.
 */
#define DAIF_D       (1ULL << 9)

/**
 * @name MDSCR_EL1 bits
 * MDSCR_EL1: software step, kernel debug, debug exceptions enabled.
 * @{
 */

/**
 * @macroname `MDSCR_SS`
 *
 * @description
 * Software step enabled.
 */
#define MDSCR_SS     (1ULL << 0)

/**
 * @macroname `MDSCR_KDE`
 *
 * @description
 * Debug exceptions are taken at EL1 too (kernel debug).
 */
#define MDSCR_KDE    (1ULL << 13)

/**
 * @macroname `MDSCR_MDE`
 *
 * @description
 * Breakpoint and watchpoint exceptions enabled.
 */
#define MDSCR_MDE    (1ULL << 15)

/** @} */

/**
 * @name DBGBCRn_EL1 bits
 * DBGBCR<n>_EL1 (breakpoint control): enable; EL1 or EL0 (PMC); all 4 bytes of
 * the address.
 * @{
 */

/**
 * @macroname `BCR_E`
 *
 * @description
 * The breakpoint is enabled.
 */
#define BCR_E        1ULL

/**
 * @macroname `BCR_PMC_EL1`
 *
 * @description
 * It matches at EL1.
 */
#define BCR_PMC_EL1  (1ULL << 1)

/**
 * @macroname `BCR_PMC_EL0`
 *
 * @description
 * It matches at EL0.
 */
#define BCR_PMC_EL0  (2ULL << 1)

/**
 * @macroname `BCR_BAS_ALL`
 *
 * @description
 * It matches all 4 bytes of the instruction.
 */
#define BCR_BAS_ALL  (0xFULL << 5)

/** @} */

/**
 * @name DBGWCRn_EL1 bits
 * DBGWCR<n>_EL1 (watchpoint control): enable; EL1 or EL0 (PAC); load or store
 * (LSC); bytes.
 * @{
 */

/**
 * @macroname `WCR_E`
 *
 * @description
 * The watchpoint is enabled.
 */
#define WCR_E        1ULL

/**
 * @macroname `WCR_PAC_EL1`
 *
 * @description
 * It matches at EL1.
 */
#define WCR_PAC_EL1  (1ULL << 1)

/**
 * @macroname `WCR_PAC_EL0`
 *
 * @description
 * It matches at EL0.
 */
#define WCR_PAC_EL0  (2ULL << 1)

/**
 * @macroname `WCR_LSC_LOAD`
 *
 * @description
 * It matches loads.
 */
#define WCR_LSC_LOAD  (1ULL << 3)

/**
 * @macroname `WCR_LSC_STORE`
 *
 * @description
 * It matches stores.
 */
#define WCR_LSC_STORE (2ULL << 3)

/**
 * @macroname `WCR_BAS_SHIFT`
 *
 * @description
 * The shift of the byte address select field (the bytes of the doubleword
 * watched).
 */
#define WCR_BAS_SHIFT 5U

/** @} */

/**
 * @name GDB register numbers
 * GDB's numbers of the registers (see arch/gdb.h) that the code uses (x0 to x30
 * are 0 to 30).
 * @{
 */

/**
 * @macroname `REG_SP`
 *
 * @description
 * GDB's number of sp.
 */
#define REG_SP     31U

/**
 * @macroname `REG_PC`
 *
 * @description
 * GDB's number of pc.
 */
#define REG_PC     32U

/**
 * @macroname `REG_CPSR`
 *
 * @description
 * GDB's number of cpsr (PSTATE, from SPSR_EL1).
 */
#define REG_CPSR   33U

/**
 * @macroname `REG_V0`
 *
 * @description
 * GDB's number of v0 (v0 to v31 follow).
 */
#define REG_V0     34U

/**
 * @macroname `REG_V31`
 *
 * @description
 * GDB's number of v31.
 */
#define REG_V31    65U

/**
 * @macroname `REG_FPSR`
 *
 * @description
 * GDB's number of fpsr.
 */
#define REG_FPSR   66U

/**
 * @macroname `REG_FPCR`
 *
 * @description
 * GDB's number of fpcr.
 */
#define REG_FPCR   67U

/** @} */

/**
 * @name FPSR and FPCR masks
 * FPSR and FPCR bits which are defined (the others are not written).
 * @{
 */

/**
 * @macroname `FPSR_MASK`
 *
 * @description
 * The bits of FPSR that GDB may change.
 */
#define FPSR_MASK  0xF800009FULL

/**
 * @macroname `FPCR_MASK`
 *
 * @description
 * The bits of FPCR that GDB may change.
 */
#define FPCR_MASK  0x07FF9F00ULL

/** @} */

/**
 * @name Instructions
 * Instructions which change the interrupt mask or wait for interrupts, which a
 * single step doesn't step over as such.
 * @{
 */

/**
 * @macroname `INSN_WFI`
 *
 * @description
 * wfi.
 */
#define INSN_WFI         0xD503207FU

/**
 * @macroname `INSN_WFE`
 *
 * @description
 * wfe.
 */
#define INSN_WFE         0xD503205FU

/**
 * @macroname `INSN_ERET`
 *
 * @description
 * eret.
 */
#define INSN_ERET        0xD69F03E0U

/**
 * @macroname `INSN_MSR_DAIFIMM`
 *
 * @description
 * msr daifset/daifclr, \#imm (with INSN_MSR_DAIFIMM_MASK): the op2 field
 * differs in bit 5.
 */
#define INSN_MSR_DAIFIMM 0xD50340DFU

/**
 * @macroname `INSN_MSR_DAIFIMM_MASK`
 *
 * @description
 * The bits that are the same in every msr daifset/daifclr, \#imm.
 */
#define INSN_MSR_DAIFIMM_MASK 0xFFFFF0DFU

/**
 * @macroname `INSN_MSR_DAIF`
 *
 * @description
 * msr daif, xN (with INSN_MSR_DAIF_MASK).
 */
#define INSN_MSR_DAIF    0xD51B4220U

/**
 * @macroname `INSN_MSR_DAIF_MASK`
 *
 * @description
 * The bits that are the same in every msr daif, xN (all but the register).
 */
#define INSN_MSR_DAIF_MASK 0xFFFFFFE0U

/** @} */

/**
 * @typename `gdb_step_state_e`
 *
 * @description
 * Single step (see the description at the beginning of gdb.c).
 *
 *  - NONE: no step.
 *  - REQUESTED: GDB has asked for a step of the context 'step_frame'; it
 *    starts when the exception handler returns to it.
 *  - RUNNING: the step has started. The next exception ends the window.
 *  - RETURNING: an exception other than the step has come in the window
 *    (the instruction was a system call, or an IRQ has come); the step
 *    ends when the handler returns to the context 'step_frame'.
 */
typedef enum
{
    STEP_NONE,       /**< No step. */
    STEP_REQUESTED,  /**< GDB has asked for a step of step_frame. */
    STEP_RUNNING,    /**< The step has started. */
    STEP_RETURNING   /**< Another exception has come in the window. */
} gdb_step_state_e;

#endif /* JET_ARM64_GDB_H */
