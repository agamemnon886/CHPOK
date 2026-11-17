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
 * Modified by Kirill Brazhnikov, 2026-11-05: adapted from
 * kernel/arch/x86_64/gdb.c (ported from kernel/arch/x86/gdb.c, removed with
 * the 32-bit x86 port) for arm64: GDB's AArch64 registers, including the
 * FP/SIMD ones, are read and written one at a time; breakpoint (brk),
 * software step, hardware breakpoint and watchpoint exceptions are passed
 * to the GDB server; single-stepping and hardware breakpoints and
 * watchpoints use the debug registers, also in the kernel; IRQs are masked
 * for the stepped instruction, so that no other thread runs before the step
 * is done.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: interrupt_frame, ja_gdb_hw_type_t, jet_fp_store_t,
 * jet_gdb_stop_reason_t; the macros and the types are moved to
 * kernel/arch/arm64/include/arch/gdb.h (hw_slot_t, the same on x86_64:
 * kernel/include/asp/gdb.h); struct hw_slot is the typedef hw_slot_t and enum
 * gdb_step_state the typedef gdb_step_state_t; the exception classes of debug
 * exceptions are in interrupt.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * gdb_step_state_e (was gdb_step_state_t), ja_gdb_hw_type_e (was
 * ja_gdb_hw_type_t), jet_gdb_stop_reason_e (was jet_gdb_stop_reason_t).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/gdb.c (the CPU support packages are in kernel/csps); the
 * comments name the new paths; sysreg.h is include/csps/arm64/cpu/sysreg.h,
 * shared with the hypervisor.
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
 */

/**
 * @file
 * Architecture part of the GDB server (kernel/debug/gdbserver.c) on
 * arm64.
 *
 * Debug exceptions. PSTATE.D masks them at the current exception level
 * (those from EL0 are never masked). The kernel runs with them unmasked
 * (ja_gdb_init()), so that hardware breakpoints and watchpoints work in
 * the kernel code too, and with MDSCR_EL1.KDE set. They are masked
 *
 *  - by the processor, when it takes an exception: so the GDB server
 *    and the code which saves and restores the registers never meet a
 *    hardware breakpoint or a step;
 *  - explicitly, in the code which leaves an exception handler, so that
 *    no exception comes when the return state (ELR_EL1, SPSR_EL1) is
 *    loaded, see ja_gdb_exception_leave().
 *
 * Single step. The processor steps if MDSCR_EL1.SS is set and the saved
 * state (SPSR_EL1.SS) which an exception return loads is 1; the step
 * exception follows the stepped instruction. MDSCR_EL1.SS is global, so
 * it is set only for the return to the context which steps, and cleared
 * by the first exception after that ("the step window"). If that exception
 * isn't the step exception (the instruction was a system call, or an IRQ
 * has come), the step is finished on the return to the same context.
 */

#include <config.h>

#ifdef POK_NEEDS_GDB

#include <asp/gdb.h>
#include <asp/space.h>
#include <asp/uaccess.h>
#include <gdb.h>
#include <libc.h>

#include "interrupt.h"
#include "space.h"
#include <cpu/sysreg.h>

/*
 * MISRA C:2025 Dir 1.2 deviation (this file): read_sysreg() is a GNU
 * statement expression, see sysreg.h.
 *
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (the cpu_*() and hw_*_write()
 * functions): system registers, the debug registers and cache maintenance
 * are accessed with GNU inline assembly, each access isolated in a small
 * function.
 */

/* The breakpoint instruction: brk #0. */
const uint8_t ja_gdb_breakpoint_insn[JA_GDB_BREAKPOINT_SIZE] = {
    0x00U, 0x00U, 0x20U, 0xD4U
};

/**
 * @funcname `cpu_debug_mask`
 *
 * @description
 * Masks debug exceptions at EL1 (DAIF.D).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_DEBUG_MASK_01}: `cpu_debug_mask()` shall mask the debug
 *   exceptions at exception level 1.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_DEBUG_MASK_01}: `cpu_debug_mask()` shall execute the
 *   instruction `msr daifset, #8` once, which sets the D bit of DAIF.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_DEBUG_MASK_01}
 *
 * @design
 * 1. Execute the assembly code `msr daifset, #8`.
 *    \llrref{LLR_GDB_CPU_DEBUG_MASK_01}
 *
 * @calls
 * None.
 */
static void cpu_debug_mask(void)
{
    asm volatile ("msr daifset, #8" ::: "memory");
}

/**
 * @funcname `cpu_debug_unmask`
 *
 * @description
 * Unmasks debug exceptions at EL1 (DAIF.D).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_DEBUG_UNMASK_01}: `cpu_debug_unmask()` shall unmask the
 *   debug exceptions at exception level 1.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_DEBUG_UNMASK_01}: `cpu_debug_unmask()` shall execute the
 *   instruction `msr daifclr, #8` once, which clears the D bit of DAIF.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_DEBUG_UNMASK_01}
 *
 * @design
 * 1. Execute the assembly code `msr daifclr, #8`.
 *    \llrref{LLR_GDB_CPU_DEBUG_UNMASK_01}
 *
 * @calls
 * None.
 */
static void cpu_debug_unmask(void)
{
    asm volatile ("msr daifclr, #8" ::: "memory");
}

/**
 * @funcname `cpu_debug_masked`
 *
 * @description
 * Tells whether debug exceptions are masked at EL1.
 *
 * @return TRUE if they are.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_DEBUG_MASKED_01}: `cpu_debug_masked()` shall tell
 *   whether the debug exceptions are masked.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_DEBUG_MASKED_01}: If `DAIF_D` is set in DAIF
 *   (`read_sysreg()`), `cpu_debug_masked()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_DEBUG_MASKED_01}
 * - \llrid{LLR_GDB_CPU_DEBUG_MASKED_02}: If `DAIF_D` is not set in DAIF
 *   (`read_sysreg()`), `cpu_debug_masked()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_DEBUG_MASKED_01}
 *
 * @design
 * 1. Return `(read_sysreg(daif) & DAIF_D) != 0U`.
 *    \llrref{LLR_GDB_CPU_DEBUG_MASKED_01}, \llrref{LLR_GDB_CPU_DEBUG_MASKED_02}
 *
 * @calls
 * None.
 */
static pok_bool_t cpu_debug_masked(void)
{
    return (read_sysreg(daif) & DAIF_D) != 0U;
}

/**
 * @funcname `cpu_mdscr_set`
 *
 * @description
 * Sets bits of MDSCR_EL1.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bits` | `uint64_t` | [in] The bits. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mdscr` | `uint64_t` | MDSCR_EL1 |
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_MDSCR_SET_01}: `cpu_mdscr_set()` shall set bits of the
 *   debug system control register.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_MDSCR_SET_01}: `cpu_mdscr_set()` shall write the value
 *   of MDSCR_EL1 back with `bits` set (`read_sysreg()`, `write_sysreg()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_MDSCR_SET_01}
 *
 * @design
 * 1. Set `mdscr` to `read_sysreg(mdscr_el1)`. \llrref{LLR_GDB_CPU_MDSCR_SET_01}
 * 2. Use the macro `write_sysreg()` with the arguments `mdscr_el1` and
 *    `mdscr | bits`. \llrref{LLR_GDB_CPU_MDSCR_SET_01}
 *
 * @calls
 * None.
 */
static void cpu_mdscr_set(uint64_t bits)
{
    uint64_t mdscr = read_sysreg(mdscr_el1);

    write_sysreg(mdscr_el1, mdscr | bits);
}

/**
 * @funcname `cpu_mdscr_clear`
 *
 * @description
 * Clears bits of MDSCR_EL1.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bits` | `uint64_t` | [in] The bits. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mdscr` | `uint64_t` | MDSCR_EL1 |
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_MDSCR_CLEAR_01}: `cpu_mdscr_clear()` shall clear bits of
 *   the debug system control register.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_MDSCR_CLEAR_01}: `cpu_mdscr_clear()` shall write the
 *   value of MDSCR_EL1 back with `bits` cleared (`read_sysreg()`,
 *   `write_sysreg()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_MDSCR_CLEAR_01}
 *
 * @design
 * 1. Set `mdscr` to `read_sysreg(mdscr_el1)`.
 *    \llrref{LLR_GDB_CPU_MDSCR_CLEAR_01}
 * 2. Use the macro `write_sysreg()` with the arguments `mdscr_el1` and
 *    `mdscr & ~bits`. \llrref{LLR_GDB_CPU_MDSCR_CLEAR_01}
 *
 * @calls
 * None.
 */
static void cpu_mdscr_clear(uint64_t bits)
{
    uint64_t mdscr = read_sysreg(mdscr_el1);

    write_sysreg(mdscr_el1, mdscr & ~bits);
}

/**
 * @funcname `cpu_os_lock_clear`
 *
 * @description
 * Clears the OS lock, which is set at reset and blocks the debug exceptions.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_OS_LOCK_CLEAR_01}: `cpu_os_lock_clear()` shall clear the
 *   OS lock, which blocks the debug exceptions after reset.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_OS_LOCK_CLEAR_01}: `cpu_os_lock_clear()` shall execute
 *   the instruction `msr oslar_el1, xzr` once, which writes 0 to OSLAR_EL1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_OS_LOCK_CLEAR_01}
 *
 * @design
 * 1. Execute the assembly code `msr oslar_el1, xzr`.
 *    \llrref{LLR_GDB_CPU_OS_LOCK_CLEAR_01}
 *
 * @calls
 * None.
 */
static void cpu_os_lock_clear(void)
{
    asm volatile ("msr oslar_el1, xzr" ::: "memory");
}

/**
 * @funcname `cpu_dcache_clean`
 *
 * @description
 * Cleans the data cache line of an address to the point of unification.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uintptr_t` | [in] The address. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_DCACHE_CLEAN_01}: `cpu_dcache_clean()` shall clean the
 *   data cache line of an address to the point of unification.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_DCACHE_CLEAN_01}: `cpu_dcache_clean()` shall execute the
 *   instruction `dc cvau` once with `addr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_DCACHE_CLEAN_01}
 *
 * @design
 * 1. Execute the assembly code `dc cvau, %0`.
 *    \llrref{LLR_GDB_CPU_DCACHE_CLEAN_01}
 *
 * @calls
 * None.
 */
static void cpu_dcache_clean(uintptr_t addr)
{
    asm volatile ("dc cvau, %0" : : "r" (addr) : "memory");
}

/**
 * @funcname `cpu_icache_invalidate`
 *
 * @description
 * Invalidates the instruction cache, after the data caches are clean.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_CPU_ICACHE_INVALIDATE_01}: `cpu_icache_invalidate()` shall
 *   invalidate the instruction cache once the data caches are clean.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_CPU_ICACHE_INVALIDATE_01}: `cpu_icache_invalidate()` shall
 *   execute, in this order, the instructions `dsb ish`, `ic iallu`, `dsb ish`
 *   and `isb`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_CPU_ICACHE_INVALIDATE_01}
 *
 * @design
 * 1. Execute the assembly code `dsb ish; ic iallu; dsb ish; isb`.
 *    \llrref{LLR_GDB_CPU_ICACHE_INVALIDATE_01}
 *
 * @calls
 * None.
 */
static void cpu_icache_invalidate(void)
{
    asm volatile ("dsb ish\n ic iallu\n dsb ish\n isb" ::: "memory");
}

/**
 * @funcname `hw_breakpoint_write`
 *
 * @description
 * Writes a breakpoint register pair DBGBVR<n>_EL1 and DBGBCR<n>_EL1 (the
 * control is disabled while the value changes).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `uint32_t` | [in] The register's number (0-5). |
 * | `value` | `uint64_t` | [in] The address. |
 * | `control` | `uint64_t` | [in] The control (0 to disable). |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_HW_BREAKPOINT_WRITE_01}: `hw_breakpoint_write()` shall write
 *   one of the six pairs of breakpoint registers, with the control disabled
 *   while the address changes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_HW_BREAKPOINT_WRITE_01}: If `n` is from 0 to 5,
 *   `hw_breakpoint_write()` shall write with `write_sysreg()`, in this order, 0
 *   to the breakpoint control register `n` (DBGBCR0_EL1 to DBGBCR5_EL1),
 *   `value` to the breakpoint value register `n` (DBGBVR0_EL1 to DBGBVR5_EL1)
 *   and `control` to the breakpoint control register `n`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_BREAKPOINT_WRITE_01}
 * - \llrid{LLR_GDB_HW_BREAKPOINT_WRITE_02}: If `n` is greater than 5,
 *   `hw_breakpoint_write()` shall write no register.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_BREAKPOINT_WRITE_01}
 *
 * @design
 * 1. Depending on `n`: \llrref{LLR_GDB_HW_BREAKPOINT_WRITE_01},
 *    \llrref{LLR_GDB_HW_BREAKPOINT_WRITE_02}
 *    1. Case `0U`: \llrref{LLR_GDB_HW_BREAKPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr0_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr0_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr0_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    2. Case `1U`: \llrref{LLR_GDB_HW_BREAKPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr1_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr1_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr1_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    3. Case `2U`: \llrref{LLR_GDB_HW_BREAKPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr2_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr2_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr2_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    4. Case `3U`: \llrref{LLR_GDB_HW_BREAKPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr3_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr3_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr3_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    5. Case `4U`: \llrref{LLR_GDB_HW_BREAKPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr4_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr4_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr4_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    6. Case `5U`: \llrref{LLR_GDB_HW_BREAKPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr5_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr5_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr5_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    7. Otherwise (default): \llrref{LLR_GDB_HW_BREAKPOINT_WRITE_02}
 *       1. Leave the switch.
 *
 * @calls
 * None.
 */
static void hw_breakpoint_write(uint32_t n, uint64_t value, uint64_t control)
{
    switch (n)
    {
    case 0U:
        write_sysreg(dbgbcr0_el1, 0U);
        write_sysreg(dbgbvr0_el1, value);
        write_sysreg(dbgbcr0_el1, control);
        break;
    case 1U:
        write_sysreg(dbgbcr1_el1, 0U);
        write_sysreg(dbgbvr1_el1, value);
        write_sysreg(dbgbcr1_el1, control);
        break;
    case 2U:
        write_sysreg(dbgbcr2_el1, 0U);
        write_sysreg(dbgbvr2_el1, value);
        write_sysreg(dbgbcr2_el1, control);
        break;
    case 3U:
        write_sysreg(dbgbcr3_el1, 0U);
        write_sysreg(dbgbvr3_el1, value);
        write_sysreg(dbgbcr3_el1, control);
        break;
    case 4U:
        write_sysreg(dbgbcr4_el1, 0U);
        write_sysreg(dbgbvr4_el1, value);
        write_sysreg(dbgbcr4_el1, control);
        break;
    case 5U:
        write_sysreg(dbgbcr5_el1, 0U);
        write_sysreg(dbgbvr5_el1, value);
        write_sysreg(dbgbcr5_el1, control);
        break;
    default:
        // JA_GDB_HW_BREAKPOINTS is 6.
        break;
    }
}

/**
 * @funcname `hw_watchpoint_write`
 *
 * @description
 * Writes a watchpoint register pair DBGWVR<n>_EL1 and DBGWCR<n>_EL1 (the
 * control is disabled while the value changes).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `uint32_t` | [in] The register's number (0-3). |
 * | `value` | `uint64_t` | [in] The address. |
 * | `control` | `uint64_t` | [in] The control (0 to disable). |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_HW_WATCHPOINT_WRITE_01}: `hw_watchpoint_write()` shall write
 *   one of the four pairs of watchpoint registers, with the control disabled
 *   while the address changes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_HW_WATCHPOINT_WRITE_01}: If `n` is from 0 to 3,
 *   `hw_watchpoint_write()` shall write with `write_sysreg()`, in this order, 0
 *   to the watchpoint control register `n` (DBGWCR0_EL1 to DBGWCR3_EL1),
 *   `value` to the watchpoint value register `n` (DBGWVR0_EL1 to DBGWVR3_EL1)
 *   and `control` to the watchpoint control register `n`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_WATCHPOINT_WRITE_01}
 * - \llrid{LLR_GDB_HW_WATCHPOINT_WRITE_02}: If `n` is greater than 3,
 *   `hw_watchpoint_write()` shall write no register.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_WATCHPOINT_WRITE_01}
 *
 * @design
 * 1. Depending on `n`: \llrref{LLR_GDB_HW_WATCHPOINT_WRITE_01},
 *    \llrref{LLR_GDB_HW_WATCHPOINT_WRITE_02}
 *    1. Case `0U`: \llrref{LLR_GDB_HW_WATCHPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgwcr0_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgwvr0_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgwcr0_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    2. Case `1U`: \llrref{LLR_GDB_HW_WATCHPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgwcr1_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgwvr1_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgwcr1_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    3. Case `2U`: \llrref{LLR_GDB_HW_WATCHPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgwcr2_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgwvr2_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgwcr2_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    4. Case `3U`: \llrref{LLR_GDB_HW_WATCHPOINT_WRITE_01}
 *       1. Use the macro `write_sysreg()` with the arguments `dbgwcr3_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgwvr3_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgwcr3_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    5. Otherwise (default): \llrref{LLR_GDB_HW_WATCHPOINT_WRITE_02}
 *       1. Leave the switch.
 *
 * @calls
 * None.
 */
static void hw_watchpoint_write(uint32_t n, uint64_t value, uint64_t control)
{
    switch (n)
    {
    case 0U:
        write_sysreg(dbgwcr0_el1, 0U);
        write_sysreg(dbgwvr0_el1, value);
        write_sysreg(dbgwcr0_el1, control);
        break;
    case 1U:
        write_sysreg(dbgwcr1_el1, 0U);
        write_sysreg(dbgwvr1_el1, value);
        write_sysreg(dbgwcr1_el1, control);
        break;
    case 2U:
        write_sysreg(dbgwcr2_el1, 0U);
        write_sysreg(dbgwvr2_el1, value);
        write_sysreg(dbgwcr2_el1, control);
        break;
    case 3U:
        write_sysreg(dbgwcr3_el1, 0U);
        write_sysreg(dbgwvr3_el1, value);
        write_sysreg(dbgwcr3_el1, control);
        break;
    default:
        // JA_GDB_HW_WATCHPOINTS is 4.
        break;
    }
}

