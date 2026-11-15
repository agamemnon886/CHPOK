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
 * comments name the new paths.
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
#include "sysreg.h"

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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `msr daifset, #8`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `msr daifclr, #8`.
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
 *
 * @llr
 *
 * @design
 * 1. Return `(read_sysreg(daif) & DAIF_D) != 0U`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `mdscr` to `read_sysreg(mdscr_el1)`.
 * 2. Use the macro `write_sysreg()` with the arguments `mdscr_el1` and
 *    `mdscr | bits`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `mdscr` to `read_sysreg(mdscr_el1)`.
 * 2. Use the macro `write_sysreg()` with the arguments `mdscr_el1` and
 *    `mdscr & ~bits`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `msr oslar_el1, xzr`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `dc cvau, %0`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `dsb ish; ic iallu; dsb ish; isb`.
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
 *
 * @llr
 *
 * @design
 * 1. Depending on `n`:
 *    1. Case `0U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr0_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr0_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr0_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    2. Case `1U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr1_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr1_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr1_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    3. Case `2U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr2_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr2_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr2_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    4. Case `3U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr3_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr3_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr3_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    5. Case `4U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr4_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr4_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr4_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    6. Case `5U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgbcr5_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgbvr5_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgbcr5_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    7. Otherwise (default):
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
 *
 * @llr
 *
 * @design
 * 1. Depending on `n`:
 *    1. Case `0U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgwcr0_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgwvr0_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgwcr0_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    2. Case `1U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgwcr1_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgwvr1_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgwcr1_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    3. Case `2U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgwcr2_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgwvr2_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgwcr2_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    4. Case `3U`:
 *       1. Use the macro `write_sysreg()` with the arguments `dbgwcr3_el1` and
 *          `0U`.
 *       2. Use the macro `write_sysreg()` with the arguments `dbgwvr3_el1` and
 *          `value`.
 *       3. Use the macro `write_sysreg()` with the arguments `dbgwcr3_el1` and
 *          `control`.
 *       4. Leave the switch.
 *    5. Otherwise (default):
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
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`):
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
 *
 * @llr
 *
 * @design
 * 1. Set `value` to `0U`.
 * 2. Set `i` to `0U`; then while `i` is less than `size`, repeat (after each
 *    pass: increment `i`):
 *    1. Set the bits `(uint64_t)in[offset + i] << (8U * i)` in `value`.
 * 3. Return `value`.
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
 *
 * @llr
 *
 * @design
 * 1. If `regno` is less than or equal to `REG_PC`:
 *    1. Set `size` to `8U`.
 * 2. Otherwise, if `regno` is equal to `REG_CPSR`:
 *    1. Set `size` to `4U`.
 * 3. Otherwise, if `regno` is less than or equal to `REG_V31`:
 *    1. Set `size` to `16U`.
 * 4. Otherwise, if `regno` is less than or equal to `REG_FPCR`:
 *    1. Set `size` to `4U`.
 * 5. Otherwise:
 *    1. Set `size` to `0U`.
 * 6. Return `size`.
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
 *
 * @llr
 *
 * @design
 * 1. Return `(ctx->spsr & SPSR_M_MASK) == 0U`.
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
 *
 * @llr
 *
 * @design
 * 1. If `regno` is less than `31U`:
 *    1. Set `value` to `frame->x[regno]`.
 * 2. Otherwise, if `regno` is equal to `REG_SP`:
 *    1. Set `value` to `ja_gdb_context_is_user(frame) ? frame->sp_el0 :`
 *       `((uint64_t)(uintptr_t)frame + sizeof(*frame))`.
 * 3. Otherwise, if `regno` is equal to `REG_PC`:
 *    1. Set `value` to `frame->elr`.
 * 4. Otherwise:
 *    1. Set `value` to `frame->spsr & ~SPSR_SS`.
 * 5. Return `value`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `written` to `TRUE`.
 * 2. If `regno` is less than `31U`:
 *    1. Set `frame->x[regno]` to `value`.
 * 3. Otherwise, if `regno` is equal to `REG_SP`:
 *    1. If `ja_gdb_context_is_user(frame)` is true:
 *       1. Set `frame->sp_el0` to `value`.
 *    2. Otherwise:
 *       1. Set `written` to `FALSE`.
 * 4. Otherwise, if `regno` is equal to `REG_PC`:
 *    1. If `(value % JA_GDB_BREAKPOINT_SIZE)` is equal to `0U`:
 *       1. Set `frame->elr` to `value`.
 *    2. Otherwise:
 *       1. Set `written` to `FALSE`.
 * 5. Otherwise:
 *    1. Set `frame->spsr` to
 *       `(frame->spsr & ~SPSR_NZCV) | (value & SPSR_NZCV)`.
 * 6. Return `written`.
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
 *
 * @llr
 *
 * @design
 * 1. If `regno` is less than or equal to `REG_V31`:
 *    1. Call `put_le()` with the arguments `fp->q[regno - REG_V0][0]`, `8U` and
 *       `value`.
 *    2. Call `put_le()` with the arguments `fp->q[regno - REG_V0][1]`, `8U` and
 *       the address of `value[8]`.
 * 2. Otherwise, if `regno` is equal to `REG_FPSR`:
 *    1. Call `put_le()` with the arguments `fp->fpsr`, `4U` and `value`.
 * 3. Otherwise:
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
 *
 * @llr
 *
 * @design
 * 1. If `regno` is less than or equal to `REG_V31`:
 *    1. Call `get_le()` with the arguments `value`, `0U` and `8U`, and store
 *       the result in `fp->q[regno - REG_V0][0]`.
 *    2. Call `get_le()` with the arguments `value`, `8U` and `8U`, and store
 *       the result in `fp->q[regno - REG_V0][1]`.
 * 2. Otherwise, if `regno` is equal to `REG_FPSR`:
 *    1. Set `fp->fpsr` to `get_le(value, 0U, 4U) & FPSR_MASK`.
 * 3. Otherwise:
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
 *
 * @llr
 *
 * @design
 * 1. Set `available` to `FALSE`.
 * 2. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *    result in `size`.
 * 3. If `regno` is less than or equal to `REG_CPSR`:
 *    1. If `ctx` is not NULL:
 *       1. Call `put_le()` with the arguments `frame_register_get(ctx, regno)`,
 *          `size` and `value`.
 *       2. Set `available` to `TRUE`.
 * 4. Otherwise, if `fp` is not NULL and `size` is not equal to `0U`:
 *    1. Call `fp_register_get()` with the arguments `fp`, `regno` and `value`.
 *    2. Set `available` to `TRUE`.
 * 5. Otherwise:
 *    1. Do nothing.
 * 6. Return `available`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `written` to `FALSE`.
 * 2. Call `ja_gdb_register_size()` with the argument `regno`, and store the
 *    result in `size`.
 * 3. If `regno` is less than or equal to `REG_CPSR`:
 *    1. If `ctx` is not NULL:
 *       1. Call `frame_register_set()` with the arguments `ctx`, `regno` and
 *          `get_le(value, 0U, size)`, and store the result in `written`.
 * 4. Otherwise, if `fp` is not NULL and `size` is not equal to `0U`:
 *    1. Call `fp_register_set()` with the arguments `fp`, `regno` and `value`.
 *    2. Set `written` to `TRUE`.
 * 5. Otherwise:
 *    1. Do nothing.
 * 6. Return `written`.
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
 *
 * @llr
 *
 * @design
 * 1. Return `ctx->elr`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `ctx->elr` to `pc`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `ctr` to `read_sysreg(ctr_el0)`.
 * 2. Set `line` to `(uintptr_t)4U << ((ctr >> 16) & 0xFU)`.
 * 3. Set `end` to `(uintptr_t)addr + size`.
 * 4. Set `p` to `(uintptr_t)addr & ~(line - 1U)`; then while `p` is less than
 *    `end`, repeat (after each pass: add `line` to `p`):
 *    1. Call `cpu_dcache_clean()` with the argument `p`.
 * 5. Call `cpu_icache_invalidate()`.
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
 *
 * @llr
 *
 * @design
 * 1. If `enable` is true:
 *    1. Set the bits `SPSR_SS` in `ctx->spsr`.
 *    2. Set `step_state` to `STEP_REQUESTED`.
 *    3. Set `step_frame` to `ctx`.
 * 2. Otherwise:
 *    1. Keep only the bits `~SPSR_SS` of `ctx->spsr`.
 *    2. If `step_frame` is equal to `ctx`:
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
 *
 * @llr
 *
 * @design
 * 1. Call `ja_addr_to_gdb_ro()` with the arguments `ctx->elr`, `4U` and `0U`,
 *    and store the result in `code`.
 * 2. Set `changes` to `TRUE`.
 * 3. If `code` is not NULL:
 *    1. Set `insn` to `(uint32_t)get_le(code, 0U, 4U)`.
 *    2. Set `changes` to
 *       `(insn == INSN_WFI) || (insn == INSN_WFE) || (insn == INSN_ERET) ||`
 *       `((insn & INSN_MSR_DAIFIMM_MASK) == INSN_MSR_DAIFIMM) || ((insn &`
 *       `INSN_MSR_DAIF_MASK) == INSN_MSR_DAIF)`.
 * 4. Return `changes`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `set` to `0U`.
 * 2. Set `clear` to `0U`.
 * 3. If `ja_gdb_context_is_user(frame)` is true:
 *    1. Set `set` to `SPSR_I`.
 * 4. Otherwise:
 *    1. Set `clear` to `SPSR_D`.
 *    2. If `insn_changes_mask(frame)` is false:
 *       1. Set `set` to `SPSR_I`.
 * 5. Set `step_changed` to `(set & ~frame->spsr) | (clear & frame->spsr)`.
 * 6. Set `step_saved` to `frame->spsr & step_changed`.
 * 7. Set `frame->spsr` to `(frame->spsr | set) & ~clear`.
 * 8. If `first` is true:
 *    1. Set the bits `SPSR_SS` in `frame->spsr`.
 * 9. Otherwise:
 *    1. Keep only the bits `~SPSR_SS` of `frame->spsr`.
 * 10. Call `cpu_mdscr_set()` with the argument `MDSCR_SS`.
 * 11. Set `step_state` to `STEP_RUNNING`.
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
 *
 * @llr
 *
 * @design
 * 1. If `step_state` is equal to `STEP_RUNNING`:
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
 *
 * @llr
 *
 * @design
 * 1. If `step_state` is equal to `STEP_RETURNING` and `(uintptr_t)step_frame`
 *    is equal to `(stack_kernel - sizeof(interrupt_frame))`:
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
 *
 * @llr
 *
 * @design
 * 1. Call `cpu_debug_unmask()`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `cpu_debug_mask()`.
 * 2. If (`step_state` is equal to `STEP_REQUESTED` or `step_state` is equal to
 *    `STEP_RETURNING`) and `frame` is equal to `step_frame`:
 *    1. Call `step_window_open()` with the arguments `frame` and
 *       `step_state == STEP_REQUESTED`.
 * 3. Otherwise:
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
 *
 * @llr
 *
 * @design
 * 1. Set `dfr0` to `read_sysreg(id_aa64dfr0_el1)`.
 * 2. Set `breakpoints` to `(uint32_t)((dfr0 >> 12) & 0xFU) + 1U`.
 * 3. Set `watchpoints` to `(uint32_t)((dfr0 >> 20) & 0xFU) + 1U`.
 * 4. Set `hw_breakpoints_n` to `breakpoints` if `breakpoints` is less than
 *    `JA_GDB_HW_BREAKPOINTS`, otherwise `JA_GDB_HW_BREAKPOINTS`.
 * 5. Set `hw_watchpoints_n` to `watchpoints` if `watchpoints` is less than
 *    `JA_GDB_HW_WATCHPOINTS`, otherwise `JA_GDB_HW_WATCHPOINTS`.
 * 6. Use the macro `write_sysreg()` with the arguments `mdscr_el1` and
 *    `MDSCR_KDE`.
 * 7. Set `i` to `0U`; then while `i` is less than `hw_breakpoints_n`, repeat
 *    (after each pass: increment `i`):
 *    1. Call `hw_breakpoint_write()` with the arguments `i`, `0U` and `0U`.
 * 8. Set `i` to `0U`; then while `i` is less than `hw_watchpoints_n`, repeat
 *    (after each pass: increment `i`):
 *    1. Call `hw_watchpoint_write()` with the arguments `i`, `0U` and `0U`.
 * 9. Call `cpu_os_lock_clear()`.
 * 10. Call `isb()`.
 * 11. Call `cpu_debug_unmask()`.
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
 *
 * @llr
 *
 * @design
 * 1. If `type` is equal to `JA_GDB_HW_EXEC`:
 *    1. Set `slots` to `hw_breakpoints`.
 *    2. Set `*count` to `hw_breakpoints_n`.
 * 2. Otherwise:
 *    1. Set `slots` to `hw_watchpoints`.
 *    2. Set `*count` to `hw_watchpoints_n`.
 * 3. Return `slots`.
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
 *
 * @llr
 *
 * @design
 * 1. If `type` is equal to `JA_GDB_HW_EXEC`:
 *    1. Set `supported` to
 *       `(len == JA_GDB_BREAKPOINT_SIZE) && ((addr % JA_GDB_BREAKPOINT_SIZE)`
 *       `== 0U)`.
 * 2. Otherwise:
 *    1. Set `supported` to
 *       `((len == 1U) || (len == 2U) || (len == 4U) || (len == 8U)) && ((addr`
 *       `% len) == 0U)`.
 * 3. Return `supported`.
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
 *
 * @llr
 *
 * @design
 * 1. Return
 *    `slot->used && (slot->type == type) && (slot->addr == addr) && (slot->len`
 *    `== len) && (slot->space_id == space_id)`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `inserted` to `FALSE`.
 * 2. Call `hw_slots_of()` with the arguments `type` and the address of `count`,
 *    and store the result in `slots`.
 * 3. If `hw_area_supported(type, addr, len)` is true:
 *    1. Set `i` to `0U`; then while `i` is less than `count`, repeat (after
 *       each pass: increment `i`):
 *       1. If `hw_slot_is(&slots[i], type, addr, len, space_id)` is true:
 *          1. Set `inserted` to `TRUE`.
 *    2. Set `i` to `0U`; then while `i` is less than `count` and `inserted` is
 *       false, repeat (after each pass: increment `i`):
 *       1. If `slots[i].used` is false:
 *          1. Set `slots[i].used` to `TRUE`.
 *          2. Set `slots[i].type` to `type`.
 *          3. Set `slots[i].addr` to `addr`.
 *          4. Set `slots[i].len` to `len`.
 *          5. Set `slots[i].space_id` to `space_id`.
 *          6. Set `inserted` to `TRUE`.
 * 4. Return `inserted`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `removed` to `FALSE`.
 * 2. Call `hw_slots_of()` with the arguments `type` and the address of `count`,
 *    and store the result in `slots`.
 * 3. Set `i` to `0U`; then while `i` is less than `count`, repeat (after each
 *    pass: increment `i`):
 *    1. If `hw_slot_is(&slots[i], type, addr, len, space_id)` is true:
 *       1. Set `slots[i].used` to `FALSE`.
 *       2. Set `removed` to `TRUE`.
 * 4. Return `removed`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_BREAKPOINTS`,
 *    repeat (after each pass: increment `i`):
 *    1. Set `hw_breakpoints[i].used` to `FALSE`.
 * 2. Set `i` to `0U`; then while `i` is less than `JA_GDB_HW_WATCHPOINTS`,
 *    repeat (after each pass: increment `i`):
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
 *
 * @llr
 *
 * @design
 * 1. Return
 *    `slot->used && ((slot->space_id == 0U) || (slot->space_id == space_id))`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `pmc` to `BCR_PMC_EL1` if `slot->space_id` is equal to `0U`, otherwise
 *    `BCR_PMC_EL0`.
 * 2. Return `BCR_E | pmc | BCR_BAS_ALL`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `pac` to `WCR_PAC_EL1` if `slot->space_id` is equal to `0U`, otherwise
 *    `WCR_PAC_EL0`.
 * 2. Set `bytes` to `((uint64_t)1U << slot->len) - 1U`.
 * 3. Depending on `slot->type`:
 *    1. Case `JA_GDB_HW_WRITE`:
 *       1. Set `lsc` to `WCR_LSC_STORE`.
 *       2. Leave the switch.
 *    2. Case `JA_GDB_HW_READ`:
 *       1. Set `lsc` to `WCR_LSC_LOAD`.
 *       2. Leave the switch.
 *    3. Otherwise (default):
 *       1. Set `lsc` to `WCR_LSC_STORE | WCR_LSC_LOAD`.
 *       2. Leave the switch.
 * 4. Return
 *    `WCR_E | pac | lsc | ((bytes << (slot->addr & 7U)) << WCR_BAS_SHIFT)`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `any` to `FALSE`.
 * 2. Set `i` to `0U`; then while `i` is less than `hw_breakpoints_n`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `any` to `any || hw_breakpoints[i].used`.
 * 3. Set `i` to `0U`; then while `i` is less than `hw_watchpoints_n`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `any` to `any || hw_watchpoints[i].used`.
 * 4. Return `any`.
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
 *
 * @llr
 *
 * @design
 * 1. If `hw_enabled` is true or `hw_any_used()` is true:
 *    1. Call `cpu_debug_masked()`, and store the result in `was_masked`.
 *    2. Set `enabled` to `FALSE`.
 *    3. Call `cpu_debug_mask()`.
 *    4. Set `i` to `0U`; then while `i` is less than `hw_breakpoints_n`, repeat
 *       (after each pass: increment `i`):
 *       1. Set `slot` to the address of `hw_breakpoints[i]`.
 *       2. Call `hw_slot_enabled()` with the arguments `slot` and `space_id`,
 *          and store the result in `on`.
 *       3. Call `hw_breakpoint_write()` with the arguments `i`, `slot->addr`
 *          and `hw_breakpoint_control(slot)` if `on` is true, otherwise `0U`.
 *       4. Set `enabled` to `enabled || on`.
 *    5. Set `i` to `0U`; then while `i` is less than `hw_watchpoints_n`, repeat
 *       (after each pass: increment `i`):
 *       1. Set `slot` to the address of `hw_watchpoints[i]`.
 *       2. Call `hw_slot_enabled()` with the arguments `slot` and `space_id`,
 *          and store the result in `on`.
 *       3. Call `hw_watchpoint_write()` with the arguments `i`,
 *          `slot->addr & ~(uint64_t)7U` and `hw_watchpoint_control(slot)` if
 *          `on` is true, otherwise `0U`.
 *       4. Set `enabled` to `enabled || on`.
 *    6. If `enabled` is true:
 *       1. Call `cpu_mdscr_set()` with the argument `MDSCR_MDE`.
 *    7. Otherwise:
 *       1. Call `cpu_mdscr_clear()` with the argument `MDSCR_MDE`.
 *    8. Call `isb()`.
 *    9. Set `hw_enabled` to `enabled`.
 *    10. If `was_masked` is false:
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
 *
 * @llr
 *
 * @design
 * 1. Do nothing.
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
 *
 * @llr
 *
 * @design
 * 1. Call `ja_gdb_hw_program()` with the argument `ja_space_get_current()`.
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
 *
 * @llr
 *
 * @design
 * 1. Depending on `type`:
 *    1. Case `JA_GDB_HW_EXEC`:
 *       1. Set `reason` to `JET_GDB_STOP_HW_BREAKPOINT`.
 *       2. Leave the switch.
 *    2. Case `JA_GDB_HW_WRITE`:
 *       1. Set `reason` to `JET_GDB_STOP_WATCH`.
 *       2. Leave the switch.
 *    3. Case `JA_GDB_HW_READ`:
 *       1. Set `reason` to `JET_GDB_STOP_RWATCH`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Set `reason` to `JET_GDB_STOP_AWATCH`.
 *       2. Leave the switch.
 * 2. Return `reason`.
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
 *
 * @llr
 *
 * @design
 * 1. Return `(uintptr_t)&pok_trap`.
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
 *
 * @llr
 *
 * @design
 * 1. Return `ja_space_get_current()` if `from_user` is true, otherwise `0U`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `pc` to `frame->elr`.
 * 2. If `from_user` is false and `pc` is equal to `pok_trap_addr()`:
 *    1. Set `frame->elr` to `pc + JA_GDB_BREAKPOINT_SIZE`.
 *    2. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `JET_GDB_STOP_REQUEST` and `pc`, and store the result in `handled`.
 * 3. Otherwise, if `jet_gdb_is_breakpoint(trap_space(from_user), pc)` is true:
 *    1. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `JET_GDB_STOP_SW_BREAKPOINT` and `pc`, and store the result in
 *       `handled`.
 * 4. Otherwise:
 *    1. Set `frame->elr` to `pc + JA_GDB_BREAKPOINT_SIZE`.
 *    2. Call `jet_gdb_trap()` with the arguments `frame`, `JET_GDB_STOP_TRAP`
 *       and `pc`, and store the result in `handled`.
 *    3. If `handled` is false:
 *       1. Set `frame->elr` to `pc`.
 * 5. Return `handled`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `step_state` to `STEP_NONE`.
 * 2. Keep only the bits `~SPSR_SS` of `frame->spsr`.
 * 3. Call `jet_gdb_trap()` with the arguments `frame`, `JET_GDB_STOP_STEP` and
 *    `frame->elr`, and return its result.
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
 *
 * @llr
 *
 * @design
 * 1. Call `trap_space()` with the argument `from_user`, and store the result in
 *    `space_id`.
 * 2. Set `handled` to `FALSE`.
 * 3. Set `i` to `0U`; then while `i` is less than `hw_breakpoints_n` and
 *    `handled` is false, repeat (after each pass: increment `i`):
 *    1. Set `slot` to the address of `hw_breakpoints[i]`.
 *    2. If `slot->used` is true and `slot->space_id` is equal to `space_id` and
 *       `slot->addr` is equal to `frame->elr`:
 *       1. Call `jet_gdb_trap()` with the arguments `frame`,
 *          `JET_GDB_STOP_HW_BREAKPOINT` and `slot->addr`, and store the result
 *          in `handled`.
 * 4. Return `handled`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `result` to `0U`.
 * 2. If `addr` is less than `start`:
 *    1. Set `result` to `start - addr`.
 * 3. Otherwise, if `addr` is greater than or equal to `(start + len)`:
 *    1. Set `result` to `addr - (start + len) + 1U`.
 * 4. Otherwise:
 *    1. Do nothing.
 * 5. Return `result`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `trap_space()` with the argument `from_user`, and store the result in
 *    `space_id`.
 * 2. Set `fault_addr` to `read_sysreg(far_el1)`.
 * 3. Set `found` to `NULL`.
 * 4. Set `found_distance` to `0U`.
 * 5. Set `handled` to `FALSE`.
 * 6. Set `i` to `0U`; then while `i` is less than `hw_watchpoints_n`, repeat
 *    (after each pass: increment `i`):
 *    1. Set `slot` to the address of `hw_watchpoints[i]`.
 *    2. If `slot->used` is true and `slot->space_id` is equal to `space_id`:
 *       1. Call `distance()` with the arguments `fault_addr`, `slot->addr` and
 *          `slot->len`, and store the result in `d`.
 *       2. If `found` is NULL or `d` is less than `found_distance`:
 *          1. Set `found` to `slot`.
 *          2. Set `found_distance` to `d`.
 * 7. If `found` is not NULL:
 *    1. Call `jet_gdb_trap()` with the arguments `frame`,
 *       `hw_stop_reason(found->type)` and `found->addr`, and store the result
 *       in `handled`.
 * 8. Return `handled`.
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
 *
 * @llr
 *
 * @design
 * 1. Depending on `ec`:
 *    1. Case `EC_BRK64`:
 *       1. Call `brk_trap()` with the arguments `frame` and `from_user`, and
 *          store the result in `handled`.
 *       2. Leave the switch.
 *    2. Case `EC_STEP_LOWER` or `EC_STEP_CUR`:
 *       1. Call `step_trap()` with the argument `frame`, and store the result
 *          in `handled`.
 *       2. Leave the switch.
 *    3. Case `EC_BREAKPOINT_LOWER` or `EC_BREAKPOINT_CUR`:
 *       1. Call `breakpoint_trap()` with the arguments `frame` and `from_user`,
 *          and store the result in `handled`.
 *       2. Leave the switch.
 *    4. Case `EC_WATCHPOINT_LOWER` or `EC_WATCHPOINT_CUR`:
 *       1. Call `watchpoint_trap()` with the arguments `frame` and `from_user`,
 *          and store the result in `handled`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Set `handled` to `FALSE`.
 *       2. Leave the switch.
 * 2. Return `handled`.
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
