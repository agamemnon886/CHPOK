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
 */

/*
 * Architecture part of the GDB server (kernel/libc/gdbserver.c) on
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

/* Exception classes (ESR_EL1.EC) of debug exceptions. */
#define EC_BREAKPOINT_LOWER 0x30U
#define EC_BREAKPOINT_CUR   0x31U
#define EC_STEP_LOWER       0x32U
#define EC_STEP_CUR         0x33U
#define EC_WATCHPOINT_LOWER 0x34U
#define EC_WATCHPOINT_CUR   0x35U
#define EC_BRK64            0x3CU

/* SPSR_EL1: debug exceptions masked, IRQs masked, software step. */
#define SPSR_D       (1ULL << 9)
#define SPSR_I       (1ULL << 7)
#define SPSR_SS      (1ULL << 21)
/* SPSR_EL1.M[4:0] is 0 for EL0 (AArch64). */
#define SPSR_M_MASK  0x1FULL
/* The flags GDB may change: N, Z, C and V. */
#define SPSR_NZCV    0xF0000000ULL

/* DAIF: debug exceptions masked. */
#define DAIF_D       (1ULL << 9)

/* MDSCR_EL1: software step, kernel debug, debug exceptions enabled. */
#define MDSCR_SS     (1ULL << 0)
#define MDSCR_KDE    (1ULL << 13)
#define MDSCR_MDE    (1ULL << 15)

/* DBGBCR<n>_EL1: enable; EL1 or EL0 (PMC); all 4 bytes of the address. */
#define BCR_E        1ULL
#define BCR_PMC_EL1  (1ULL << 1)
#define BCR_PMC_EL0  (2ULL << 1)
#define BCR_BAS_ALL  (0xFULL << 5)

/* DBGWCR<n>_EL1: enable; EL1 or EL0 (PAC); load or store (LSC); bytes. */
#define WCR_E        1ULL
#define WCR_PAC_EL1  (1ULL << 1)
#define WCR_PAC_EL0  (2ULL << 1)
#define WCR_LSC_LOAD  (1ULL << 3)
#define WCR_LSC_STORE (2ULL << 3)
#define WCR_BAS_SHIFT 5U

/* GDB's numbers of the registers (see arch/gdb.h). */
#define REG_SP     31U
#define REG_PC     32U
#define REG_CPSR   33U
#define REG_V0     34U
#define REG_V31    65U
#define REG_FPSR   66U
#define REG_FPCR   67U

/* FPSR and FPCR bits which are defined (the others are not written). */
#define FPSR_MASK  0xF800009FULL
#define FPCR_MASK  0x07FF9F00ULL

/* Instructions which change the interrupt mask or wait for interrupts. */
#define INSN_WFI         0xD503207FU
#define INSN_WFE         0xD503205FU
#define INSN_ERET        0xD69F03E0U
/* msr daifset/daifclr, #imm: the op2 field differs in bit 5. */
#define INSN_MSR_DAIFIMM 0xD50340DFU
#define INSN_MSR_DAIFIMM_MASK 0xFFFFF0DFU
/* msr daif, xN. */
#define INSN_MSR_DAIF    0xD51B4220U
#define INSN_MSR_DAIF_MASK 0xFFFFFFE0U

/* The breakpoint instruction: brk #0. */
const uint8_t ja_gdb_breakpoint_insn[JA_GDB_BREAKPOINT_SIZE] = {
    0x00U, 0x00U, 0x20U, 0xD4U
};

static void cpu_debug_mask(void)
{
    asm volatile ("msr daifset, #8" ::: "memory");
}

static void cpu_debug_unmask(void)
{
    asm volatile ("msr daifclr, #8" ::: "memory");
}

static pok_bool_t cpu_debug_masked(void)
{
    return (read_sysreg(daif) & DAIF_D) != 0U;
}

/* Set or clear bits of MDSCR_EL1. */
static void cpu_mdscr_set(uint64_t bits)
{
    uint64_t mdscr = read_sysreg(mdscr_el1);

    write_sysreg(mdscr_el1, mdscr | bits);
}

static void cpu_mdscr_clear(uint64_t bits)
{
    uint64_t mdscr = read_sysreg(mdscr_el1);

    write_sysreg(mdscr_el1, mdscr & ~bits);
}

/* The OS lock, which is set at reset, blocks the debug exceptions. */
static void cpu_os_lock_clear(void)
{
    asm volatile ("msr oslar_el1, xzr" ::: "memory");
}

/* Clean the data cache line of 'addr' to the point of unification. */
static void cpu_dcache_clean(uintptr_t addr)
{
    asm volatile ("dc cvau, %0" : : "r" (addr) : "memory");
}

/* Invalidate the instruction cache, after the data caches are clean. */
static void cpu_icache_invalidate(void)
{
    asm volatile ("dsb ish\n ic iallu\n dsb ish\n isb" ::: "memory");
}