/**
 * @funcname `put_le`
 *
 * @description
 * Stores the low bytes of a value, little-endian.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | [in] The value. |
 * | `size` | `size_t` | [in] The number of bytes. |
 * | `out` | `uint8_t *` | [out] Where they are stored. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_PUT_LE_01}: `put_le()` shall store the low bytes of a
 *   value in little-endian order, the order of the registers in the protocol.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_PUT_LE_01}: `put_le()` shall store the bits `8 * i +
 *   7` to `8 * i` of `value` at `out[i]`, for each `i` less than `size`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_PUT_LE_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GDB_ARM64_PUT_LE_01}
 *    1. Set `out[i]` to `(uint8_t)(value >> (8U * i))`.
 *
 * @calls
 * None.
 */
static void put_le(uint64_t value, size_t size, uint8_t* out)
{
    for (size_t i = 0U; i < size; i++)
    {
        out[i] = (uint8_t)(value >> (8U * i));
    }
}

/**
 * @funcname `get_le`
 *
 * @description
 * Loads bytes at an offset as a little-endian value.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `in` | `const uint8_t *` | [in] The bytes. |
 * | `offset` | `size_t` | [in] The offset. |
 * | `size` | `size_t` | [in] The number of bytes. |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the byte |
 * | `value` | `uint64_t` | The value |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_GET_LE_01}: `get_le()` shall give the value of bytes
 *   in little-endian order.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_GET_LE_01}: `get_le()` shall return the value whose
 *   bits `8 * i + 7` to `8 * i` are `in[offset + i]`, for each `i` less than
 *   `size`, and whose other bits are 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_GET_LE_01}
 *
 * @design
 * 1. Set `value` to `0U`. \llrref{LLR_GDB_ARM64_GET_LE_01}
 * 2. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GDB_ARM64_GET_LE_01}
 *    1. Set the bits `(uint64_t)in[offset + i] << (8U * i)` in `value`.
 * 3. Return `value`. \llrref{LLR_GDB_ARM64_GET_LE_01}
 *
 * @calls
 * None.
 */
static uint64_t get_le(const uint8_t* in, size_t offset, size_t size)
{
    uint64_t value = 0U;

    for (size_t i = 0U; i < size; i++)
    {
        value |= (uint64_t)in[offset + i] << (8U * i);
    }

    return value;
}

/**
 * @funcname `ja_gdb_register_size`
 *
 * @description
 * Returns the size of a GDB register: 8 for x0-x30, sp and pc, 4 for cpsr, fpsr
 * and fpcr, 16 for v0-v31.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 *
 * @return The size, or 0 for no such register.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | The size |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_01}: `ja_gdb_register_size()`
 *   shall give the size of each register that the debugger knows: 8 bytes for
 *   x0 to x30, SP and PC, 4 for CPSR, FPSR and FPCR, 16 for v0 to v31, and 0
 *   for no such register.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_01}: If `regno` is not greater
 *   than `REG_PC`, `ja_gdb_register_size()` shall return 8.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_02}: If `regno` is `REG_CPSR`,
 *   `ja_gdb_register_size()` shall return 4.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_03}: If `regno` is greater than
 *   `REG_PC`, is not `REG_CPSR` and is not greater than `REG_V31`,
 *   `ja_gdb_register_size()` shall return 16.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_04}: If `regno` is greater than
 *   `REG_V31` and is not greater than `REG_FPCR`, `ja_gdb_register_size()`
 *   shall return 4.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_05}: If `regno` is greater than
 *   `REG_FPCR`, `ja_gdb_register_size()` shall return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_01}
 *
 * @design
 * 1. If `regno` is less than or equal to `REG_PC`:
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_01}
 *    1. Set `size` to `8U`.
 * 2. Otherwise, if `regno` is equal to `REG_CPSR`:
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_02}
 *    1. Set `size` to `4U`.
 * 3. Otherwise, if `regno` is less than or equal to `REG_V31`:
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_03}
 *    1. Set `size` to `16U`.
 * 4. Otherwise, if `regno` is less than or equal to `REG_FPCR`:
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_04}
 *    1. Set `size` to `4U`.
 * 5. Otherwise: \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_05}
 *    1. Set `size` to `0U`.
 * 6. Return `size`. \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_04},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SIZE_05}
 *
 * @calls
 * None.
 */
size_t ja_gdb_register_size(uint32_t regno)
{
    size_t size;

    if (regno <= REG_PC) {
        size = 8U;
    } else if (regno == REG_CPSR) {
        size = 4U;
    } else if (regno <= REG_V31) {
        size = 16U;
    } else if (regno <= REG_FPCR) {
        size = 4U;
    } else {
        size = 0U;
    }

    return size;
}

/**
 * @funcname `ja_gdb_context_is_user`
 *
 * @description
 * Tells whether a context runs at EL0 (SPSR_EL1.M is 0).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `const interrupt_frame *` | [in] The context. |
 *
 * @return TRUE for user space.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_CONTEXT_IS_USER_01}: `ja_gdb_context_is_user()`
 *   shall tell whether a context runs at exception level 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_002}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_CONTEXT_IS_USER_01}: If the bits `SPSR_M_MASK`
 *   of `spsr` of `ctx` are 0, `ja_gdb_context_is_user()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_CONTEXT_IS_USER_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_CONTEXT_IS_USER_02}: If the bits `SPSR_M_MASK`
 *   of `spsr` of `ctx` are not 0, `ja_gdb_context_is_user()` shall return
 *   false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_CONTEXT_IS_USER_01}
 *
 * @design
 * 1. Return `(ctx->spsr & SPSR_M_MASK) == 0U`.
 *    \llrref{LLR_GDB_ARM64_JA_GDB_CONTEXT_IS_USER_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_CONTEXT_IS_USER_02}
 *
 * @calls
 * None.
 */
pok_bool_t ja_gdb_context_is_user(const interrupt_frame* ctx)
{
    return (ctx->spsr & SPSR_M_MASK) == 0U;
}

/**
 * @funcname `frame_register_get`
 *
 * @description
 * Returns an integer register (up to cpsr) of an exception frame: sp is SP_EL0
 * for user code, the address after the frame for kernel code; the step bit of
 * cpsr is left out (the processor's, not the program's).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `const interrupt_frame *` | [in] The frame. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | The value |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_FRAME_REGISTER_GET_01}: `frame_register_get()` shall
 *   give an integer register, up to CPSR, from an exception frame: SP is SP_EL0
 *   for user code and the address after the frame for kernel code; the step bit
 *   of CPSR is not shown.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_GET_01}: If `regno` is less than 31,
 *   `frame_register_get()` shall return `x[regno]` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_GET_01}
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_GET_02}: If `regno` is `REG_SP` and
 *   `ja_gdb_context_is_user()` returns true for `frame`, `frame_register_get()`
 *   shall return `sp_el0` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_GET_01}
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_GET_03}: If `regno` is `REG_SP` and
 *   `ja_gdb_context_is_user()` returns false for `frame`,
 *   `frame_register_get()` shall return the address that follows the frame
 *   (`frame` plus the size of the frame).
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_GET_01}
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_GET_04}: If `regno` is `REG_PC`,
 *   `frame_register_get()` shall return `elr` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_GET_01}
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_GET_05}: If `regno` is not less than 31
 *   and is neither `REG_SP` nor `REG_PC`, `frame_register_get()` shall return
 *   `spsr` of `frame` with `SPSR_SS` cleared.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_GET_01}
 *
 * @design
 * 1. If `regno` is less than `31U`:
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_01}
 *    1. Set `value` to `frame->x[regno]`.
 * 2. Otherwise, if `regno` is equal to `REG_SP`:
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_02},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_03}
 *    1. Set `value` to `ja_gdb_context_is_user(frame) ? frame->sp_el0 :`
 *       `((uint64_t)(uintptr_t)frame + sizeof(*frame))`.
 * 3. Otherwise, if `regno` is equal to `REG_PC`:
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_04}
 *    1. Set `value` to `frame->elr`.
 * 4. Otherwise: \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_05}
 *    1. Set `value` to `frame->spsr & ~SPSR_SS`.
 * 5. Return `value`. \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_01},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_02},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_03},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_04},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_GET_05}
 *
 * @calls
 * - `ja_gdb_context_is_user()`
 */
/*
 * Value of integer register 'regno' (up to cpsr) of the frame.
 *
 * MISRA C:2025 Rule 11.4 deviation: the stack pointer of kernel code is the
 * address after the frame, which the exception entry has put on its stack.
 */
static uint64_t frame_register_get(const interrupt_frame* frame,
    uint32_t regno)
{
    uint64_t value;

    if (regno < 31U) {
        value = frame->x[regno];
    } else if (regno == REG_SP) {
        value = ja_gdb_context_is_user(frame) ? frame->sp_el0
            : ((uint64_t)(uintptr_t)frame + sizeof(*frame));
    } else if (regno == REG_PC) {
        value = frame->elr;
    } else {
        // The step bit is the processor's, not the program's.
        value = frame->spsr & ~SPSR_SS;
    }

    return value;
}

/**
 * @funcname `frame_register_set`
 *
 * @description
 * Sets an integer register (up to cpsr) of an exception frame. Only the
 * condition flags of cpsr are changed, so GDB can't change the exception level
 * or unmask interrupts; the sp of kernel code and an unaligned pc can't be set.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The frame. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `uint64_t` | [in] The value. |
 *
 * @return FALSE if it can't be set.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `written` | `pok_bool_t` | Whether it was set |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_FRAME_REGISTER_SET_01}: `frame_register_set()` shall
 *   set an integer register, up to CPSR, in an exception frame, changing only
 *   the condition flags of CPSR, and tell whether it could.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_SET_01}: If `regno` is less than 31,
 *   `frame_register_set()` shall set `x[regno]` of `frame` to `value` and
 *   return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_SET_02}: If `regno` is `REG_SP` and
 *   `ja_gdb_context_is_user()` returns true for `frame`, `frame_register_set()`
 *   shall set `sp_el0` of `frame` to `value` and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_SET_03}: If `regno` is `REG_SP` and
 *   `ja_gdb_context_is_user()` returns false for `frame`,
 *   `frame_register_set()` shall return `FALSE` and leave `frame` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_SET_04}: If `regno` is `REG_PC` and
 *   `value` is a multiple of `JA_GDB_BREAKPOINT_SIZE`, `frame_register_set()`
 *   shall set `elr` of `frame` to `value` and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_SET_05}: If `regno` is `REG_PC` and
 *   `value` is not a multiple of `JA_GDB_BREAKPOINT_SIZE`,
 *   `frame_register_set()` shall return `FALSE` and leave `frame` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_FRAME_REGISTER_SET_06}: If `regno` is not less than 31
 *   and is neither `REG_SP` nor `REG_PC`, `frame_register_set()` shall set the
 *   bits `SPSR_NZCV` of `spsr` of `frame` to those of `value`, leave its other
 *   bits as they are, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FRAME_REGISTER_SET_01}
 *
 * @design
 * 1. Set `written` to `TRUE`. \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_01},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_02},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_04},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_06}
 * 2. If `regno` is less than `31U`:
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_01}
 *    1. Set `frame->x[regno]` to `value`.
 * 3. Otherwise, if `regno` is equal to `REG_SP`:
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_02},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_03}
 *    1. If `ja_gdb_context_is_user(frame)` is true:
 *       \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_02}
 *       1. Set `frame->sp_el0` to `value`.
 *    2. Otherwise: \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_03}
 *       1. Set `written` to `FALSE`.
 * 4. Otherwise, if `regno` is equal to `REG_PC`:
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_04},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_05}
 *    1. If `(value % JA_GDB_BREAKPOINT_SIZE)` is equal to `0U`:
 *       \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_04}
 *       1. Set `frame->elr` to `value`.
 *    2. Otherwise: \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_05}
 *       1. Set `written` to `FALSE`.
 * 5. Otherwise: \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_06}
 *    1. Set `frame->spsr` to
 *       `(frame->spsr & ~SPSR_NZCV) | (value & SPSR_NZCV)`.
 * 6. Return `written`. \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_01},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_02},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_03},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_04},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_05},
 *    \llrref{LLR_GDB_ARM64_FRAME_REGISTER_SET_06}
 *
 * @calls
 * - `ja_gdb_context_is_user()`
 */
static pok_bool_t frame_register_set(interrupt_frame* frame, uint32_t regno,
    uint64_t value)
{
    pok_bool_t written = TRUE;

    if (regno < 31U)
    {
        frame->x[regno] = value;
    }
    else if (regno == REG_SP)
    {
        // The stack pointer of kernel code isn't in the frame.
        if (ja_gdb_context_is_user(frame)) {
            frame->sp_el0 = value;
        } else {
            written = FALSE;
        }
    }
    else if (regno == REG_PC)
    {
        // An unaligned PC would fault at once.
        if ((value % JA_GDB_BREAKPOINT_SIZE) == 0U) {
            frame->elr = value;
        } else {
            written = FALSE;
        }
    }
    else
    {
        frame->spsr = (frame->spsr & ~SPSR_NZCV) | (value & SPSR_NZCV);
    }

    return written;
}

/**
 * @funcname `fp_register_get`
 *
 * @description
 * Reads an FP/SIMD register (v0 and above) from an FP store.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp` | `const jet_fp_store_t *` | [in] The FP store. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `uint8_t *` | [out] Where the value is stored. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_FP_REGISTER_GET_01}: `fp_register_get()` shall give a
 *   floating point register, from v0 on, from a store of the floating point
 *   state.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_FP_REGISTER_GET_01}: If `regno` is not greater than
 *   `REG_V31`, `fp_register_get()` shall store with `put_le()` `q[regno -
 *   REG_V0][0]` of `fp` as 8 bytes at `value` and `q[regno - REG_V0][1]` of
 *   `fp` as 8 bytes at `value + 8`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FP_REGISTER_GET_01}
 * - \llrid{LLR_GDB_ARM64_FP_REGISTER_GET_02}: If `regno` is `REG_FPSR`,
 *   `fp_register_get()` shall store with `put_le()` `fpsr` of `fp` as 4 bytes
 *   at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FP_REGISTER_GET_01}
 * - \llrid{LLR_GDB_ARM64_FP_REGISTER_GET_03}: If `regno` is greater than
 *   `REG_V31` and is not `REG_FPSR`, `fp_register_get()` shall store with
 *   `put_le()` `fpcr` of `fp` as 4 bytes at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FP_REGISTER_GET_01}
 *
 * @design
 * 1. If `regno` is less than or equal to `REG_V31`:
 *    \llrref{LLR_GDB_ARM64_FP_REGISTER_GET_01}
 *    1. Call `put_le()` with the arguments `fp->q[regno - REG_V0][0]`, `8U` and
 *       `value`.
 *    2. Call `put_le()` with the arguments `fp->q[regno - REG_V0][1]`, `8U` and
 *       the address of `value[8]`.
 * 2. Otherwise, if `regno` is equal to `REG_FPSR`:
 *    \llrref{LLR_GDB_ARM64_FP_REGISTER_GET_02}
 *    1. Call `put_le()` with the arguments `fp->fpsr`, `4U` and `value`.
 * 3. Otherwise: \llrref{LLR_GDB_ARM64_FP_REGISTER_GET_03}
 *    1. Call `put_le()` with the arguments `fp->fpcr`, `4U` and `value`.
 *
 * @calls
 * - `put_le()`
 */
static void fp_register_get(const jet_fp_store_t* fp, uint32_t regno,
    uint8_t* value)
{
    if (regno <= REG_V31)
    {
        put_le(fp->q[regno - REG_V0][0], 8U, value);
        put_le(fp->q[regno - REG_V0][1], 8U, &value[8]);
    }
    else if (regno == REG_FPSR)
    {
        put_le(fp->fpsr, 4U, value);
    }
    else
    {
        put_le(fp->fpcr, 4U, value);
    }
}

/**
 * @funcname `fp_register_set`
 *
 * @description
 * Writes an FP/SIMD register (v0 and above) into an FP store; undefined bits of
 * fpsr and fpcr aren't written.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fp` | `jet_fp_store_t *` | [out] The FP store. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `const uint8_t *` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_FP_REGISTER_SET_01}: `fp_register_set()` shall write a
 *   floating point register, from v0 on, into a store of the floating point
 *   state, without the undefined bits of FPSR and FPCR.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_FP_REGISTER_SET_01}: If `regno` is not greater than
 *   `REG_V31`, `fp_register_set()` shall set `q[regno - REG_V0][0]` of `fp` to
 *   the value of the 8 bytes at `value` and `q[regno - REG_V0][1]` of `fp` to
 *   the value of the 8 bytes at `value + 8` (`get_le()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FP_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_FP_REGISTER_SET_02}: If `regno` is `REG_FPSR`,
 *   `fp_register_set()` shall set `fpsr` of `fp` to the value of the 4 bytes at
 *   `value` (`get_le()`), ANDed with `FPSR_MASK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FP_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_FP_REGISTER_SET_03}: If `regno` is greater than
 *   `REG_V31` and is not `REG_FPSR`, `fp_register_set()` shall set `fpcr` of
 *   `fp` to the value of the 4 bytes at `value` (`get_le()`), ANDed with
 *   `FPCR_MASK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_FP_REGISTER_SET_01}
 *
 * @design
 * 1. If `regno` is less than or equal to `REG_V31`:
 *    \llrref{LLR_GDB_ARM64_FP_REGISTER_SET_01}
 *    1. Call `get_le()` with the arguments `value`, `0U` and `8U`, and store
 *       the result in `fp->q[regno - REG_V0][0]`.
 *    2. Call `get_le()` with the arguments `value`, `8U` and `8U`, and store
 *       the result in `fp->q[regno - REG_V0][1]`.
 * 2. Otherwise, if `regno` is equal to `REG_FPSR`:
 *    \llrref{LLR_GDB_ARM64_FP_REGISTER_SET_02}
 *    1. Set `fp->fpsr` to `get_le(value, 0U, 4U) & FPSR_MASK`.
 * 3. Otherwise: \llrref{LLR_GDB_ARM64_FP_REGISTER_SET_03}
 *    1. Set `fp->fpcr` to `get_le(value, 0U, 4U) & FPCR_MASK`.
 *
 * @calls
 * - `get_le()`
 */
static void fp_register_set(jet_fp_store_t* fp, uint32_t regno,
    const uint8_t* value)
{
    if (regno <= REG_V31)
    {
        fp->q[regno - REG_V0][0] = get_le(value, 0U, 8U);
        fp->q[regno - REG_V0][1] = get_le(value, 8U, 8U);
    }
    else if (regno == REG_FPSR)
    {
        fp->fpsr = get_le(value, 0U, 4U) & FPSR_MASK;
    }
    else
    {
        fp->fpcr = get_le(value, 0U, 4U) & FPCR_MASK;
    }
}

/**
 * @funcname `ja_gdb_register_get`
 *
 * @description
 * Reads a GDB register of a thread: from its exception frame (up to cpsr) or
 * its FP store.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `const interrupt_frame *` | [in] The thread's registers, or NULL. |
 * | `fp` | `const jet_fp_store_t *` | [in] The thread's FP store, or NULL. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `uint8_t *` | [out] Where the value is stored. |
 *
 * @return TRUE if the register is available.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `available` | `pok_bool_t` | Whether it is available |
 * | `size` | `size_t` | Its size |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_REGISTER_GET_01}: `ja_gdb_register_get()` shall
 *   give a register of a thread to the debugger: from its exception frame up to
 *   CPSR, or from its floating point store, and tell whether it is available.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_01}: If `regno` is not greater
 *   than `REG_CPSR` and `ctx` is not NULL, `ja_gdb_register_get()` shall store
 *   at `value` with `put_le()` the result of `frame_register_get()` for `ctx`
 *   and `regno`, in the number of bytes that `ja_gdb_register_size()` gives for
 *   `regno`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_GET_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_02}: If `regno` is not greater
 *   than `REG_CPSR` and `ctx` is NULL, `ja_gdb_register_get()` shall return
 *   `FALSE` without writing at `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_GET_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_03}: If `regno` is greater than
 *   `REG_CPSR`, `fp` is not NULL and the size that `ja_gdb_register_size()`
 *   gives for `regno` is not 0, `ja_gdb_register_get()` shall call
 *   `fp_register_get()` once with `fp`, `regno` and `value`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_GET_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_04}: If `regno` is greater than
 *   `REG_CPSR`, and `fp` is NULL or the size that `ja_gdb_register_size()`
 *   gives for `regno` is 0, `ja_gdb_register_get()` shall return `FALSE`
 *   without calling `fp_register_get()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_GET_01}
 *
 * @design
 * 1. Set `available` to `FALSE`. \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_04}
 * 2. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *    result in `size`. \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_04}
 * 3. If `regno` is less than or equal to `REG_CPSR`:
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_02}
 *    1. If `ctx` is not NULL:
 *       1. Call `put_le()` with the arguments `frame_register_get(ctx, regno)`,
 *          `size` and `value`. \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_01}
 *       2. Set `available` to `TRUE`.
 *          \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_01}
 * 4. Otherwise, if `fp` is not NULL and `size` is not equal to `0U`:
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_04}
 *    1. Call `fp_register_get()` with the arguments `fp`, `regno` and `value`.
 *       \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_03}
 *    2. Set `available` to `TRUE`.
 *       \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_03}
 * 5. Return `available`. \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_GET_04}
 *
 * @calls
 * - `ja_gdb_register_size()`
 * - `put_le()`
 * - `frame_register_get()`
 * - `fp_register_get()`
 */
pok_bool_t ja_gdb_register_get(const interrupt_frame* ctx,
    const jet_fp_store_t* fp, uint32_t regno, uint8_t* value)
{
    pok_bool_t available = FALSE;
    size_t size = ja_gdb_register_size(regno);

    if (regno <= REG_CPSR)
    {
        if (ctx != NULL)
        {
            put_le(frame_register_get(ctx, regno), size, value);
            available = TRUE;
        }
    }
    else if ((fp != NULL) && (size != 0U))
    {
        fp_register_get(fp, regno, value);
        available = TRUE;
    }
    else
    {
        // No FP/SIMD registers, or no such register.
    }

    return available;
}

/**
 * @funcname `ja_gdb_register_set`
 *
 * @description
 * Writes a GDB register of a thread: into its exception frame (up to cpsr) or
 * its FP store.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | [in,out] The thread's registers, or NULL. |
 * | `fp` | `jet_fp_store_t *` | [out] The thread's FP store, or NULL. |
 * | `regno` | `uint32_t` | [in] GDB's number of the register. |
 * | `value` | `const uint8_t *` | [in] The value. |
 *
 * @return TRUE if the register was written.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `size` | `size_t` | Its size |
 * | `written` | `pok_bool_t` | Whether it was written |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_REGISTER_SET_01}: `ja_gdb_register_set()` shall
 *   write a register of a thread from the debugger: into its exception frame up
 *   to CPSR, or into its floating point store, and tell whether it did.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_01}: If `regno` is not greater
 *   than `REG_CPSR` and `ctx` is not NULL, `ja_gdb_register_set()` shall return
 *   the result of `frame_register_set()` for `ctx`, `regno` and the value of
 *   the bytes at `value` (`get_le()`, in the number of bytes that
 *   `ja_gdb_register_size()` gives for `regno`).
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_02}: If `regno` is not greater
 *   than `REG_CPSR` and `ctx` is NULL, `ja_gdb_register_set()` shall return
 *   `FALSE` without calling `frame_register_set()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_03}: If `regno` is greater than
 *   `REG_CPSR`, `fp` is not NULL and the size that `ja_gdb_register_size()`
 *   gives for `regno` is not 0, `ja_gdb_register_set()` shall call
 *   `fp_register_set()` once with `fp`, `regno` and `value`, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_SET_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_04}: If `regno` is greater than
 *   `REG_CPSR`, and `fp` is NULL or the size that `ja_gdb_register_size()`
 *   gives for `regno` is 0, `ja_gdb_register_set()` shall return `FALSE`
 *   without calling `fp_register_set()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_REGISTER_SET_01}
 *
 * @design
 * 1. Set `written` to `FALSE`. \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_04}
 * 2. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *    result in `size`. \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_04}
 * 3. If `regno` is less than or equal to `REG_CPSR`:
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_02}
 *    1. If `ctx` is not NULL:
 *       1. Call `frame_register_set()` with the arguments `ctx`, `regno` and
 *          `get_le(value, 0U, size)`, and store the result in `written`.
 *          \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_01}
 * 4. Otherwise, if `fp` is not NULL and `size` is not equal to `0U`:
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_04}
 *    1. Call `fp_register_set()` with the arguments `fp`, `regno` and `value`.
 *       \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_03}
 *    2. Set `written` to `TRUE`. \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_03}
 * 5. Return `written`. \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_REGISTER_SET_04}
 *
 * @calls
 * - `ja_gdb_register_size()`
 * - `frame_register_set()`
 * - `get_le()`
 * - `fp_register_set()`
 */
pok_bool_t ja_gdb_register_set(interrupt_frame* ctx,
    jet_fp_store_t* fp, uint32_t regno, const uint8_t* value)
{
    pok_bool_t written = FALSE;
    size_t size = ja_gdb_register_size(regno);

    if (regno <= REG_CPSR)
    {
        if (ctx != NULL) {
            written = frame_register_set(ctx, regno, get_le(value, 0U, size));
        }
    }
    else if ((fp != NULL) && (size != 0U))
    {
        fp_register_set(fp, regno, value);
        written = TRUE;
    }
    else
    {
        // No FP/SIMD registers, or no such register.
    }

    return written;
}

/**
 * @funcname `ja_gdb_pc_get`
 *
 * @description
 * Returns the PC (ELR_EL1) of a context.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `const interrupt_frame *` | [in] The context. |
 *
 * @return The PC.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_PC_GET_01}: `ja_gdb_pc_get()` shall give the
 *   program counter of a context.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_PC_GET_01}: `ja_gdb_pc_get()` shall return
 *   `elr` of `ctx`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_PC_GET_01}
 *
 * @design
 * 1. Return `ctx->elr`. \llrref{LLR_GDB_ARM64_JA_GDB_PC_GET_01}
 *
 * @calls
 * None.
 */
uintptr_t ja_gdb_pc_get(const interrupt_frame* ctx)
{
    return ctx->elr;
}

/**
 * @funcname `ja_gdb_pc_set`
 *
 * @description
 * Sets the PC (ELR_EL1) of a context.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | [out] The context. |
 * | `pc` | `uintptr_t` | [in] The PC. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_PC_SET_01}: `ja_gdb_pc_set()` shall set the
 *   program counter of a context.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_PC_SET_01}: `ja_gdb_pc_set()` shall set `elr`
 *   of `ctx` to `pc`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_PC_SET_01}
 *
 * @design
 * 1. Set `ctx->elr` to `pc`. \llrref{LLR_GDB_ARM64_JA_GDB_PC_SET_01}
 *
 * @calls
 * None.
 */
void ja_gdb_pc_set(interrupt_frame* ctx, uintptr_t pc)
{
    ctx->elr = pc;
}

/**
 * @funcname `ja_gdb_sync_icache`
 *
 * @description
 * Makes code written by GDB visible to instruction fetch: cleans the data cache
 * lines of the code, then invalidates the instruction cache.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `void *` | [in] The code. |
 * | `size` | `size_t` | [in] Its size. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctr` | `uint64_t` | CTR_EL0 |
 * | `end` | `uintptr_t` | The end of the code |
 * | `line` | `uintptr_t` | The data cache line size |
 * | `p` | `uintptr_t` | The address of a line |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_SYNC_ICACHE_01}: `ja_gdb_sync_icache()` shall
 *   make code that the debugger wrote visible to the instruction fetch.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_SYNC_ICACHE_01}: `ja_gdb_sync_icache()` shall
 *   call `cpu_dcache_clean()` with the address of each cache line from the line
 *   of `addr` up to, but not including, `addr` plus `size`, where the size of a
 *   line is 4 shifted left by the bits 19 to 16 of CTR_EL0, and then call
 *   `cpu_icache_invalidate()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_SYNC_ICACHE_01}
 *
 * @design
 * 1. Set `ctr` to `read_sysreg(ctr_el0)`.
 *    \llrref{LLR_GDB_ARM64_JA_GDB_SYNC_ICACHE_01}
 * 2. Set `line` to `(uintptr_t)4U << ((ctr >> 16) & 0xFU)`.
 *    \llrref{LLR_GDB_ARM64_JA_GDB_SYNC_ICACHE_01}
 * 3. Set `end` to `(uintptr_t)addr + size`.
 *    \llrref{LLR_GDB_ARM64_JA_GDB_SYNC_ICACHE_01}
 * 4. Set `p` to `(uintptr_t)addr & ~(line - 1U)`; then while `p` is less than
 *    `end`, repeat (after each pass: add `line` to `p`):
 *    \llrref{LLR_GDB_ARM64_JA_GDB_SYNC_ICACHE_01}
 *    1. Call `cpu_dcache_clean()` with the argument `p`.
 * 5. Call `cpu_icache_invalidate()`.
 *    \llrref{LLR_GDB_ARM64_JA_GDB_SYNC_ICACHE_01}
 *
 * @calls
 * - `cpu_dcache_clean()`
 * - `cpu_icache_invalidate()`
 */
/*
 * MISRA C:2025 Rule 11.4 and Rule 11.6 deviation: the cache lines of the
 * code at the address are found by integer arithmetic on the address.
 */
void ja_gdb_sync_icache(void* addr, size_t size)
{
    // CTR_EL0.DminLine: log2 of the data cache line size in words.
    uint64_t ctr = read_sysreg(ctr_el0);
    uintptr_t line = (uintptr_t)4U << ((ctr >> 16) & 0xFU);
    uintptr_t end = (uintptr_t)addr + size;

    for (uintptr_t p = (uintptr_t)addr & ~(line - 1U); p < end; p += line)
    {
        cpu_dcache_clean(p);
    }
    cpu_icache_invalidate();
}

static gdb_step_state_e step_state = STEP_NONE;
static const interrupt_frame* step_frame = NULL;
/* Bits of SPSR_EL1 changed for the stepped instruction, and their values. */
static uint64_t step_changed = 0U;
static uint64_t step_saved = 0U;

/**
 * @funcname `ja_gdb_single_step`
 *
 * @description
 * Asks for or cancels a single step of a context (see the description at the
 * beginning of the file): the step starts when the exception handler returns to
 * it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `interrupt_frame *` | [in,out] The context. |
 * | `enable` | `pok_bool_t` | [in] TRUE to step, FALSE to cancel. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `step_frame` | `const interrupt_frame *` | The context that steps |
 * | `step_state` | `gdb_step_state_e` | The state of the single step |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_SINGLE_STEP_01}: `ja_gdb_single_step()` shall
 *   ask for, or cancel, a single step of a context, which starts when the
 *   exception handler returns to it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_SINGLE_STEP_01}: If `enable` is true,
 *   `ja_gdb_single_step()` shall set `SPSR_SS` in `spsr` of `ctx`, and set
 *   `step_state` to `STEP_REQUESTED` and `step_frame` to `ctx`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_SINGLE_STEP_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_SINGLE_STEP_02}: If `enable` is false,
 *   `ja_gdb_single_step()` shall clear `SPSR_SS` in `spsr` of `ctx`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_SINGLE_STEP_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_SINGLE_STEP_03}: If `enable` is false,
 *   `ja_gdb_single_step()` shall set `step_state` to `STEP_NONE` if and only if
 *   `step_frame` is `ctx`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_SINGLE_STEP_01}
 *
 * @design
 * 1. If `enable` is true: \llrref{LLR_GDB_ARM64_JA_GDB_SINGLE_STEP_01}
 *    1. Set the bits `SPSR_SS` in `ctx->spsr`.
 *    2. Set `step_state` to `STEP_REQUESTED`.
 *    3. Set `step_frame` to `ctx`.
 * 2. Otherwise: \llrref{LLR_GDB_ARM64_JA_GDB_SINGLE_STEP_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_SINGLE_STEP_03}
 *    1. Keep only the bits `~SPSR_SS` of `ctx->spsr`.
 *       \llrref{LLR_GDB_ARM64_JA_GDB_SINGLE_STEP_02}
 *    2. If `step_frame` is equal to `ctx`:
 *       \llrref{LLR_GDB_ARM64_JA_GDB_SINGLE_STEP_03}
 *       1. Set `step_state` to `STEP_NONE`.
 *
 * @calls
 * None.
 */
void ja_gdb_single_step(interrupt_frame* ctx,
    pok_bool_t enable)
{
    if (enable)
    {
        ctx->spsr |= SPSR_SS;
        step_state = STEP_REQUESTED;
        step_frame = ctx;
    }
    else
    {
        ctx->spsr &= ~SPSR_SS;
        if (step_frame == ctx) {
            step_state = STEP_NONE;
        }
    }
}

/**
 * @funcname `insn_changes_mask`
 *
 * @description
 * Tells whether the instruction at the PC of a kernel context changes the
 * interrupt mask or waits for an interrupt (wfi, wfe, msr daif*, eret). If it
 * can't be read, assume it does.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `const interrupt_frame *` | [in] The context. |
 *
 * @return TRUE if it does.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `changes` | `pok_bool_t` | Whether it does |
 * | `code` | `const uint8_t *` | The code, at its kernel address |
 * | `insn` | `uint32_t` | The instruction |
 *
 * @hlr
 * - \hlrid{HLR_GDB_INSN_CHANGES_MASK_01}: `insn_changes_mask()` shall tell
 *   whether the instruction at the program counter of a kernel context changes
 *   the interrupt mask or waits for an interrupt, assuming it does if it cannot
 *   be read.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_INSN_CHANGES_MASK_01}: If `ja_addr_to_gdb_ro()` returns NULL
 *   for `elr` of `ctx`, 4 and the space 0, `insn_changes_mask()` shall return
 *   `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_INSN_CHANGES_MASK_01}
 * - \llrid{LLR_GDB_INSN_CHANGES_MASK_02}: If `ja_addr_to_gdb_ro()` returns an
 *   address and the instruction there (the value of its 4 bytes, `get_le()`) is
 *   `INSN_WFI`, `INSN_WFE` or `INSN_ERET`, or its bits `INSN_MSR_DAIFIMM_MASK`
 *   are `INSN_MSR_DAIFIMM`, or its bits `INSN_MSR_DAIF_MASK` are
 *   `INSN_MSR_DAIF`, `insn_changes_mask()` shall return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_INSN_CHANGES_MASK_01}
 * - \llrid{LLR_GDB_INSN_CHANGES_MASK_03}: If `ja_addr_to_gdb_ro()` returns an
 *   address and the instruction there is none of `INSN_WFI`, `INSN_WFE` and
 *   `INSN_ERET`, its bits `INSN_MSR_DAIFIMM_MASK` are not `INSN_MSR_DAIFIMM`
 *   and its bits `INSN_MSR_DAIF_MASK` are not `INSN_MSR_DAIF`,
 *   `insn_changes_mask()` shall return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_INSN_CHANGES_MASK_01}
 *
 * @design
 * 1. Call `ja_addr_to_gdb_ro()` with the arguments `ctx->elr`, `4U` and `0U`,
 *    and store the result in `code`. \llrref{LLR_GDB_INSN_CHANGES_MASK_01},
 *    \llrref{LLR_GDB_INSN_CHANGES_MASK_02},
 *    \llrref{LLR_GDB_INSN_CHANGES_MASK_03}
 * 2. Set `changes` to `TRUE`. \llrref{LLR_GDB_INSN_CHANGES_MASK_01}
 * 3. If `code` is not NULL: \llrref{LLR_GDB_INSN_CHANGES_MASK_02},
 *    \llrref{LLR_GDB_INSN_CHANGES_MASK_03}
 *    1. Set `insn` to `(uint32_t)get_le(code, 0U, 4U)`.
 *    2. Set `changes` to
 *       `(insn == INSN_WFI) || (insn == INSN_WFE) || (insn == INSN_ERET) ||`
 *       `((insn & INSN_MSR_DAIFIMM_MASK) == INSN_MSR_DAIFIMM) || ((insn &`
 *       `INSN_MSR_DAIF_MASK) == INSN_MSR_DAIF)`.
 * 4. Return `changes`. \llrref{LLR_GDB_INSN_CHANGES_MASK_01},
 *    \llrref{LLR_GDB_INSN_CHANGES_MASK_02},
 *    \llrref{LLR_GDB_INSN_CHANGES_MASK_03}
 *
 * @calls
 * - `ja_addr_to_gdb_ro()`
 * - `get_le()`
 */