/* Write DBGBVR<n>_EL1 and DBGBCR<n>_EL1 (the latter is 0 to disable). */
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

/* Write DBGWVR<n>_EL1 and DBGWCR<n>_EL1 (the latter is 0 to disable). */
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

/* Store the 'size' low bytes of 'value' at 'out' (little-endian). */
static void put_le(uint64_t value, size_t size, uint8_t* out)
{
    for (size_t i = 0U; i < size; i++)
    {
        out[i] = (uint8_t)(value >> (8U * i));
    }
}

/* Load 'size' bytes at 'in' + 'offset' (little-endian). */
static uint64_t get_le(const uint8_t* in, size_t offset, size_t size)
{
    uint64_t value = 0U;

    for (size_t i = 0U; i < size; i++)
    {
        value |= (uint64_t)in[offset + i] << (8U * i);
    }

    return value;
}

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

pok_bool_t ja_gdb_context_is_user(const struct jet_interrupt_context* ctx)
{
    return (ctx->spsr & SPSR_M_MASK) == 0U;
}

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

/*
 * Set integer register 'regno' (up to cpsr) of the frame. Returns FALSE if
 * it can't be set. Only the condition flags of cpsr are changed, so GDB
 * can't change the exception level or unmask interrupts.
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

/* Read FP/SIMD register 'regno' (v0 and above). */
static void fp_register_get(const struct jet_fp_store* fp, uint32_t regno,
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

/* Write FP/SIMD register 'regno' (v0 and above). */
static void fp_register_set(struct jet_fp_store* fp, uint32_t regno,
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

pok_bool_t ja_gdb_register_get(const struct jet_interrupt_context* ctx,
    const struct jet_fp_store* fp, uint32_t regno, uint8_t* value)
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

pok_bool_t ja_gdb_register_set(struct jet_interrupt_context* ctx,
    struct jet_fp_store* fp, uint32_t regno, const uint8_t* value)
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

uintptr_t ja_gdb_pc_get(const struct jet_interrupt_context* ctx)
{
    return ctx->elr;
}

void ja_gdb_pc_set(struct jet_interrupt_context* ctx, uintptr_t pc)
{
    ctx->elr = pc;
}

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

/*
 * Single step (see the description at the beginning of the file).
 *
 *  - NONE: no step.
 *  - REQUESTED: GDB has asked for a step of the context 'step_frame'; it
 *    starts when the exception handler returns to it.
 *  - RUNNING: the step has started. The next exception ends the window.
 *  - RETURNING: an exception other than the step has come in the window
 *    (the instruction was a system call, or an IRQ has come); the step
 *    ends when the handler returns to the context 'step_frame'.
 */
enum gdb_step_state
{
    STEP_NONE,
    STEP_REQUESTED,
    STEP_RUNNING,
    STEP_RETURNING
};

static enum gdb_step_state step_state = STEP_NONE;
static const interrupt_frame* step_frame = NULL;
/* Bits of SPSR_EL1 changed for the stepped instruction, and their values. */
static uint64_t step_changed = 0U;
static uint64_t step_saved = 0U;

void ja_gdb_single_step(struct jet_interrupt_context* ctx,
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

/*
 * Whether the instruction at the PC of kernel context 'ctx' changes the
 * interrupt mask or waits for an interrupt (wfi, wfe, msr daif*, eret). If
 * it can't be read, assume it does.
 *
 * MISRA C:2025 Rule 11.5 deviation: ja_addr_to_gdb_ro() returns the
 * kernel address of the code as a pointer to void.
 */
static pok_bool_t insn_changes_mask(const struct jet_interrupt_context* ctx)
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

/*
 * Start the step window for the return to 'frame': IRQs are masked for
 * the stepped instruction (unless it changes the mask itself), and in the
 * kernel debug exceptions are enabled. 'first' is whether the instruction
 * is to be executed (it isn't if it has been executed, and the step is
 * pending).
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

void ja_gdb_exception_unmask(void)
{
    cpu_debug_unmask();
}

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

/* Hardware breakpoint or watchpoint in a debug register. */
struct hw_slot
{
    pok_bool_t used;
    enum ja_gdb_hw_type type;
    uintptr_t addr;
    size_t len;
    jet_space_id space_id;
};

static struct hw_slot hw_breakpoints[JA_GDB_HW_BREAKPOINTS];
static struct hw_slot hw_watchpoints[JA_GDB_HW_WATCHPOINTS];
/* Numbers of the breakpoint and watchpoint registers of the processor. */
static uint32_t hw_breakpoints_n = 0U;
static uint32_t hw_watchpoints_n = 0U;

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

static struct hw_slot* hw_slots_of(enum ja_gdb_hw_type type, uint32_t* count)
{
    struct hw_slot* slots;

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

/* Whether the debug registers can watch 'len' bytes at 'addr'. */
static pok_bool_t hw_area_supported(enum ja_gdb_hw_type type,
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

static pok_bool_t hw_slot_is(const struct hw_slot* slot,
    enum ja_gdb_hw_type type, uintptr_t addr, size_t len,
    jet_space_id space_id)
{
    return slot->used && (slot->type == type) && (slot->addr == addr)
        && (slot->len == len) && (slot->space_id == space_id);
}

pok_bool_t ja_gdb_hw_insert(enum ja_gdb_hw_type type, uintptr_t addr,
    size_t len, jet_space_id space_id)
{
    pok_bool_t inserted = FALSE;
    uint32_t count;
    struct hw_slot* slots = hw_slots_of(type, &count);

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

pok_bool_t ja_gdb_hw_remove(enum ja_gdb_hw_type type, uintptr_t addr,
    size_t len, jet_space_id space_id)
{
    pok_bool_t removed = FALSE;
    uint32_t count;
    struct hw_slot* slots = hw_slots_of(type, &count);

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

/* Whether the slot is enabled for the kernel and the space 'space_id'. */
static pok_bool_t hw_slot_enabled(const struct hw_slot* slot,
    jet_space_id space_id)
{
    return slot->used
        && ((slot->space_id == 0U) || (slot->space_id == space_id));
}

static uint64_t hw_breakpoint_control(const struct hw_slot* slot)
{
    uint64_t pmc = (slot->space_id == 0U) ? BCR_PMC_EL1 : BCR_PMC_EL0;

    return BCR_E | pmc | BCR_BAS_ALL;
}

static uint64_t hw_watchpoint_control(const struct hw_slot* slot)
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
            const struct hw_slot* slot = &hw_breakpoints[i];
            pok_bool_t on = hw_slot_enabled(slot, space_id);

            hw_breakpoint_write(i, slot->addr,
                on ? hw_breakpoint_control(slot) : 0U);
            enabled = enabled || on;
        }

        for (uint32_t i = 0U; i < hw_watchpoints_n; i++)
        {
            const struct hw_slot* slot = &hw_watchpoints[i];
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

void ja_gdb_hw_enter(void)
{
    // The GDB server runs with debug exceptions masked.
}

void ja_gdb_hw_leave(void)
{
    ja_gdb_hw_program(ja_space_get_current());
}

/* Reason of a stop at a hardware watchpoint. */
static enum jet_gdb_stop_reason hw_stop_reason(enum ja_gdb_hw_type type)
{
    enum jet_gdb_stop_reason reason;

    switch (type)
    {
    case JA_GDB_HW_EXEC:  reason = JET_GDB_STOP_HW_BREAKPOINT; break;
    case JA_GDB_HW_WRITE: reason = JET_GDB_STOP_WATCH; break;
    case JA_GDB_HW_READ:  reason = JET_GDB_STOP_RWATCH; break;
    default:              reason = JET_GDB_STOP_AWATCH; break;
    }

    return reason;
}

/*
 * MISRA C:2025 Rule 11.1 deviation: the address of pok_trap() is compared
 * with the address of the breakpoint as an integer.
 */
static uintptr_t pok_trap_addr(void)
{
    return (uintptr_t)&pok_trap;
}

/* Space of the code which has trapped. */
static jet_space_id trap_space(pok_bool_t from_user)
{
    return from_user ? ja_space_get_current() : 0U;
}

/* Breakpoint instruction: ELR is at the instruction. */
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

/* Software step exception: the step is done. */
static pok_bool_t step_trap(interrupt_frame* frame)
{
    step_state = STEP_NONE;
    frame->spsr &= ~SPSR_SS;

    return jet_gdb_trap(frame, JET_GDB_STOP_STEP, frame->elr);
}

/* Hardware breakpoint: the exception comes before the instruction. */
static pok_bool_t breakpoint_trap(interrupt_frame* frame,
    pok_bool_t from_user)
{
    jet_space_id space_id = trap_space(from_user);
    pok_bool_t handled = FALSE;

    for (uint32_t i = 0U; (i < hw_breakpoints_n) && !handled; i++)
    {
        const struct hw_slot* slot = &hw_breakpoints[i];

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

/* Distance of 'addr' from the area [start, start + len) (0 if inside). */
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

/*
 * Hardware watchpoint: the exception comes before the access, and the
 * processor doesn't tell which watchpoint has matched: it's the one which
 * is the closest to the address of the access.
 */
static pok_bool_t watchpoint_trap(interrupt_frame* frame,
    pok_bool_t from_user)
{
    jet_space_id space_id = trap_space(from_user);
    uint64_t fault_addr = read_sysreg(far_el1);
    const struct hw_slot* found = NULL;
    uint64_t found_distance = 0U;
    pok_bool_t handled = FALSE;

    for (uint32_t i = 0U; i < hw_watchpoints_n; i++)
    {
        const struct hw_slot* slot = &hw_watchpoints[i];

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