/*
 * Whether the instruction at the PC of kernel context 'ctx' changes the
 * interrupt mask or waits for an interrupt (wfi, wfe, msr daif*, eret). If
 * it can't be read, assume it does.
 *
 * MISRA C:2025 Rule 11.5 deviation: ja_addr_to_gdb_ro() returns the
 * kernel address of the code as a pointer to void.
 */
static pok_bool_t insn_changes_mask(const interrupt_frame* ctx)
{
    const uint8_t* code = ja_addr_to_gdb_ro(ctx->elr, 4U, 0U);
    pok_bool_t changes = TRUE;

    if (code != NULL)
    {
        uint32_t insn = (uint32_t)get_le(code, 0U, 4U);

        changes = (insn == INSN_WFI) || (insn == INSN_WFE)
            || (insn == INSN_ERET)
            || ((insn & INSN_MSR_DAIFIMM_MASK) == INSN_MSR_DAIFIMM)
            || ((insn & INSN_MSR_DAIF_MASK) == INSN_MSR_DAIF);
    }

    return changes;
}

/**
 * @funcname `step_window_open`
 *
 * @description
 * Starts the step window for the return to a context: IRQs are masked for the
 * stepped instruction (unless it changes the mask itself), and in the kernel
 * debug exceptions are enabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The context. |
 * | `first` | `pok_bool_t` | [in] Whether the instruction is to be executed (it isn't if it has been executed, and the step is pending). |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `step_changed` | `uint64_t` | The SPSR_EL1 bits changed for the stepped instruction |
 * | `step_saved` | `uint64_t` | Their values before |
 * | `step_state` | `gdb_step_state_e` | The state of the single step |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `clear` | `uint64_t` | The SPSR_EL1 bits to clear |
 * | `set` | `uint64_t` | The SPSR_EL1 bits to set |
 *
 * @hlr
 * - \hlrid{HLR_GDB_STEP_WINDOW_OPEN_01}: `step_window_open()` shall prepare the
 *   return to a context that steps: interrupts masked for the stepped
 *   instruction unless it changes the mask itself, and debug exceptions
 *   unmasked in the kernel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_STEP_WINDOW_OPEN_01}: If `ja_gdb_context_is_user()` returns
 *   true for `frame`, `step_window_open()` shall set `SPSR_I` in `spsr` of
 *   `frame` and leave `SPSR_D` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_STEP_WINDOW_OPEN_01}
 * - \llrid{LLR_GDB_STEP_WINDOW_OPEN_02}: If `ja_gdb_context_is_user()` returns
 *   false and `insn_changes_mask()` returns false for `frame`,
 *   `step_window_open()` shall set `SPSR_I` and clear `SPSR_D` in `spsr` of
 *   `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_STEP_WINDOW_OPEN_01}
 * - \llrid{LLR_GDB_STEP_WINDOW_OPEN_03}: If `ja_gdb_context_is_user()` returns
 *   false and `insn_changes_mask()` returns true for `frame`,
 *   `step_window_open()` shall clear `SPSR_D` in `spsr` of `frame` and leave
 *   `SPSR_I` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_STEP_WINDOW_OPEN_01}
 * - \llrid{LLR_GDB_STEP_WINDOW_OPEN_04}: `step_window_open()` shall set
 *   `step_changed` to the bits of `spsr` of `frame` whose value this changes,
 *   and `step_saved` to the values that these bits had before.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_STEP_WINDOW_OPEN_01}
 * - \llrid{LLR_GDB_STEP_WINDOW_OPEN_05}: If `first` is true,
 *   `step_window_open()` shall set `SPSR_SS` in `spsr` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_STEP_WINDOW_OPEN_01}
 * - \llrid{LLR_GDB_STEP_WINDOW_OPEN_06}: If `first` is false,
 *   `step_window_open()` shall clear `SPSR_SS` in `spsr` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_STEP_WINDOW_OPEN_01}
 * - \llrid{LLR_GDB_STEP_WINDOW_OPEN_07}: `step_window_open()` shall then call
 *   `cpu_mdscr_set()` once with `MDSCR_SS` and set `step_state` to
 *   `STEP_RUNNING`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_STEP_WINDOW_OPEN_01}
 *
 * @design
 * 1. Set `set` to `0U`. \llrref{LLR_GDB_STEP_WINDOW_OPEN_01},
 *    \llrref{LLR_GDB_STEP_WINDOW_OPEN_02}, \llrref{LLR_GDB_STEP_WINDOW_OPEN_03}
 * 2. Set `clear` to `0U`. \llrref{LLR_GDB_STEP_WINDOW_OPEN_01},
 *    \llrref{LLR_GDB_STEP_WINDOW_OPEN_02}, \llrref{LLR_GDB_STEP_WINDOW_OPEN_03}
 * 3. If `ja_gdb_context_is_user(frame)` is true:
 *    \llrref{LLR_GDB_STEP_WINDOW_OPEN_01}
 *    1. Set `set` to `SPSR_I`.
 * 4. Otherwise: \llrref{LLR_GDB_STEP_WINDOW_OPEN_02},
 *    \llrref{LLR_GDB_STEP_WINDOW_OPEN_03}
 *    1. Set `clear` to `SPSR_D`.
 *    2. If `insn_changes_mask(frame)` is false:
 *       1. Set `set` to `SPSR_I`. \llrref{LLR_GDB_STEP_WINDOW_OPEN_02}
 * 5. Set `step_changed` to `(set & ~frame->spsr) | (clear & frame->spsr)`.
 *    \llrref{LLR_GDB_STEP_WINDOW_OPEN_04}
 * 6. Set `step_saved` to `frame->spsr & step_changed`.
 *    \llrref{LLR_GDB_STEP_WINDOW_OPEN_04}
 * 7. Set `frame->spsr` to `(frame->spsr | set) & ~clear`.
 *    \llrref{LLR_GDB_STEP_WINDOW_OPEN_01},
 *    \llrref{LLR_GDB_STEP_WINDOW_OPEN_02}, \llrref{LLR_GDB_STEP_WINDOW_OPEN_03}
 * 8. If `first` is true: \llrref{LLR_GDB_STEP_WINDOW_OPEN_05}
 *    1. Set the bits `SPSR_SS` in `frame->spsr`.
 * 9. Otherwise: \llrref{LLR_GDB_STEP_WINDOW_OPEN_06}
 *    1. Keep only the bits `~SPSR_SS` of `frame->spsr`.
 * 10. Call `cpu_mdscr_set()` with the argument `MDSCR_SS`.
 *     \llrref{LLR_GDB_STEP_WINDOW_OPEN_07}
 * 11. Set `step_state` to `STEP_RUNNING`. \llrref{LLR_GDB_STEP_WINDOW_OPEN_07}
 *
 * @calls
 * - `ja_gdb_context_is_user()`
 * - `insn_changes_mask()`
 * - `cpu_mdscr_set()`
 */
static void step_window_open(interrupt_frame* frame, pok_bool_t first)
{
    uint64_t set = 0U;
    uint64_t clear = 0U;

    if (ja_gdb_context_is_user(frame))
    {
        set = SPSR_I;
    }
    else
    {
        clear = SPSR_D;
        if (!insn_changes_mask(frame)) {
            set = SPSR_I;
        }
    }

    step_changed = (set & ~frame->spsr) | (clear & frame->spsr);
    step_saved = frame->spsr & step_changed;
    frame->spsr = (frame->spsr | set) & ~clear;

    if (first) {
        frame->spsr |= SPSR_SS;
    } else {
        frame->spsr &= ~SPSR_SS;
    }

    cpu_mdscr_set(MDSCR_SS);
    step_state = STEP_RUNNING;
}

/**
 * @funcname `ja_gdb_exception_enter`
 *
 * @description
 * Called at the start of every exception handler: the first exception after the
 * return to the stepped context ends the step window (the SPSR bits changed for
 * the step are put back).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `step_changed` | `uint64_t` | The SPSR_EL1 bits changed for the stepped instruction |
 * | `step_frame` | `const interrupt_frame *` | The context that steps |
 * | `step_saved` | `uint64_t` | Their values before |
 * | `step_state` | `gdb_step_state_e` | The state of the single step |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_JA_GDB_EXCEPTION_ENTER_01}: `ja_gdb_exception_enter()` shall
 *   end the window of a step at the first exception after the return to the
 *   stepped context, restoring the bits changed for the step.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_JA_GDB_EXCEPTION_ENTER_01}: If and only if `step_state` is
 *   `STEP_RUNNING`, `ja_gdb_exception_enter()` shall call `cpu_mdscr_clear()`
 *   once with `MDSCR_SS`, set the bits `step_changed` of `spsr` of `frame` back
 *   to `step_saved`, and set `step_state` to `STEP_RETURNING` and `step_frame`
 *   to `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_EXCEPTION_ENTER_01}
 *
 * @design
 * 1. If `step_state` is equal to `STEP_RUNNING`:
 *    \llrref{LLR_GDB_JA_GDB_EXCEPTION_ENTER_01}
 *    1. Call `cpu_mdscr_clear()` with the argument `MDSCR_SS`.
 *    2. Set `frame->spsr` to `(frame->spsr & ~step_changed) | step_saved`.
 *    3. Set `step_state` to `STEP_RETURNING`.
 *    4. Set `step_frame` to `frame`.
 *
 * @calls
 * - `cpu_mdscr_clear()`
 */
void ja_gdb_exception_enter(interrupt_frame* frame)
{
    if (step_state == STEP_RUNNING)
    {
        // The first exception after the return to the stepped context.
        cpu_mdscr_clear(MDSCR_SS);
        frame->spsr = (frame->spsr & ~step_changed) | step_saved;
        step_state = STEP_RETURNING;
        step_frame = frame;
    }
}

/**
 * @funcname `ja_gdb_user_jump`
 *
 * @description
 * Called when a thread (re)starts in user space: a step that it had begun won't
 * end.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stack_kernel` | `jet_stack_t` | [in] The thread's kernel stack. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `step_frame` | `const interrupt_frame *` | The context that steps |
 * | `step_state` | `gdb_step_state_e` | The state of the single step |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_JA_GDB_USER_JUMP_01}: `ja_gdb_user_jump()` shall cancel a
 *   step that a thread had begun when the thread starts again in user mode.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_JA_GDB_USER_JUMP_01}: If and only if `step_state` is
 *   `STEP_RETURNING` and `step_frame` is the frame at the top of the stack
 *   (`stack_kernel` minus the size of `interrupt_frame`), `ja_gdb_user_jump()`
 *   shall set `step_state` to `STEP_NONE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_USER_JUMP_01}
 *
 * @design
 * 1. If `step_state` is equal to `STEP_RETURNING` and `(uintptr_t)step_frame`
 *    is equal to `(stack_kernel - sizeof(interrupt_frame))`:
 *    \llrref{LLR_GDB_JA_GDB_USER_JUMP_01}
 *    1. Set `step_state` to `STEP_NONE`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation: the frame of a thread's exceptions from
 * user space is at the top of its kernel stack, given as an address.
 */
void ja_gdb_user_jump(jet_stack_t stack_kernel)
{
    // The thread (re)starts: the step it had begun won't end.
    if ((step_state == STEP_RETURNING)
        && ((uintptr_t)step_frame
            == (stack_kernel - sizeof(interrupt_frame))))
    {
        step_state = STEP_NONE;
    }
}

/**
 * @funcname `ja_gdb_exception_unmask`
 *
 * @description
 * Unmasks debug exceptions in an exception handler that isn't GDB's.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_JA_GDB_EXCEPTION_UNMASK_01}: `ja_gdb_exception_unmask()`
 *   shall unmask the debug exceptions in an exception handler that is not the
 *   debugger's.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_JA_GDB_EXCEPTION_UNMASK_01}: `ja_gdb_exception_unmask()`
 *   shall call `cpu_debug_unmask()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_EXCEPTION_UNMASK_01}
 *
 * @design
 * 1. Call `cpu_debug_unmask()`. \llrref{LLR_GDB_JA_GDB_EXCEPTION_UNMASK_01}
 *
 * @calls
 * - `cpu_debug_unmask()`
 */
void ja_gdb_exception_unmask(void)
{
    cpu_debug_unmask();
}

/**
 * @funcname `ja_gdb_exception_leave`
 *
 * @description
 * Called when an exception handler returns: masks debug exceptions, and opens
 * the step window if the return is to the context that steps (otherwise clears
 * a stale step bit).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `step_frame` | `const interrupt_frame *` | The context that steps |
 * | `step_state` | `gdb_step_state_e` | The state of the single step |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_JA_GDB_EXCEPTION_LEAVE_01}: `ja_gdb_exception_leave()` shall
 *   mask the debug exceptions when an exception handler returns, and open the
 *   window of a step if the return is to the stepped context.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_JA_GDB_EXCEPTION_LEAVE_01}: `ja_gdb_exception_leave()` shall
 *   call `cpu_debug_mask()` once, before anything else.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_EXCEPTION_LEAVE_01}
 * - \llrid{LLR_GDB_JA_GDB_EXCEPTION_LEAVE_02}: If `step_state` is
 *   `STEP_REQUESTED` or `STEP_RETURNING`, and `frame` is `step_frame`,
 *   `ja_gdb_exception_leave()` shall call `step_window_open()` once with
 *   `frame` and whether `step_state` is `STEP_REQUESTED`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_EXCEPTION_LEAVE_01}
 * - \llrid{LLR_GDB_JA_GDB_EXCEPTION_LEAVE_03}: If `step_state` is neither
 *   `STEP_REQUESTED` nor `STEP_RETURNING`, or `frame` is not `step_frame`,
 *   `ja_gdb_exception_leave()` shall clear `SPSR_SS` in `spsr` of `frame`,
 *   without calling `step_window_open()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_EXCEPTION_LEAVE_01}
 *
 * @design
 * 1. Call `cpu_debug_mask()`. \llrref{LLR_GDB_JA_GDB_EXCEPTION_LEAVE_01}
 * 2. If (`step_state` is equal to `STEP_REQUESTED` or `step_state` is equal to
 *    `STEP_RETURNING`) and `frame` is equal to `step_frame`:
 *    \llrref{LLR_GDB_JA_GDB_EXCEPTION_LEAVE_02}
 *    1. Call `step_window_open()` with the arguments `frame` and
 *       `step_state == STEP_REQUESTED`.
 * 3. Otherwise: \llrref{LLR_GDB_JA_GDB_EXCEPTION_LEAVE_03}
 *    1. Keep only the bits `~SPSR_SS` of `frame->spsr`.
 *
 * @calls
 * - `cpu_debug_mask()`
 * - `step_window_open()`
 */
void ja_gdb_exception_leave(interrupt_frame* frame)
{
    cpu_debug_mask();

    if (((step_state == STEP_REQUESTED) || (step_state == STEP_RETURNING))
        && (frame == step_frame))
    {
        step_window_open(frame, step_state == STEP_REQUESTED);
    }
    else
    {
        // A stale bit would make the context step when another one does.
        frame->spsr &= ~SPSR_SS;
    }
}

static hw_slot_t hw_breakpoints[JA_GDB_HW_BREAKPOINTS];
static hw_slot_t hw_watchpoints[JA_GDB_HW_WATCHPOINTS];
/* Numbers of the breakpoint and watchpoint registers of the processor. */
static uint32_t hw_breakpoints_n = 0U;
static uint32_t hw_watchpoints_n = 0U;

/**
 * @funcname `ja_gdb_init`
 *
 * @description
 * Initializes debugging at boot: the number of breakpoint and watchpoint
 * registers, no step and no breakpoint or watchpoint (the debug registers are
 * UNKNOWN after reset), kernel debug enabled, the OS lock cleared and debug
 * exceptions unmasked.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_breakpoints_n` | `uint32_t` | The number of breakpoint registers used |
 * | `hw_watchpoints_n` | `uint32_t` | The number of watchpoint registers used |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `breakpoints` | `uint32_t` | The number of breakpoint registers |
 * | `dfr0` | `uint64_t` | ID_AA64DFR0_EL1 |
 * | `i` | `uint32_t` | The number of a register |
 * | `watchpoints` | `uint32_t` | The number of watchpoint registers |
 *
 * @hlr
 * - \hlrid{HLR_GDB_JA_GDB_INIT_01}: `ja_gdb_init()` shall initialize the debug
 *   support at boot: the number of breakpoint and watchpoint registers, none of
 *   them set, debug exceptions enabled in the kernel and unmasked.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *   - \srqref{SRQ_JET_BOOT_002}
 *
 * @llr
 * - \llrid{LLR_GDB_JA_GDB_INIT_01}: `ja_gdb_init()` shall set
 *   `hw_breakpoints_n` to the bits 15 to 12 of ID_AA64DFR0_EL1 plus 1 if that
 *   number is less than `JA_GDB_HW_BREAKPOINTS`, and to `JA_GDB_HW_BREAKPOINTS`
 *   if it is not.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_INIT_01}
 * - \llrid{LLR_GDB_JA_GDB_INIT_02}: `ja_gdb_init()` shall set
 *   `hw_watchpoints_n` to the bits 23 to 20 of ID_AA64DFR0_EL1 plus 1 if that
 *   number is less than `JA_GDB_HW_WATCHPOINTS`, and to `JA_GDB_HW_WATCHPOINTS`
 *   if it is not.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_INIT_01}
 * - \llrid{LLR_GDB_JA_GDB_INIT_03}: `ja_gdb_init()` shall then write
 *   `MDSCR_KDE` to MDSCR_EL1 (`write_sysreg()`), call `hw_breakpoint_write()`
 *   with `i`, 0 and 0 for each `i` less than `hw_breakpoints_n`, and call
 *   `hw_watchpoint_write()` with `i`, 0 and 0 for each `i` less than
 *   `hw_watchpoints_n`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_INIT_01}
 * - \llrid{LLR_GDB_JA_GDB_INIT_04}: `ja_gdb_init()` shall then call, in this
 *   order, `cpu_os_lock_clear()`, `isb()` and `cpu_debug_unmask()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_INIT_01}
 *
 * @design
 * 1. Set `dfr0` to `read_sysreg(id_aa64dfr0_el1)`.
 *    \llrref{LLR_GDB_JA_GDB_INIT_01}, \llrref{LLR_GDB_JA_GDB_INIT_02}
 * 2. Set `breakpoints` to `(uint32_t)((dfr0 >> 12) & 0xFU) + 1U`.
 *    \llrref{LLR_GDB_JA_GDB_INIT_01}
 * 3. Set `watchpoints` to `(uint32_t)((dfr0 >> 20) & 0xFU) + 1U`.
 *    \llrref{LLR_GDB_JA_GDB_INIT_02}
 * 4. Set `hw_breakpoints_n` to `breakpoints` if `breakpoints` is less than
 *    `JA_GDB_HW_BREAKPOINTS`, otherwise `JA_GDB_HW_BREAKPOINTS`.
 *    \llrref{LLR_GDB_JA_GDB_INIT_01}
 * 5. Set `hw_watchpoints_n` to `watchpoints` if `watchpoints` is less than
 *    `JA_GDB_HW_WATCHPOINTS`, otherwise `JA_GDB_HW_WATCHPOINTS`.
 *    \llrref{LLR_GDB_JA_GDB_INIT_02}
 * 6. Use the macro `write_sysreg()` with the arguments `mdscr_el1` and
 *    `MDSCR_KDE`. \llrref{LLR_GDB_JA_GDB_INIT_03}
 * 7. Set `i` to `0U`; then while `i` is less than `hw_breakpoints_n`, repeat
 *    (after each pass: increment `i`): \llrref{LLR_GDB_JA_GDB_INIT_03}
 *    1. Call `hw_breakpoint_write()` with the arguments `i`, `0U` and `0U`.
 * 8. Set `i` to `0U`; then while `i` is less than `hw_watchpoints_n`, repeat
 *    (after each pass: increment `i`): \llrref{LLR_GDB_JA_GDB_INIT_03}
 *    1. Call `hw_watchpoint_write()` with the arguments `i`, `0U` and `0U`.
 * 9. Call `cpu_os_lock_clear()`. \llrref{LLR_GDB_JA_GDB_INIT_04}
 * 10. Call `isb()`. \llrref{LLR_GDB_JA_GDB_INIT_04}
 * 11. Call `cpu_debug_unmask()`. \llrref{LLR_GDB_JA_GDB_INIT_04}
 *
 * @calls
 * - `hw_breakpoint_write()`
 * - `hw_watchpoint_write()`
 * - `cpu_os_lock_clear()`
 * - `isb()`
 * - `cpu_debug_unmask()`
 */
void ja_gdb_init(void)
{
    uint64_t dfr0 = read_sysreg(id_aa64dfr0_el1);
    uint32_t breakpoints = (uint32_t)((dfr0 >> 12) & 0xFU) + 1U;
    uint32_t watchpoints = (uint32_t)((dfr0 >> 20) & 0xFU) + 1U;

    hw_breakpoints_n = (breakpoints < JA_GDB_HW_BREAKPOINTS)
        ? breakpoints : JA_GDB_HW_BREAKPOINTS;
    hw_watchpoints_n = (watchpoints < JA_GDB_HW_WATCHPOINTS)
        ? watchpoints : JA_GDB_HW_WATCHPOINTS;

    /*
     * The debug registers are UNKNOWN after reset: no step, no hardware
     * breakpoint or watchpoint until GDB inserts one.
     */
    write_sysreg(mdscr_el1, MDSCR_KDE);
    for (uint32_t i = 0U; i < hw_breakpoints_n; i++)
    {
        hw_breakpoint_write(i, 0U, 0U);
    }
    for (uint32_t i = 0U; i < hw_watchpoints_n; i++)
    {
        hw_watchpoint_write(i, 0U, 0U);
    }
    cpu_os_lock_clear();
    isb();
    cpu_debug_unmask();
}

/**
 * @funcname `hw_slots_of`
 *
 * @description
 * Returns the slots of a type: the breakpoints or the watchpoints.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 * | `count` | `uint32_t *` | [out] Where their number is stored. |
 *
 * @return The slots.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_breakpoints` | `hw_slot_t[6]` | The hardware breakpoints |
 * | `hw_breakpoints_n` | `uint32_t` | The number of breakpoint registers used |
 * | `hw_watchpoints` | `hw_slot_t[4]` | The hardware watchpoints |
 * | `hw_watchpoints_n` | `uint32_t` | The number of watchpoint registers used |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `slots` | `hw_slot_t *` | The slots |
 *
 * @hlr
 * - \hlrid{HLR_GDB_HW_SLOTS_OF_01}: `hw_slots_of()` shall give the slots of a
 *   type: the breakpoints or the watchpoints, with their number.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_HW_SLOTS_OF_01}: If `type` is `JA_GDB_HW_EXEC`,
 *   `hw_slots_of()` shall return `hw_breakpoints` and store `hw_breakpoints_n`
 *   at `count`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_SLOTS_OF_01}
 * - \llrid{LLR_GDB_HW_SLOTS_OF_02}: If `type` is not `JA_GDB_HW_EXEC`,
 *   `hw_slots_of()` shall return `hw_watchpoints` and store `hw_watchpoints_n`
 *   at `count`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_SLOTS_OF_01}
 *
 * @design
 * 1. If `type` is equal to `JA_GDB_HW_EXEC`: \llrref{LLR_GDB_HW_SLOTS_OF_01}
 *    1. Set `slots` to `hw_breakpoints`.
 *    2. Set `*count` to `hw_breakpoints_n`.
 * 2. Otherwise: \llrref{LLR_GDB_HW_SLOTS_OF_02}
 *    1. Set `slots` to `hw_watchpoints`.
 *    2. Set `*count` to `hw_watchpoints_n`.
 * 3. Return `slots`. \llrref{LLR_GDB_HW_SLOTS_OF_01},
 *    \llrref{LLR_GDB_HW_SLOTS_OF_02}
 *
 * @calls
 * None.
 */
static hw_slot_t* hw_slots_of(ja_gdb_hw_type_e type, uint32_t* count)
{
    hw_slot_t* slots;

    if (type == JA_GDB_HW_EXEC)
    {
        slots = hw_breakpoints;
        *count = hw_breakpoints_n;
    }
    else
    {
        slots = hw_watchpoints;
        *count = hw_watchpoints_n;
    }

    return slots;
}

/**
 * @funcname `hw_area_supported`
 *
 * @description
 * Tells whether the debug registers can watch an area: an aligned 4 bytes for
 * an instruction breakpoint, an aligned 1, 2, 4 or 8 bytes for a watchpoint.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `len` | `size_t` | [in] The length. |
 *
 * @return TRUE if supported.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `supported` | `pok_bool_t` | Whether it is supported |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_HW_AREA_SUPPORTED_01}: `hw_area_supported()` shall
 *   tell whether the debug registers can watch an area: 4 aligned bytes for an
 *   instruction breakpoint, and 1, 2, 4 or 8 aligned bytes for a watchpoint.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_HW_AREA_SUPPORTED_01}: If `type` is `JA_GDB_HW_EXEC`,
 *   `len` is `JA_GDB_BREAKPOINT_SIZE` and `addr` is a multiple of
 *   `JA_GDB_BREAKPOINT_SIZE`, `hw_area_supported()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_HW_AREA_SUPPORTED_01}
 * - \llrid{LLR_GDB_ARM64_HW_AREA_SUPPORTED_02}: If `type` is `JA_GDB_HW_EXEC`,
 *   and `len` is not `JA_GDB_BREAKPOINT_SIZE` or `addr` is not a multiple of
 *   `JA_GDB_BREAKPOINT_SIZE`, `hw_area_supported()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_HW_AREA_SUPPORTED_01}
 * - \llrid{LLR_GDB_ARM64_HW_AREA_SUPPORTED_03}: If `type` is not
 *   `JA_GDB_HW_EXEC`, `len` is 1, 2, 4 or 8, and `addr` is a multiple of `len`,
 *   `hw_area_supported()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_HW_AREA_SUPPORTED_01}
 * - \llrid{LLR_GDB_ARM64_HW_AREA_SUPPORTED_04}: If `type` is not
 *   `JA_GDB_HW_EXEC`, and `len` is none of 1, 2, 4 and 8 or `addr` is not a
 *   multiple of `len`, `hw_area_supported()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_HW_AREA_SUPPORTED_01}
 *
 * @design
 * 1. If `type` is equal to `JA_GDB_HW_EXEC`:
 *    \llrref{LLR_GDB_ARM64_HW_AREA_SUPPORTED_01},
 *    \llrref{LLR_GDB_ARM64_HW_AREA_SUPPORTED_02}
 *    1. Set `supported` to
 *       `(len == JA_GDB_BREAKPOINT_SIZE) && ((addr % JA_GDB_BREAKPOINT_SIZE)`
 *       `== 0U)`.
 * 2. Otherwise: \llrref{LLR_GDB_ARM64_HW_AREA_SUPPORTED_03},
 *    \llrref{LLR_GDB_ARM64_HW_AREA_SUPPORTED_04}
 *    1. Set `supported` to
 *       `((len == 1U) || (len == 2U) || (len == 4U) || (len == 8U)) && ((addr`
 *       `% len) == 0U)`.
 * 3. Return `supported`. \llrref{LLR_GDB_ARM64_HW_AREA_SUPPORTED_01},
 *    \llrref{LLR_GDB_ARM64_HW_AREA_SUPPORTED_02},
 *    \llrref{LLR_GDB_ARM64_HW_AREA_SUPPORTED_03},
 *    \llrref{LLR_GDB_ARM64_HW_AREA_SUPPORTED_04}
 *
 * @calls
 * None.
 */
static pok_bool_t hw_area_supported(ja_gdb_hw_type_e type,
    uintptr_t addr, size_t len)
{
    pok_bool_t supported;

    if (type == JA_GDB_HW_EXEC) {
        // GDB's breakpoint kind on arm64 is 4 (the size of brk).
        supported = (len == JA_GDB_BREAKPOINT_SIZE)
            && ((addr % JA_GDB_BREAKPOINT_SIZE) == 0U);
    } else {
        supported = ((len == 1U) || (len == 2U) || (len == 4U)
            || (len == 8U)) && ((addr % len) == 0U);
    }

    return supported;
}

/**
 * @funcname `hw_slot_is`
 *
 * @description
 * Tells whether a slot holds a hardware breakpoint or watchpoint.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `slot` | `const hw_slot_t *` | [in] The slot. |
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `len` | `size_t` | [in] The length. |
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @return TRUE if it does.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_HW_SLOT_IS_01}: `hw_slot_is()` shall tell whether a
 *   slot holds a given hardware breakpoint or watchpoint.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_HW_SLOT_IS_01}: If `used` of `slot` is true and its
 *   `type`, `addr`, `len` and `space_id` are equal to the arguments of the same
 *   names, `hw_slot_is()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_HW_SLOT_IS_01}
 * - \llrid{LLR_GDB_ARM64_HW_SLOT_IS_02}: If `used` of `slot` is false, or its
 *   `type`, its `addr`, its `len` or its `space_id` differs from the argument
 *   of the same name, `hw_slot_is()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_HW_SLOT_IS_01}
 *
 * @design
 * 1. Return
 *    `slot->used && (slot->type == type) && (slot->addr == addr) && (slot->len`
 *    `== len) && (slot->space_id == space_id)`.
 *    \llrref{LLR_GDB_ARM64_HW_SLOT_IS_01}, \llrref{LLR_GDB_ARM64_HW_SLOT_IS_02}
 *
 * @calls
 * None.
 */
static pok_bool_t hw_slot_is(const hw_slot_t* slot,
    ja_gdb_hw_type_e type, uintptr_t addr, size_t len,
    jet_space_id space_id)
{
    return slot->used && (slot->type == type) && (slot->addr == addr)
        && (slot->len == len) && (slot->space_id == space_id);
}

/**
 * @funcname `ja_gdb_hw_insert`
 *
 * @description
 * Inserts a hardware breakpoint or watchpoint into a free slot (written to the
 * debug registers when the system resumes); an existing one is kept.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `len` | `size_t` | [in] The length. |
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @return TRUE if it is in place.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `uint32_t` | The number of slots |
 * | `i` | `uint32_t` | The index of a slot |
 * | `inserted` | `pok_bool_t` | Whether it is in place |
 * | `slots` | `hw_slot_t *` | The slots of the type |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_HW_INSERT_01}: `ja_gdb_hw_insert()` shall
 *   insert a hardware breakpoint or watchpoint that the debug registers support
 *   into a free slot of its kind, keep one that exists, and tell whether it is
 *   in place.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_INSERT_01}: If `hw_area_supported()` returns
 *   false for `type`, `addr` and `len`, `ja_gdb_hw_insert()` shall return
 *   `FALSE` and leave the slots as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_INSERT_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_INSERT_02}: If `hw_area_supported()` returns
 *   true and `hw_slot_is()` returns true for one of the slots that
 *   `hw_slots_of()` gives for `type` and `type`, `addr`, `len` and `space_id`,
 *   `ja_gdb_hw_insert()` shall return `TRUE` and leave the slots as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_INSERT_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03}: If `hw_area_supported()` returns
 *   true, `hw_slot_is()` returns false for every slot that `hw_slots_of()`
 *   gives for `type`, and `used` of one of these slots is false,
 *   `ja_gdb_hw_insert()` shall set, in the first such slot, `used` to `TRUE`
 *   and `type`, `addr`, `len` and `space_id` to the arguments of the same
 *   names, and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_INSERT_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_INSERT_04}: If `hw_area_supported()` returns
 *   true, `hw_slot_is()` returns false for every slot that `hw_slots_of()`
 *   gives for `type`, and `used` of every one of these slots is true,
 *   `ja_gdb_hw_insert()` shall return `FALSE` and leave the slots as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_INSERT_01}
 *
 * @design
 * 1. Set `inserted` to `FALSE`. \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_04}
 * 2. Call `hw_slots_of()` with the arguments `type` and the address of `count`,
 *    and store the result in `slots`.
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_04}
 * 3. If `hw_area_supported(type, addr, len)` is true:
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_04}
 *    1. Set `i` to `0U`; then while `i` is less than `count`, repeat (after
 *       each pass: increment `i`): \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_02},
 *       \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03},
 *       \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_04}
 *       1. If `hw_slot_is(&slots[i], type, addr, len, space_id)` is true:
 *          \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_02}
 *          1. Set `inserted` to `TRUE`.
 *    2. Set `i` to `0U`; then while `i` is less than `count` and `inserted` is
 *       false, repeat (after each pass: increment `i`):
 *       \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03},
 *       \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_04}
 *       1. If `slots[i].used` is false:
 *          1. Set `slots[i].used` to `TRUE`.
 *             \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03}
 *          2. Set `slots[i].type` to `type`.
 *             \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03}
 *          3. Set `slots[i].addr` to `addr`.
 *             \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03}
 *          4. Set `slots[i].len` to `len`.
 *             \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03}
 *          5. Set `slots[i].space_id` to `space_id`.
 *             \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03}
 *          6. Set `inserted` to `TRUE`.
 *             \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03}
 * 4. Return `inserted`. \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_INSERT_04}
 *
 * @calls
 * - `hw_slots_of()`
 * - `hw_area_supported()`
 * - `hw_slot_is()`
 */
pok_bool_t ja_gdb_hw_insert(ja_gdb_hw_type_e type, uintptr_t addr,
    size_t len, jet_space_id space_id)
{
    pok_bool_t inserted = FALSE;
    uint32_t count;
    hw_slot_t* slots = hw_slots_of(type, &count);

    if (hw_area_supported(type, addr, len))
    {
        for (uint32_t i = 0U; i < count; i++)
        {
            if (hw_slot_is(&slots[i], type, addr, len, space_id)) {
                inserted = TRUE; // Already there.
            }
        }

        for (uint32_t i = 0U; (i < count) && !inserted; i++)
        {
            if (!slots[i].used)
            {
                slots[i].used = TRUE;
                slots[i].type = type;
                slots[i].addr = addr;
                slots[i].len = len;
                slots[i].space_id = space_id;
                inserted = TRUE;
            }
        }
    }

    return inserted;
}

/**
 * @funcname `ja_gdb_hw_remove`
 *
 * @description
 * Removes a hardware breakpoint or watchpoint.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 * | `addr` | `uintptr_t` | [in] The address. |
 * | `len` | `size_t` | [in] The length. |
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @return TRUE if there was one.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `uint32_t` | The number of slots |
 * | `i` | `uint32_t` | The index of a slot |
 * | `removed` | `pok_bool_t` | Whether it was removed |
 * | `slots` | `hw_slot_t *` | The slots of the type |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_HW_REMOVE_01}: `ja_gdb_hw_remove()` shall
 *   remove a hardware breakpoint or watchpoint and tell whether there was one.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_01}: For each of the slots that
 *   `hw_slots_of()` gives for `type` for which `hw_slot_is()` returns true with
 *   `type`, `addr`, `len` and `space_id`, `ja_gdb_hw_remove()` shall set `used`
 *   to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_REMOVE_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_02}: If `hw_slot_is()` returns true
 *   for at least one of the slots that `hw_slots_of()` gives for `type`,
 *   `ja_gdb_hw_remove()` shall return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_REMOVE_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_03}: If `hw_slot_is()` returns false
 *   for every slot that `hw_slots_of()` gives for `type`, `ja_gdb_hw_remove()`
 *   shall return `FALSE` and leave the slots as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_REMOVE_01}
 *
 * @design
 * 1. Set `removed` to `FALSE`. \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_03}
 * 2. Call `hw_slots_of()` with the arguments `type` and the address of `count`,
 *    and store the result in `slots`.
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_03}
 * 3. Set `i` to `0U`; then while `i` is less than `count`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_03}
 *    1. If `hw_slot_is(&slots[i], type, addr, len, space_id)` is true:
 *       \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_01},
 *       \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_02}
 *       1. Set `slots[i].used` to `FALSE`.
 *       2. Set `removed` to `TRUE`.
 * 4. Return `removed`. \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_03}
 *
 * @calls
 * - `hw_slots_of()`
 * - `hw_slot_is()`
 */
pok_bool_t ja_gdb_hw_remove(ja_gdb_hw_type_e type, uintptr_t addr,
    size_t len, jet_space_id space_id)
{
    pok_bool_t removed = FALSE;
    uint32_t count;
    hw_slot_t* slots = hw_slots_of(type, &count);

    for (uint32_t i = 0U; i < count; i++)
    {
        if (hw_slot_is(&slots[i], type, addr, len, space_id))
        {
            slots[i].used = FALSE;
            removed = TRUE;
        }
    }

    return removed;
}

/**
 * @funcname `ja_gdb_hw_remove_all`
 *
 * @description
 * Removes every hardware breakpoint and watchpoint.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_breakpoints` | `hw_slot_t[6]` | The hardware breakpoints |
 * | `hw_watchpoints` | `hw_slot_t[4]` | The hardware watchpoints |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a slot |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_HW_REMOVE_ALL_01}: `ja_gdb_hw_remove_all()`
 *   shall remove every hardware breakpoint and watchpoint.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_015}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_ALL_01}: `ja_gdb_hw_remove_all()`
 *   shall set `used` of each of the `JA_GDB_HW_BREAKPOINTS` elements of
 *   `hw_breakpoints` and of each of the `JA_GDB_HW_WATCHPOINTS` elements of
 *   `hw_watchpoints` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_REMOVE_ALL_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_BREAKPOINTS`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_ALL_01}
 *    1. Set `hw_breakpoints[i].used` to `FALSE`.
 * 2. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_WATCHPOINTS`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_REMOVE_ALL_01}
 *    1. Set `hw_watchpoints[i].used` to `FALSE`.
 *
 * @calls
 * None.
 */
void ja_gdb_hw_remove_all(void)
{
    for (uint32_t i = 0U; i < JA_GDB_HW_BREAKPOINTS; i++)
    {
        hw_breakpoints[i].used = FALSE;
    }

    for (uint32_t i = 0U; i < JA_GDB_HW_WATCHPOINTS; i++)
    {
        hw_watchpoints[i].used = FALSE;
    }
}

/**
 * @funcname `hw_slot_enabled`
 *
 * @description
 * Tells whether a slot is enabled for the kernel and a space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `slot` | `const hw_slot_t *` | [in] The slot. |
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @return TRUE if it is.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_HW_SLOT_ENABLED_01}: `hw_slot_enabled()` shall tell whether
 *   a slot is in effect for a space: one of the kernel or of that space.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_HW_SLOT_ENABLED_01}: If `used` of `slot` is true and its
 *   `space_id` is 0 or is equal to `space_id`, `hw_slot_enabled()` shall return
 *   true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_SLOT_ENABLED_01}
 * - \llrid{LLR_GDB_HW_SLOT_ENABLED_02}: If `used` of `slot` is false, or its
 *   `space_id` is neither 0 nor equal to `space_id`, `hw_slot_enabled()` shall
 *   return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_SLOT_ENABLED_01}
 *
 * @design
 * 1. Return
 *    `slot->used && ((slot->space_id == 0U) || (slot->space_id == space_id))`.
 *    \llrref{LLR_GDB_HW_SLOT_ENABLED_01}, \llrref{LLR_GDB_HW_SLOT_ENABLED_02}
 *
 * @calls
 * None.
 */
static pok_bool_t hw_slot_enabled(const hw_slot_t* slot,
    jet_space_id space_id)
{
    return slot->used
        && ((slot->space_id == 0U) || (slot->space_id == space_id));
}

/**
 * @funcname `hw_breakpoint_control`
 *
 * @description
 * Returns the DBGBCR value of a breakpoint: enabled, at EL1 or EL0, all 4
 * bytes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `slot` | `const hw_slot_t *` | [in] The slot. |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pmc` | `uint64_t` | The exception level |
 *
 * @hlr
 * - \hlrid{HLR_GDB_HW_BREAKPOINT_CONTROL_01}: `hw_breakpoint_control()` shall
 *   give the control value of a breakpoint: enabled, at exception level 1 for
 *   the kernel or 0 for a space, on its 4 bytes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_HW_BREAKPOINT_CONTROL_01}: If `space_id` of `slot` is 0,
 *   `hw_breakpoint_control()` shall return `BCR_E | BCR_PMC_EL1 | BCR_BAS_ALL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_BREAKPOINT_CONTROL_01}
 * - \llrid{LLR_GDB_HW_BREAKPOINT_CONTROL_02}: If `space_id` of `slot` is not 0,
 *   `hw_breakpoint_control()` shall return `BCR_E | BCR_PMC_EL0 | BCR_BAS_ALL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_BREAKPOINT_CONTROL_01}
 *
 * @design
 * 1. Set `pmc` to `BCR_PMC_EL1` if `slot->space_id` is equal to `0U`, otherwise
 *    `BCR_PMC_EL0`. \llrref{LLR_GDB_HW_BREAKPOINT_CONTROL_01},
 *    \llrref{LLR_GDB_HW_BREAKPOINT_CONTROL_02}
 * 2. Return `BCR_E | pmc | BCR_BAS_ALL`.
 *    \llrref{LLR_GDB_HW_BREAKPOINT_CONTROL_01},
 *    \llrref{LLR_GDB_HW_BREAKPOINT_CONTROL_02}
 *
 * @calls
 * None.
 */
static uint64_t hw_breakpoint_control(const hw_slot_t* slot)
{
    uint64_t pmc = (slot->space_id == 0U) ? BCR_PMC_EL1 : BCR_PMC_EL0;

    return BCR_E | pmc | BCR_BAS_ALL;
}

/**
 * @funcname `hw_watchpoint_control`
 *
 * @description
 * Returns the DBGWCR value of a watchpoint: enabled, at EL1 or EL0, loads
 * and/or stores, the bytes of the double word.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `slot` | `const hw_slot_t *` | [in] The slot. |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bytes` | `uint64_t` | The bytes watched |
 * | `lsc` | `uint64_t` | Loads and/or stores |
 * | `pac` | `uint64_t` | The exception level |
 *
 * @hlr
 * - \hlrid{HLR_GDB_HW_WATCHPOINT_CONTROL_01}: `hw_watchpoint_control()` shall
 *   give the control value of a watchpoint: enabled, at exception level 1 or 0,
 *   for stores, loads or both, on its bytes of the double word.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_HW_WATCHPOINT_CONTROL_01}: `hw_watchpoint_control()` shall
 *   return `WCR_E` with the privilege field, the access field and, at
 *   `WCR_BAS_SHIFT`, the byte mask: as many 1 bits as `len` of `slot`, shifted
 *   left by the low 3 bits of its `addr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_WATCHPOINT_CONTROL_01}
 * - \llrid{LLR_GDB_HW_WATCHPOINT_CONTROL_02}: `hw_watchpoint_control()` shall
 *   take as the privilege field `WCR_PAC_EL1` if `space_id` of `slot` is 0, and
 *   `WCR_PAC_EL0` if it is not.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_WATCHPOINT_CONTROL_01}
 * - \llrid{LLR_GDB_HW_WATCHPOINT_CONTROL_03}: If `type` of `slot` is
 *   `JA_GDB_HW_WRITE` or `JA_GDB_HW_READ`, `hw_watchpoint_control()` shall take
 *   `WCR_LSC_STORE` or `WCR_LSC_LOAD`, in this order, as the access field.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_WATCHPOINT_CONTROL_01}
 * - \llrid{LLR_GDB_HW_WATCHPOINT_CONTROL_04}: If `type` of `slot` is neither
 *   `JA_GDB_HW_WRITE` nor `JA_GDB_HW_READ`, `hw_watchpoint_control()` shall
 *   take `WCR_LSC_STORE | WCR_LSC_LOAD` as the access field.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_WATCHPOINT_CONTROL_01}
 *
 * @design
 * 1. Set `pac` to `WCR_PAC_EL1` if `slot->space_id` is equal to `0U`, otherwise
 *    `WCR_PAC_EL0`. \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_02}
 * 2. Set `bytes` to `((uint64_t)1U << slot->len) - 1U`.
 *    \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_01}
 * 3. Depending on `slot->type`: \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_03},
 *    \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_04}
 *    1. Case `JA_GDB_HW_WRITE`: \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_03}
 *       1. Set `lsc` to `WCR_LSC_STORE`.
 *       2. Leave the switch.
 *    2. Case `JA_GDB_HW_READ`: \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_03}
 *       1. Set `lsc` to `WCR_LSC_LOAD`.
 *       2. Leave the switch.
 *    3. Otherwise (default): \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_04}
 *       1. Set `lsc` to `WCR_LSC_STORE | WCR_LSC_LOAD`.
 *       2. Leave the switch.
 * 4. Return
 *    `WCR_E | pac | lsc | ((bytes << (slot->addr & 7U)) << WCR_BAS_SHIFT)`.
 *    \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_01},
 *    \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_02},
 *    \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_03},
 *    \llrref{LLR_GDB_HW_WATCHPOINT_CONTROL_04}
 *
 * @calls
 * None.
 */
static uint64_t hw_watchpoint_control(const hw_slot_t* slot)
{
    uint64_t pac = (slot->space_id == 0U) ? WCR_PAC_EL1 : WCR_PAC_EL0;
    uint64_t bytes = ((uint64_t)1U << slot->len) - 1U;
    uint64_t lsc;

    switch (slot->type)
    {
    case JA_GDB_HW_WRITE: lsc = WCR_LSC_STORE; break;
    case JA_GDB_HW_READ:  lsc = WCR_LSC_LOAD; break;
    default:              lsc = WCR_LSC_STORE | WCR_LSC_LOAD; break;
    }

    // The register holds a double word; the bytes in it are selected.
    return WCR_E | pac | lsc
        | ((bytes << (slot->addr & 7U)) << WCR_BAS_SHIFT);
}

/**
 * @funcname `hw_any_used`
 *
 * @description
 * Tells whether any hardware breakpoint or watchpoint is set.
 *
 * @return TRUE if one is.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_breakpoints` | `hw_slot_t[6]` | The hardware breakpoints |
 * | `hw_breakpoints_n` | `uint32_t` | The number of breakpoint registers used |
 * | `hw_watchpoints` | `hw_slot_t[4]` | The hardware watchpoints |
 * | `hw_watchpoints_n` | `uint32_t` | The number of watchpoint registers used |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `any` | `pok_bool_t` | Whether one is |
 * | `i` | `uint32_t` | The index of a slot |
 *
 * @hlr
 * - \hlrid{HLR_GDB_HW_ANY_USED_01}: `hw_any_used()` shall tell whether any
 *   hardware breakpoint or watchpoint is set.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_HW_ANY_USED_01}: If `used` is true for one of the first
 *   `hw_breakpoints_n` elements of `hw_breakpoints` or for one of the first
 *   `hw_watchpoints_n` elements of `hw_watchpoints`, `hw_any_used()` shall
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_ANY_USED_01}
 * - \llrid{LLR_GDB_HW_ANY_USED_02}: If `used` is false for all of the first
 *   `hw_breakpoints_n` elements of `hw_breakpoints` and all of the first
 *   `hw_watchpoints_n` elements of `hw_watchpoints`, `hw_any_used()` shall
 *   return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_HW_ANY_USED_01}
 *
 * @design
 * 1. Set `any` to `FALSE`. \llrref{LLR_GDB_HW_ANY_USED_01},
 *    \llrref{LLR_GDB_HW_ANY_USED_02}
 * 2. Set `i` to `0U`; then while `i` is less than `hw_breakpoints_n`, repeat
 *    (after each pass: increment `i`): \llrref{LLR_GDB_HW_ANY_USED_01},
 *    \llrref{LLR_GDB_HW_ANY_USED_02}
 *    1. Set `any` to `any || hw_breakpoints[i].used`.
 * 3. Set `i` to `0U`; then while `i` is less than `hw_watchpoints_n`, repeat
 *    (after each pass: increment `i`): \llrref{LLR_GDB_HW_ANY_USED_01},
 *    \llrref{LLR_GDB_HW_ANY_USED_02}
 *    1. Set `any` to `any || hw_watchpoints[i].used`.
 * 4. Return `any`. \llrref{LLR_GDB_HW_ANY_USED_01},
 *    \llrref{LLR_GDB_HW_ANY_USED_02}
 *
 * @calls
 * None.
 */
static pok_bool_t hw_any_used(void)
{
    pok_bool_t any = FALSE;

    for (uint32_t i = 0U; i < hw_breakpoints_n; i++)
    {
        any = any || hw_breakpoints[i].used;
    }

    for (uint32_t i = 0U; i < hw_watchpoints_n; i++)
    {
        any = any || hw_watchpoints[i].used;
    }

    return any;
}

/**
 * @funcname `ja_gdb_hw_program`
 *
 * @description
 * Writes the debug registers for a space: the breakpoints and watchpoints of
 * the kernel and of that space only; nothing is done if none is used or
 * enabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `space_id` | `jet_space_id` | [in] The space. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_breakpoints` | `hw_slot_t[6]` | The hardware breakpoints |
 * | `hw_breakpoints_n` | `uint32_t` | The number of breakpoint registers used |
 * | `hw_watchpoints` | `hw_slot_t[4]` | The hardware watchpoints |
 * | `hw_watchpoints_n` | `uint32_t` | The number of watchpoint registers used |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `enabled` | `pok_bool_t` | Whether some register is enabled now |
 * | `hw_enabled` | `static pok_bool_t` | Whether some debug register is enabled |
 * | `i` | `uint32_t` | The index of a slot |
 * | `on` | `pok_bool_t` | Whether it is enabled |
 * | `slot` | `const hw_slot_t *` | The slot |
 * | `was_masked` | `pok_bool_t` | Whether debug exceptions were masked |
 *
 * @hlr
 * - \hlrid{HLR_GDB_JA_GDB_HW_PROGRAM_01}: `ja_gdb_hw_program()` shall put into
 *   the debug registers the breakpoints and watchpoints of the kernel and of a
 *   space only, and enable the debug exceptions only if one is in effect.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_JA_GDB_HW_PROGRAM_01}: If `hw_enabled` is false and
 *   `hw_any_used()` returns false, `ja_gdb_hw_program()` shall return without
 *   writing a register.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_HW_PROGRAM_01}
 * - \llrid{LLR_GDB_JA_GDB_HW_PROGRAM_02}: If `hw_enabled` is true or
 *   `hw_any_used()` returns true, `ja_gdb_hw_program()` shall call
 *   `cpu_debug_mask()` and then `hw_breakpoint_write()` with `i`, `addr` of
 *   `hw_breakpoints[i]` and its control for each `i` less than
 *   `hw_breakpoints_n`, and `hw_watchpoint_write()` with `i`, `addr` of
 *   `hw_watchpoints[i]` with its low 3 bits cleared and its control for each
 *   `i` less than `hw_watchpoints_n`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_HW_PROGRAM_01}
 * - \llrid{LLR_GDB_JA_GDB_HW_PROGRAM_03}: `ja_gdb_hw_program()` shall take as
 *   the control of a slot the result of `hw_breakpoint_control()` or
 *   `hw_watchpoint_control()` for the slot if `hw_slot_enabled()` returns true
 *   for the slot and `space_id`, and 0 if it returns false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_HW_PROGRAM_01}
 * - \llrid{LLR_GDB_JA_GDB_HW_PROGRAM_04}: If `hw_slot_enabled()` returned true
 *   for at least one slot, `ja_gdb_hw_program()` shall then call
 *   `cpu_mdscr_set()` once with `MDSCR_MDE`, call `isb()` and set `hw_enabled`
 *   to true.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_HW_PROGRAM_01}
 * - \llrid{LLR_GDB_JA_GDB_HW_PROGRAM_05}: If `hw_slot_enabled()` returned false
 *   for every slot, `ja_gdb_hw_program()` shall then call `cpu_mdscr_clear()`
 *   once with `MDSCR_MDE`, call `isb()` and set `hw_enabled` to false.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_HW_PROGRAM_01}
 * - \llrid{LLR_GDB_JA_GDB_HW_PROGRAM_06}: If and only if `cpu_debug_masked()`
 *   returned false before the call of `cpu_debug_mask()`, `ja_gdb_hw_program()`
 *   shall call `cpu_debug_unmask()` at the end.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_JA_GDB_HW_PROGRAM_01}
 *
 * @design
 * 1. If `hw_enabled` is true or `hw_any_used()` is true:
 *    \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_01},
 *    \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_02},
 *    \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_03},
 *    \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_04},
 *    \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_05},
 *    \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_06}
 *    1. Call `cpu_debug_masked()`, and store the result in `was_masked`.
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_06}
 *    2. Set `enabled` to `FALSE`. \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_04},
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_05}
 *    3. Call `cpu_debug_mask()`. \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_02}
 *    4. Set `i` to `0U`; then while `i` is less than `hw_breakpoints_n`, repeat
 *       (after each pass: increment `i`):
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_02},
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_03},
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_04},
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_05}
 *       1. Set `slot` to the address of `hw_breakpoints[i]`.
 *       2. Call `hw_slot_enabled()` with the arguments `slot` and `space_id`,
 *          and store the result in `on`.
 *       3. Call `hw_breakpoint_write()` with the arguments `i`, `slot->addr`
 *          and `hw_breakpoint_control(slot)` if `on` is true, otherwise `0U`.
 *       4. Set `enabled` to `enabled || on`.
 *    5. Set `i` to `0U`; then while `i` is less than `hw_watchpoints_n`, repeat
 *       (after each pass: increment `i`):
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_02},
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_03},
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_04},
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_05}
 *       1. Set `slot` to the address of `hw_watchpoints[i]`.
 *       2. Call `hw_slot_enabled()` with the arguments `slot` and `space_id`,
 *          and store the result in `on`.
 *       3. Call `hw_watchpoint_write()` with the arguments `i`,
 *          `slot->addr & ~(uint64_t)7U` and `hw_watchpoint_control(slot)` if
 *          `on` is true, otherwise `0U`.
 *       4. Set `enabled` to `enabled || on`.
 *    6. If `enabled` is true: \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_04}
 *       1. Call `cpu_mdscr_set()` with the argument `MDSCR_MDE`.
 *    7. Otherwise: \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_05}
 *       1. Call `cpu_mdscr_clear()` with the argument `MDSCR_MDE`.
 *    8. Call `isb()`. \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_04},
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_05}
 *    9. Set `hw_enabled` to `enabled`. \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_04},
 *       \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_05}
 *    10. If `was_masked` is false: \llrref{LLR_GDB_JA_GDB_HW_PROGRAM_06}
 *        1. Call `cpu_debug_unmask()`.
 *
 * @calls
 * - `hw_any_used()`
 * - `cpu_debug_masked()`
 * - `cpu_debug_mask()`
 * - `hw_slot_enabled()`
 * - `hw_breakpoint_write()`
 * - `hw_breakpoint_control()`
 * - `hw_watchpoint_write()`
 * - `hw_watchpoint_control()`
 * - `cpu_mdscr_set()`
 * - `cpu_mdscr_clear()`
 * - `isb()`
 * - `cpu_debug_unmask()`
 */
void ja_gdb_hw_program(jet_space_id space_id)
{
    // Whether some debug register is enabled.
    static pok_bool_t hw_enabled = FALSE;

    if (hw_enabled || hw_any_used())
    {
        pok_bool_t was_masked = cpu_debug_masked();
        pok_bool_t enabled = FALSE;

        cpu_debug_mask();

        for (uint32_t i = 0U; i < hw_breakpoints_n; i++)
        {
            const hw_slot_t* slot = &hw_breakpoints[i];
            pok_bool_t on = hw_slot_enabled(slot, space_id);

            hw_breakpoint_write(i, slot->addr,
                on ? hw_breakpoint_control(slot) : 0U);
            enabled = enabled || on;
        }

        for (uint32_t i = 0U; i < hw_watchpoints_n; i++)
        {
            const hw_slot_t* slot = &hw_watchpoints[i];
            pok_bool_t on = hw_slot_enabled(slot, space_id);

            hw_watchpoint_write(i, slot->addr & ~(uint64_t)7U,
                on ? hw_watchpoint_control(slot) : 0U);
            enabled = enabled || on;
        }

        if (enabled) {
            cpu_mdscr_set(MDSCR_MDE);
        } else {
            cpu_mdscr_clear(MDSCR_MDE);
        }
        isb();
        hw_enabled = enabled;

        if (!was_masked) {
            cpu_debug_unmask();
        }
    }
}

/**
 * @funcname `ja_gdb_hw_enter`
 *
 * @description
 * Called when the GDB server starts: nothing to do, it runs with debug
 * exceptions masked.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_HW_ENTER_01}: `ja_gdb_hw_enter()` shall leave
 *   the debug registers as they are when the GDB server runs, which it does
 *   with debug exceptions masked.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_ENTER_01}: `ja_gdb_hw_enter()` shall return
 *   without reading or writing a register.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_ENTER_01}
 *
 * @design
 * 1. Do nothing. \llrref{LLR_GDB_ARM64_JA_GDB_HW_ENTER_01}
 *
 * @calls
 * None.
 */
void ja_gdb_hw_enter(void)
{
    // The GDB server runs with debug exceptions masked.
}

/**
 * @funcname `ja_gdb_hw_leave`
 *
 * @description
 * Writes the debug registers for the current space when the system resumes.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_HW_LEAVE_01}: `ja_gdb_hw_leave()` shall put the
 *   breakpoints and watchpoints of the current space into the debug registers
 *   when the module resumes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_005}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_HW_LEAVE_01}: `ja_gdb_hw_leave()` shall call
 *   `ja_gdb_hw_program()` once with the result of `ja_space_get_current()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_HW_LEAVE_01}
 *
 * @design
 * 1. Call `ja_gdb_hw_program()` with the argument `ja_space_get_current()`.
 *    \llrref{LLR_GDB_ARM64_JA_GDB_HW_LEAVE_01}
 *
 * @calls
 * - `ja_gdb_hw_program()`
 * - `ja_space_get_current()`
 */
void ja_gdb_hw_leave(void)
{
    ja_gdb_hw_program(ja_space_get_current());
}

/**
 * @funcname `hw_stop_reason`
 *
 * @description
 * Returns the stop reason of a hardware breakpoint or watchpoint type.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `type` | `ja_gdb_hw_type_e` | [in] The type. |
 *
 * @return The stop reason.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `reason` | `jet_gdb_stop_reason_e` | The stop reason |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_HW_STOP_REASON_01}: `hw_stop_reason()` shall give the
 *   stop reason of a type of hardware breakpoint or watchpoint.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_HW_STOP_REASON_01}: If `type` is `JA_GDB_HW_EXEC`,
 *   `JA_GDB_HW_WRITE` or `JA_GDB_HW_READ`, `hw_stop_reason()` shall return
 *   `JET_GDB_STOP_HW_BREAKPOINT`, `JET_GDB_STOP_WATCH` or
 *   `JET_GDB_STOP_RWATCH`, in this order.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_HW_STOP_REASON_01}
 * - \llrid{LLR_GDB_ARM64_HW_STOP_REASON_02}: If `type` is none of
 *   `JA_GDB_HW_EXEC`, `JA_GDB_HW_WRITE` and `JA_GDB_HW_READ`,
 *   `hw_stop_reason()` shall return `JET_GDB_STOP_AWATCH`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_HW_STOP_REASON_01}
 *
 * @design
 * 1. Depending on `type`: \llrref{LLR_GDB_ARM64_HW_STOP_REASON_01},
 *    \llrref{LLR_GDB_ARM64_HW_STOP_REASON_02}
 *    1. Case `JA_GDB_HW_EXEC`: \llrref{LLR_GDB_ARM64_HW_STOP_REASON_01}
 *       1. Set `reason` to `JET_GDB_STOP_HW_BREAKPOINT`.
 *       2. Leave the switch.
 *    2. Case `JA_GDB_HW_WRITE`: \llrref{LLR_GDB_ARM64_HW_STOP_REASON_01}
 *       1. Set `reason` to `JET_GDB_STOP_WATCH`.
 *       2. Leave the switch.
 *    3. Case `JA_GDB_HW_READ`: \llrref{LLR_GDB_ARM64_HW_STOP_REASON_01}
 *       1. Set `reason` to `JET_GDB_STOP_RWATCH`.
 *       2. Leave the switch.
 *    4. Otherwise (default): \llrref{LLR_GDB_ARM64_HW_STOP_REASON_02}
 *       1. Set `reason` to `JET_GDB_STOP_AWATCH`.
 *       2. Leave the switch.
 * 2. Return `reason`. \llrref{LLR_GDB_ARM64_HW_STOP_REASON_01},
 *    \llrref{LLR_GDB_ARM64_HW_STOP_REASON_02}
 *
 * @calls
 * None.
 */
static jet_gdb_stop_reason_e hw_stop_reason(ja_gdb_hw_type_e type)
{
    jet_gdb_stop_reason_e reason;

    switch (type)
    {
    case JA_GDB_HW_EXEC:  reason = JET_GDB_STOP_HW_BREAKPOINT; break;
    case JA_GDB_HW_WRITE: reason = JET_GDB_STOP_WATCH; break;
    case JA_GDB_HW_READ:  reason = JET_GDB_STOP_RWATCH; break;
    default:              reason = JET_GDB_STOP_AWATCH; break;
    }

    return reason;
}

/**
 * @funcname `pok_trap_addr`
 *
 * @description
 * Returns the address of pok_trap() (the brk of a GDB request).
 *
 * @return The address.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_POK_TRAP_ADDR_01}: `pok_trap_addr()` shall give the
 *   address of the trap by which the GDB partition asks to stop the module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_POK_TRAP_ADDR_01}: `pok_trap_addr()` shall return the
 *   address of `pok_trap()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_POK_TRAP_ADDR_01}
 *
 * @design
 * 1. Return `(uintptr_t)&pok_trap`. \llrref{LLR_GDB_ARM64_POK_TRAP_ADDR_01}
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.1 deviation: the address of pok_trap() is compared
 * with the address of the breakpoint as an integer.
 */
static uintptr_t pok_trap_addr(void)
{
    return (uintptr_t)&pok_trap;
}

/**
 * @funcname `trap_space`
 *
 * @description
 * Returns the space of the code that trapped.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `from_user` | `pok_bool_t` | [in] Whether it was user code. |
 *
 * @return The current space, or 0 for the kernel.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_TRAP_SPACE_01}: `trap_space()` shall give the memory
 *   space of the code that trapped: the current space for user code and 0 for
 *   the kernel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_TRAP_SPACE_01}: If `from_user` is true, `trap_space()`
 *   shall return the result of `ja_space_get_current()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_TRAP_SPACE_01}
 * - \llrid{LLR_GDB_ARM64_TRAP_SPACE_02}: If `from_user` is false,
 *   `trap_space()` shall return 0 without calling `ja_space_get_current()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_TRAP_SPACE_01}
 *
 * @design
 * 1. Return `ja_space_get_current()` if `from_user` is true, otherwise `0U`.
 *    \llrref{LLR_GDB_ARM64_TRAP_SPACE_01}, \llrref{LLR_GDB_ARM64_TRAP_SPACE_02}
 *
 * @calls
 * - `ja_space_get_current()`
 */
static jet_space_id trap_space(pok_bool_t from_user)
{
    return from_user ? ja_space_get_current() : 0U;
}

/**
 * @funcname `brk_trap`
 *
 * @description
 * Handles the breakpoint instruction (ELR is at it): a request of the GDB
 * partition (pok_trap()), a GDB breakpoint, or the program's own breakpoint
 * (GDB goes on after it, as on x86).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 * | `from_user` | `pok_bool_t` | [in] Whether it came from user space. |
 *
 * @return TRUE if GDB handled it.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `handled` | `pok_bool_t` | Whether GDB handled it |
 * | `pc` | `uintptr_t` | The address of the brk |
 *
 * @hlr
 * - \hlrid{HLR_GDB_BRK_TRAP_01}: `brk_trap()` shall handle the breakpoint
 *   instruction: a request of the GDB partition, a software breakpoint of the
 *   debugger, or a breakpoint of the program itself (the debugger going on
 *   after it); and tell whether the debugger handled it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_BRK_TRAP_01}: If `from_user` is false and the address of the
 *   trap (`elr` of `frame`) is the result of `pok_trap_addr()`, `brk_trap()`
 *   shall add `JA_GDB_BREAKPOINT_SIZE` to `elr` of `frame` and return the
 *   result of `jet_gdb_trap()` for `frame`, `JET_GDB_STOP_REQUEST` and the
 *   address of the trap.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_BRK_TRAP_01}
 * - \llrid{LLR_GDB_BRK_TRAP_02}: If `from_user` is true or the address of the
 *   trap is not the result of `pok_trap_addr()`, and `jet_gdb_is_breakpoint()`
 *   returns true for the result of `trap_space()` for `from_user` and the
 *   address of the trap, `brk_trap()` shall return the result of
 *   `jet_gdb_trap()` for `frame`, `JET_GDB_STOP_SW_BREAKPOINT` and the address
 *   of the trap, leaving `elr` of `frame` as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_BRK_TRAP_01}
 * - \llrid{LLR_GDB_BRK_TRAP_03}: If `from_user` is true or the address of the
 *   trap is not the result of `pok_trap_addr()`, and `jet_gdb_is_breakpoint()`
 *   returns false for the result of `trap_space()` for `from_user` and the
 *   address of the trap, `brk_trap()` shall add `JA_GDB_BREAKPOINT_SIZE` to
 *   `elr` of `frame` and return the result of `jet_gdb_trap()` for `frame`,
 *   `JET_GDB_STOP_TRAP` and the address of the trap.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_BRK_TRAP_01}
 * - \llrid{LLR_GDB_BRK_TRAP_04}: If and only if that `jet_gdb_trap()` for
 *   `JET_GDB_STOP_TRAP` returns false, `brk_trap()` shall set `elr` of `frame`
 *   back to the address of the trap.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_BRK_TRAP_01}
 *
 * @design
 * 1. Set `pc` to `frame->elr`. \llrref{LLR_GDB_BRK_TRAP_01},
 *    \llrref{LLR_GDB_BRK_TRAP_02}, \llrref{LLR_GDB_BRK_TRAP_03},
 *    \llrref{LLR_GDB_BRK_TRAP_04}
 * 2. If `from_user` is false and `pc` is equal to `pok_trap_addr()`:
 *    \llrref{LLR_GDB_BRK_TRAP_01}
 *    1. Set `frame->elr` to `pc + JA_GDB_BREAKPOINT_SIZE`.
 *    2. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `JET_GDB_STOP_REQUEST` and `pc`, and store the result in `handled`.
 * 3. Otherwise, if `jet_gdb_is_breakpoint(trap_space(from_user), pc)` is true:
 *    \llrref{LLR_GDB_BRK_TRAP_02}
 *    1. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `JET_GDB_STOP_SW_BREAKPOINT` and `pc`, and store the result in
 *       `handled`.
 * 4. Otherwise: \llrref{LLR_GDB_BRK_TRAP_03}, \llrref{LLR_GDB_BRK_TRAP_04}
 *    1. Set `frame->elr` to `pc + JA_GDB_BREAKPOINT_SIZE`.
 *    2. Call `jet_gdb_trap()` with the arguments `frame`, `JET_GDB_STOP_TRAP`
 *       and `pc`, and store the result in `handled`.
 *    3. If `handled` is false: \llrref{LLR_GDB_BRK_TRAP_04}
 *       1. Set `frame->elr` to `pc`.
 * 5. Return `handled`. \llrref{LLR_GDB_BRK_TRAP_01},
 *    \llrref{LLR_GDB_BRK_TRAP_02}, \llrref{LLR_GDB_BRK_TRAP_03}
 *
 * @calls
 * - `pok_trap_addr()`
 * - `jet_gdb_trap()`
 * - `jet_gdb_is_breakpoint()`
 * - `trap_space()`
 */
static pok_bool_t brk_trap(interrupt_frame* frame, pok_bool_t from_user)
{
    uintptr_t pc = frame->elr;
    pok_bool_t handled;

    if (!from_user && (pc == pok_trap_addr()))
    {
        // Go on after the instruction.
        frame->elr = pc + JA_GDB_BREAKPOINT_SIZE;
        handled = jet_gdb_trap(frame, JET_GDB_STOP_REQUEST, pc);
    }
    else if (jet_gdb_is_breakpoint(trap_space(from_user), pc))
    {
        // GDB expects the PC at the breakpoint it has inserted.
        handled = jet_gdb_trap(frame, JET_GDB_STOP_SW_BREAKPOINT, pc);
    }
    else
    {
        // The program's own breakpoint: GDB can go on after it, as on x86.
        frame->elr = pc + JA_GDB_BREAKPOINT_SIZE;
        handled = jet_gdb_trap(frame, JET_GDB_STOP_TRAP, pc);
        if (!handled) {
            frame->elr = pc;
        }
    }

    return handled;
}

/**
 * @funcname `step_trap`
 *
 * @description
 * Handles the software step exception: the step is done.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 *
 * @return TRUE if GDB handled it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `step_state` | `gdb_step_state_e` | The state of the single step |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_GDB_STEP_TRAP_01}: `step_trap()` shall end a single step at its
 *   exception and stop the module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_STEP_TRAP_01}: `step_trap()` shall set `step_state` to
 *   `STEP_NONE`, clear `SPSR_SS` in `spsr` of `frame`, and return the result of
 *   `jet_gdb_trap()` for `frame`, `JET_GDB_STOP_STEP` and `elr` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_STEP_TRAP_01}
 *
 * @design
 * 1. Set `step_state` to `STEP_NONE`. \llrref{LLR_GDB_STEP_TRAP_01}
 * 2. Keep only the bits `~SPSR_SS` of `frame->spsr`.
 *    \llrref{LLR_GDB_STEP_TRAP_01}
 * 3. Call `jet_gdb_trap()` with the arguments `frame`, `JET_GDB_STOP_STEP` and
 *    `frame->elr`, and return its result. \llrref{LLR_GDB_STEP_TRAP_01}
 *
 * @calls
 * - `jet_gdb_trap()`
 */
static pok_bool_t step_trap(interrupt_frame* frame)
{
    step_state = STEP_NONE;
    frame->spsr &= ~SPSR_SS;

    return jet_gdb_trap(frame, JET_GDB_STOP_STEP, frame->elr);
}

/**
 * @funcname `breakpoint_trap`
 *
 * @description
 * Handles a hardware breakpoint (the exception comes before the instruction):
 * one of the kernel or of the partition that trapped.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 * | `from_user` | `pok_bool_t` | [in] Whether it came from user space. |
 *
 * @return TRUE if GDB handled it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_breakpoints` | `hw_slot_t[6]` | The hardware breakpoints |
 * | `hw_breakpoints_n` | `uint32_t` | The number of breakpoint registers used |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `handled` | `pok_bool_t` | Whether GDB handled it |
 * | `i` | `uint32_t` | The index of a slot |
 * | `slot` | `const hw_slot_t *` | The slot |
 * | `space_id` | `jet_space_id` | The space of the code |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_BREAKPOINT_TRAP_01}: `breakpoint_trap()` shall stop
 *   the module at a hardware breakpoint of the kernel or of the partition that
 *   trapped.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_BREAKPOINT_TRAP_01}: `breakpoint_trap()` shall call
 *   `jet_gdb_trap()` with `frame`, `JET_GDB_STOP_HW_BREAKPOINT` and `addr` for
 *   the elements of `hw_breakpoints` (the first `hw_breakpoints_n`, in their
 *   order) whose `used` is true, whose `space_id` is the result of
 *   `trap_space()` for `from_user` and whose `addr` is `elr` of `frame`, until
 *   a call returns true, and return the result of the last call.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_BREAKPOINT_TRAP_01}
 * - \llrid{LLR_GDB_ARM64_BREAKPOINT_TRAP_02}: If every one of the first
 *   `hw_breakpoints_n` elements of `hw_breakpoints` has `used` false, or a
 *   `space_id` other than the result of `trap_space()`, or an `addr` other than
 *   `elr` of `frame`, `breakpoint_trap()` shall return `FALSE` without calling
 *   `jet_gdb_trap()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_BREAKPOINT_TRAP_01}
 *
 * @design
 * 1. Call `trap_space()` with the argument `from_user`, and store the result in
 *    `space_id`. \llrref{LLR_GDB_ARM64_BREAKPOINT_TRAP_01},
 *    \llrref{LLR_GDB_ARM64_BREAKPOINT_TRAP_02}
 * 2. Set `handled` to `FALSE`. \llrref{LLR_GDB_ARM64_BREAKPOINT_TRAP_01},
 *    \llrref{LLR_GDB_ARM64_BREAKPOINT_TRAP_02}
 * 3. Set `i` to `0U`; then while `i` is less than `hw_breakpoints_n` and
 *    `handled` is false, repeat (after each pass: increment `i`):
 *    \llrref{LLR_GDB_ARM64_BREAKPOINT_TRAP_01},
 *    \llrref{LLR_GDB_ARM64_BREAKPOINT_TRAP_02}
 *    1. Set `slot` to the address of `hw_breakpoints[i]`.
 *    2. If `slot->used` is true and `slot->space_id` is equal to `space_id` and
 *       `slot->addr` is equal to `frame->elr`:
 *       \llrref{LLR_GDB_ARM64_BREAKPOINT_TRAP_01}
 *       1. Call `jet_gdb_trap()` with the arguments `frame`,
 *          `JET_GDB_STOP_HW_BREAKPOINT` and `slot->addr`, and store the result
 *          in `handled`.
 * 4. Return `handled`. \llrref{LLR_GDB_ARM64_BREAKPOINT_TRAP_01},
 *    \llrref{LLR_GDB_ARM64_BREAKPOINT_TRAP_02}
 *
 * @calls
 * - `trap_space()`
 * - `jet_gdb_trap()`
 */
static pok_bool_t breakpoint_trap(interrupt_frame* frame,
    pok_bool_t from_user)
{
    jet_space_id space_id = trap_space(from_user);
    pok_bool_t handled = FALSE;

    for (uint32_t i = 0U; (i < hw_breakpoints_n) && !handled; i++)
    {
        const hw_slot_t* slot = &hw_breakpoints[i];

        // A breakpoint of this context: of the kernel or of the partition.
        if (slot->used && (slot->space_id == space_id)
            && (slot->addr == frame->elr))
        {
            handled = jet_gdb_trap(frame, JET_GDB_STOP_HW_BREAKPOINT,
                slot->addr);
        }
    }

    return handled;
}

/**
 * @funcname `distance`
 *
 * @description
 * Returns the distance of an address from an area (0 if inside).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `addr` | `uint64_t` | [in] The address. |
 * | `start` | `uint64_t` | [in] The start of the area. |
 * | `len` | `uint64_t` | [in] Its length. |
 *
 * @return The distance.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `result` | `uint64_t` | The distance |
 *
 * @hlr
 * - \hlrid{HLR_GDB_DISTANCE_01}: `distance()` shall give the distance of an
 *   address from an area, 0 inside it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_DISTANCE_01}: If `addr` is less than `start`, `distance()`
 *   shall return `start` minus `addr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DISTANCE_01}
 * - \llrid{LLR_GDB_DISTANCE_02}: If `addr` is not less than `start` plus `len`,
 *   `distance()` shall return `addr` minus `start` minus `len` plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DISTANCE_01}
 * - \llrid{LLR_GDB_DISTANCE_03}: If `addr` is not less than `start` and is less
 *   than `start` plus `len`, `distance()` shall return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_DISTANCE_01}
 *
 * @design
 * 1. Set `result` to `0U`. \llrref{LLR_GDB_DISTANCE_03}
 * 2. If `addr` is less than `start`: \llrref{LLR_GDB_DISTANCE_01}
 *    1. Set `result` to `start - addr`.
 * 3. Otherwise, if `addr` is greater than or equal to `(start + len)`:
 *    \llrref{LLR_GDB_DISTANCE_02}, \llrref{LLR_GDB_DISTANCE_03}
 *    1. Set `result` to `addr - (start + len) + 1U`.
 *       \llrref{LLR_GDB_DISTANCE_02}
 * 4. Return `result`. \llrref{LLR_GDB_DISTANCE_01},
 *    \llrref{LLR_GDB_DISTANCE_02}, \llrref{LLR_GDB_DISTANCE_03}
 *
 * @calls
 * None.
 */
static uint64_t distance(uint64_t addr, uint64_t start, uint64_t len)
{
    uint64_t result = 0U;

    if (addr < start) {
        result = start - addr;
    } else if (addr >= (start + len)) {
        result = addr - (start + len) + 1U;
    } else {
        // Inside.
    }

    return result;
}

/**
 * @funcname `watchpoint_trap`
 *
 * @description
 * Handles a hardware watchpoint: the exception comes before the access, and the
 * processor doesn't tell which watchpoint has matched: it's the one which is
 * the closest to the address of the access.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 * | `from_user` | `pok_bool_t` | [in] Whether it came from user space. |
 *
 * @return TRUE if GDB handled it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_watchpoints` | `hw_slot_t[4]` | The hardware watchpoints |
 * | `hw_watchpoints_n` | `uint32_t` | The number of watchpoint registers used |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `d` | `uint64_t` | The slot's distance |
 * | `fault_addr` | `uint64_t` | The address of the access |
 * | `found` | `const hw_slot_t *` | The closest watchpoint |
 * | `found_distance` | `uint64_t` | Its distance |
 * | `handled` | `pok_bool_t` | Whether GDB handled it |
 * | `i` | `uint32_t` | The index of a slot |
 * | `slot` | `const hw_slot_t *` | The slot |
 * | `space_id` | `jet_space_id` | The space of the code |
 *
 * @hlr
 * - \hlrid{HLR_GDB_WATCHPOINT_TRAP_01}: `watchpoint_trap()` shall stop the
 *   module at a hardware watchpoint of the kernel or of the partition that
 *   trapped: the one closest to the address of the fault, as the processor does
 *   not tell which one matched.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_GDB_WATCHPOINT_TRAP_01}: Among the first `hw_watchpoints_n`
 *   elements of `hw_watchpoints` whose `used` is true and whose `space_id` is
 *   the result of `trap_space()` for `from_user`, `watchpoint_trap()` shall
 *   take the one for which the result of `distance()` for FAR_EL1, its `addr`
 *   and its `len` is the smallest (the first one of them if several have that
 *   distance), and return the result of `jet_gdb_trap()` for `frame`, the
 *   result of `hw_stop_reason()` for its `type` and its `addr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_WATCHPOINT_TRAP_01}
 * - \llrid{LLR_GDB_WATCHPOINT_TRAP_02}: If every one of the first
 *   `hw_watchpoints_n` elements of `hw_watchpoints` has `used` false or a
 *   `space_id` other than the result of `trap_space()`, `watchpoint_trap()`
 *   shall return `FALSE` without calling `jet_gdb_trap()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_WATCHPOINT_TRAP_01}
 *
 * @design
 * 1. Call `trap_space()` with the argument `from_user`, and store the result in
 *    `space_id`. \llrref{LLR_GDB_WATCHPOINT_TRAP_01},
 *    \llrref{LLR_GDB_WATCHPOINT_TRAP_02}
 * 2. Set `fault_addr` to `read_sysreg(far_el1)`.
 *    \llrref{LLR_GDB_WATCHPOINT_TRAP_01}, \llrref{LLR_GDB_WATCHPOINT_TRAP_02}
 * 3. Set `found` to `NULL`. \llrref{LLR_GDB_WATCHPOINT_TRAP_01},
 *    \llrref{LLR_GDB_WATCHPOINT_TRAP_02}
 * 4. Set `found_distance` to `0U`. \llrref{LLR_GDB_WATCHPOINT_TRAP_01},
 *    \llrref{LLR_GDB_WATCHPOINT_TRAP_02}
 * 5. Set `handled` to `FALSE`. \llrref{LLR_GDB_WATCHPOINT_TRAP_01},
 *    \llrref{LLR_GDB_WATCHPOINT_TRAP_02}
 * 6. Set `i` to `0U`; then while `i` is less than `hw_watchpoints_n`, repeat
 *    (after each pass: increment `i`): \llrref{LLR_GDB_WATCHPOINT_TRAP_01},
 *    \llrref{LLR_GDB_WATCHPOINT_TRAP_02}
 *    1. Set `slot` to the address of `hw_watchpoints[i]`.
 *    2. If `slot->used` is true and `slot->space_id` is equal to `space_id`:
 *       \llrref{LLR_GDB_WATCHPOINT_TRAP_01}
 *       1. Call `distance()` with the arguments `fault_addr`, `slot->addr` and
 *          `slot->len`, and store the result in `d`.
 *       2. If `found` is NULL or `d` is less than `found_distance`:
 *          1. Set `found` to `slot`.
 *          2. Set `found_distance` to `d`.
 * 7. If `found` is not NULL: \llrref{LLR_GDB_WATCHPOINT_TRAP_01},
 *    \llrref{LLR_GDB_WATCHPOINT_TRAP_02}
 *    1. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `hw_stop_reason(found->type)` and `found->addr`, and store the result
 *       in `handled`. \llrref{LLR_GDB_WATCHPOINT_TRAP_01}
 * 8. Return `handled`. \llrref{LLR_GDB_WATCHPOINT_TRAP_01},
 *    \llrref{LLR_GDB_WATCHPOINT_TRAP_02}
 *
 * @calls
 * - `trap_space()`
 * - `distance()`
 * - `jet_gdb_trap()`
 * - `hw_stop_reason()`
 */
static pok_bool_t watchpoint_trap(interrupt_frame* frame,
    pok_bool_t from_user)
{
    jet_space_id space_id = trap_space(from_user);
    uint64_t fault_addr = read_sysreg(far_el1);
    const hw_slot_t* found = NULL;
    uint64_t found_distance = 0U;
    pok_bool_t handled = FALSE;

    for (uint32_t i = 0U; i < hw_watchpoints_n; i++)
    {
        const hw_slot_t* slot = &hw_watchpoints[i];

        // A watchpoint of this context: of the kernel or of the partition.
        if (slot->used && (slot->space_id == space_id))
        {
            uint64_t d = distance(fault_addr, slot->addr, slot->len);

            if ((found == NULL) || (d < found_distance))
            {
                found = slot;
                found_distance = d;
            }
        }
    }

    if (found != NULL)
    {
        handled = jet_gdb_trap(frame, hw_stop_reason(found->type),
            found->addr);
    }

    return handled;
}

/**
 * @funcname `ja_gdb_trap`
 *
 * @description
 * Handles a debug exception (from the exception handlers): brk, a software
 * step, a hardware breakpoint or a watchpoint.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 * | `ec` | `uint32_t` | [in] The exception class. |
 * | `from_user` | `pok_bool_t` | [in] Whether it came from user space. |
 *
 * @return TRUE if GDB handled it.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `handled` | `pok_bool_t` | Whether GDB handled it |
 *
 * @hlr
 * - \hlrid{HLR_GDB_ARM64_JA_GDB_TRAP_01}: `ja_gdb_trap()` shall handle a debug
 *   exception: a breakpoint instruction, a software step, a hardware breakpoint
 *   or a watchpoint, and tell whether the debugger handled it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *   - \srqref{SRQ_JET_DBG_006}
 *
 * @llr
 * - \llrid{LLR_GDB_ARM64_JA_GDB_TRAP_01}: If `ec` is `EC_BRK64`,
 *   `ja_gdb_trap()` shall return the result of `brk_trap()` for `frame` and
 *   `from_user`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_TRAP_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_TRAP_02}: If `ec` is `EC_STEP_LOWER` or
 *   `EC_STEP_CUR`, `ja_gdb_trap()` shall return the result of `step_trap()` for
 *   `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_TRAP_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_TRAP_03}: If `ec` is `EC_BREAKPOINT_LOWER` or
 *   `EC_BREAKPOINT_CUR`, `ja_gdb_trap()` shall return the result of
 *   `breakpoint_trap()` for `frame` and `from_user`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_TRAP_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_TRAP_04}: If `ec` is `EC_WATCHPOINT_LOWER` or
 *   `EC_WATCHPOINT_CUR`, `ja_gdb_trap()` shall return the result of
 *   `watchpoint_trap()` for `frame` and `from_user`.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_TRAP_01}
 * - \llrid{LLR_GDB_ARM64_JA_GDB_TRAP_05}: If `ec` is none of `EC_BRK64`,
 *   `EC_STEP_LOWER`, `EC_STEP_CUR`, `EC_BREAKPOINT_LOWER`, `EC_BREAKPOINT_CUR`,
 *   `EC_WATCHPOINT_LOWER` and `EC_WATCHPOINT_CUR`, `ja_gdb_trap()` shall return
 *   `FALSE` without calling a trap function.
 *   <br>Traces to:
 *   - \hlrref{HLR_GDB_ARM64_JA_GDB_TRAP_01}
 *
 * @design
 * 1. Depending on `ec`: \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_04},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_05}
 *    1. Case `EC_BRK64`: \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_01}
 *       1. Call `brk_trap()` with the arguments `frame` and `from_user`, and
 *          store the result in `handled`.
 *       2. Leave the switch.
 *    2. Case `EC_STEP_LOWER` or `EC_STEP_CUR`:
 *       \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_02}
 *       1. Call `step_trap()` with the argument `frame`, and store the result
 *          in `handled`.
 *       2. Leave the switch.
 *    3. Case `EC_BREAKPOINT_LOWER` or `EC_BREAKPOINT_CUR`:
 *       \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_03}
 *       1. Call `breakpoint_trap()` with the arguments `frame` and `from_user`,
 *          and store the result in `handled`.
 *       2. Leave the switch.
 *    4. Case `EC_WATCHPOINT_LOWER` or `EC_WATCHPOINT_CUR`:
 *       \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_04}
 *       1. Call `watchpoint_trap()` with the arguments `frame` and `from_user`,
 *          and store the result in `handled`.
 *       2. Leave the switch.
 *    5. Otherwise (default): \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_05}
 *       1. Set `handled` to `FALSE`.
 *       2. Leave the switch.
 * 2. Return `handled`. \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_01},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_02},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_03},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_04},
 *    \llrref{LLR_GDB_ARM64_JA_GDB_TRAP_05}
 *
 * @calls
 * - `brk_trap()`
 * - `step_trap()`
 * - `breakpoint_trap()`
 * - `watchpoint_trap()`
 */
pok_bool_t ja_gdb_trap(interrupt_frame* frame, uint32_t ec,
    pok_bool_t from_user)
{
    pok_bool_t handled;

    switch (ec)
    {
    case EC_BRK64:
        handled = brk_trap(frame, from_user);
        break;
    case EC_STEP_LOWER:
    case EC_STEP_CUR:
        handled = step_trap(frame);
        break;
    case EC_BREAKPOINT_LOWER:
    case EC_BREAKPOINT_CUR:
        handled = breakpoint_trap(frame, from_user);
        break;
    case EC_WATCHPOINT_LOWER:
    case EC_WATCHPOINT_CUR:
        handled = watchpoint_trap(frame, from_user);
        break;
    default:
        handled = FALSE;
        break;
    }

    return handled;
}

#endif /* POK_NEEDS_GDB */
