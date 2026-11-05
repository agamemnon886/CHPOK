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
 * Modified by Kirill Brazhnikov, 2026-11-04: ported from
 * kernel/arch/x86/gdb.c (removed with the 32-bit x86 port) to x86_64:
 * GDB's amd64 registers, including x87 and SSE ones from the FXSAVE area,
 * are read and written one at a time; breakpoint and debug exceptions
 * are passed to the GDB server (as kernel/arch/x86/interrupt.c did);
 * single-stepping and hardware breakpoints and watchpoints use the trap
 * flag and the debug registers; interrupts stay disabled for the stepped
 * instruction, so that no other thread runs before the step is done.
 */

/*
 * Architecture part of the GDB server (kernel/libc/gdbserver.c) on
 * x86_64.
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

/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (the cpu_*() functions):
 * the debug registers are accessed with GNU inline assembly, each access
 * isolated in a small function.
 */

/* RFLAGS: trap flag (single step), interrupts enabled, resume flag. */
#define RFLAGS_TF 0x100U
#define RFLAGS_IF 0x200U
#define RFLAGS_RF 0x10000U
/* RFLAGS bits GDB may change: CF, PF, AF, ZF, SF, DF and OF. */
#define RFLAGS_GDB_WRITABLE 0xCD5U

/* DR6: hits of DR0-DR3, single step; value with no event recorded. */
#define DR6_HITS 0xFU
#define DR6_BS 0x4000U
#define DR6_CLEAR 0xFFFF0FF0U

/* DR7: local exact breakpoints (needed for data breakpoints on old CPUs). */
#define DR7_LE 0x100U

/* GDB's numbers of the registers (see arch/gdb.h). */
#define REG_RIP    16U
#define REG_EFLAGS 17U
#define REG_SS     19U
#define REG_ST0    24U
#define REG_FCTRL  32U
#define REG_FSTAT  33U
#define REG_FTAG   34U
#define REG_FISEG  35U
#define REG_FIOFF  36U
#define REG_FOSEG  37U
#define REG_FOOFF  38U
#define REG_FOP    39U
#define REG_XMM0   40U
#define REG_MXCSR  56U

/* Offsets in the FXSAVE (64-bit) area. */
#define FX_FCW        0U
#define FX_FSW        2U
#define FX_FTW        4U
#define FX_FOP        6U
#define FX_FIP        8U
#define FX_FDP        16U
#define FX_MXCSR      24U
#define FX_MXCSR_MASK 28U
#define FX_ST0        32U
#define FX_XMM0       160U
/* st0-st7 and xmm0-xmm15 take 16 bytes each in the area. */
#define FX_STRIDE     16U
#define ST_SIZE       10U
/* MXCSR bits which may be set if the processor reports no mask. */
#define MXCSR_MASK_DEFAULT 0xFFBFU

/* x87 tag values. */
#define FP_TAG_VALID   0U
#define FP_TAG_ZERO    1U
#define FP_TAG_SPECIAL 2U
#define FP_TAG_EMPTY   3U

/* The breakpoint instruction: int3. */
const uint8_t ja_gdb_breakpoint_insn[JA_GDB_BREAKPOINT_SIZE] = { 0xCCU };

static uint64_t cpu_read_dr6(void)
{
    uint64_t value;

    asm volatile ("mov %%dr6, %0" : "=r" (value));
    return value;
}

static void cpu_write_dr6(uint64_t value)
{
    asm volatile ("mov %0, %%dr6" : : "r" (value));
}

static void cpu_write_dr7(uint64_t value)
{
    asm volatile ("mov %0, %%dr7" : : "r" (value));
}

/* Write debug address register DR<index> (0-3). */
static void cpu_write_dr_addr(uint32_t index, uint64_t value)
{
    switch (index)
    {
    case 0U:
        asm volatile ("mov %0, %%dr0" : : "r" (value));
        break;
    case 1U:
        asm volatile ("mov %0, %%dr1" : : "r" (value));
        break;
    case 2U:
        asm volatile ("mov %0, %%dr2" : : "r" (value));
        break;
    case 3U:
        asm volatile ("mov %0, %%dr3" : : "r" (value));
        break;
    default:
        // There are only 4 address registers.
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

    if (regno <= REG_RIP) {
        size = 8U;
    } else if (regno < REG_ST0) {
        size = 4U;
    } else if (regno < REG_FCTRL) {
        size = ST_SIZE;
    } else if (regno < REG_XMM0) {
        size = 4U;
    } else if (regno < REG_MXCSR) {
        size = 16U;
    } else if (regno == REG_MXCSR) {
        size = 4U;
    } else {
        size = 0U;
    }

    return size;
}

/* Value of integer register 'regno' (up to ss) of the frame. */
static uint64_t frame_register_get(const interrupt_frame* frame,
    uint32_t regno)
{
    uint64_t value;

    switch (regno)
    {
    case 0U:  value = frame->rax; break;
    case 1U:  value = frame->rbx; break;
    case 2U:  value = frame->rcx; break;
    case 3U:  value = frame->rdx; break;
    case 4U:  value = frame->rsi; break;
    case 5U:  value = frame->rdi; break;
    case 6U:  value = frame->rbp; break;
    case 7U:  value = frame->rsp; break;
    case 8U:  value = frame->r8; break;
    case 9U:  value = frame->r9; break;
    case 10U: value = frame->r10; break;
    case 11U: value = frame->r11; break;
    case 12U: value = frame->r12; break;
    case 13U: value = frame->r13; break;
    case 14U: value = frame->r14; break;
    case 15U: value = frame->r15; break;
    case 16U: value = frame->rip; break;
    case 17U: value = frame->rflags; break;
    case 18U: value = frame->cs; break;
    case 19U: value = frame->ss; break;
    default:  value = 0U; break;
    }

    return value;
}

/*
 * Set integer register 'regno' (up to eflags) of the frame. Only the
 * status flags of eflags are changed.
 */
static void frame_register_set(interrupt_frame* frame, uint32_t regno,
    uint64_t value)
{
    switch (regno)
    {
    case 0U:  frame->rax = value; break;
    case 1U:  frame->rbx = value; break;
    case 2U:  frame->rcx = value; break;
    case 3U:  frame->rdx = value; break;
    case 4U:  frame->rsi = value; break;
    case 5U:  frame->rdi = value; break;
    case 6U:  frame->rbp = value; break;
    case 7U:  frame->rsp = value; break;
    case 8U:  frame->r8 = value; break;
    case 9U:  frame->r9 = value; break;
    case 10U: frame->r10 = value; break;
    case 11U: frame->r11 = value; break;
    case 12U: frame->r12 = value; break;
    case 13U: frame->r13 = value; break;
    case 14U: frame->r14 = value; break;
    case 15U: frame->r15 = value; break;
    case 16U: frame->rip = value; break;
    case 17U:
        frame->rflags = (frame->rflags & ~(uint64_t)RFLAGS_GDB_WRITABLE)
            | (value & RFLAGS_GDB_WRITABLE);
        break;
    default:
        // Segment registers define the privilege level: never changed.
        break;
    }
}

/* Tag of x87 register st<st> (a value in the FXSAVE area 'fx'). */
static uint32_t fp_tag(const uint8_t* fx, uint32_t st)
{
    uint32_t offset = FX_ST0 + (st * FX_STRIDE);
    uint32_t exponent = (((uint32_t)fx[offset + 9U] & 0x7FU) << 8)
        | (uint32_t)fx[offset + 8U];
    pok_bool_t integer = ((uint32_t)fx[offset + 7U] & 0x80U) != 0U;
    pok_bool_t fraction_zero = ((uint32_t)fx[offset + 7U] & 0x7FU) == 0U;
    uint32_t tag;

    for (uint32_t i = 0U; i < 7U; i++)
    {
        if (fx[offset + i] != 0U) {
            fraction_zero = FALSE;
        }
    }

    if (exponent == 0x7FFFU) {
        tag = FP_TAG_SPECIAL;
    } else if (exponent == 0U) {
        tag = (fraction_zero && !integer) ? FP_TAG_ZERO : FP_TAG_SPECIAL;
    } else {
        tag = integer ? FP_TAG_VALID : FP_TAG_SPECIAL;
    }

    return tag;
}

/*
 * Full x87 tag word from the abridged one in the FXSAVE area (as GDB
 * does for its native targets). Tags are indexed by physical register;
 * st0 is physical register TOP.
 */
static uint32_t fp_full_tag(const uint8_t* fx)
{
    uint32_t top = ((uint32_t)fx[FX_FSW + 1U] >> 3) & 7U;
    uint32_t abridged = fx[FX_FTW];
    uint32_t full = 0U;

    for (uint32_t phys = 0U; phys < 8U; phys++)
    {
        uint32_t tag = FP_TAG_EMPTY;

        if ((abridged & (1U << phys)) != 0U) {
            tag = fp_tag(fx, (phys + 8U - top) % 8U);
        }
        full |= tag << (2U * phys);
    }

    return full;
}

/* Abridged tag (1 bit per physical register) from a full tag word. */
static uint8_t fp_abridged_tag(uint32_t full)
{
    uint32_t abridged = 0U;

    for (uint32_t phys = 0U; phys < 8U; phys++)
    {
        uint32_t tag = full >> (2U * phys);

        if ((tag & 3U) != FP_TAG_EMPTY) {
            abridged |= 1U << phys;
        }
    }

    return (uint8_t)abridged;
}

/* Copy 'size' bytes from 'from' + 'from_offset' to 'to' + 'to_offset'. */
static void copy_bytes(uint8_t* to, size_t to_offset, const uint8_t* from,
    size_t from_offset, size_t size)
{
    for (size_t i = 0U; i < size; i++)
    {
        to[to_offset + i] = from[from_offset + i];
    }
}

/* Read floating point register 'regno' (st0 and above). */
static void fp_register_get(const struct jet_fp_store* fp, uint32_t regno,
    uint8_t* value)
{
    const uint8_t* fx = fp->data;

    if (regno < REG_FCTRL)
    {
        copy_bytes(value, 0U, fx, FX_ST0 + ((regno - REG_ST0) * FX_STRIDE),
            ST_SIZE);
    }
    else if ((regno >= REG_XMM0) && (regno < REG_MXCSR))
    {
        copy_bytes(value, 0U, fx, FX_XMM0 + ((regno - REG_XMM0) * FX_STRIDE),
            FX_STRIDE);
    }
    else
    {
        uint64_t v;

        switch (regno)
        {
        case REG_FCTRL: v = get_le(fx, FX_FCW, 2U); break;
        case REG_FSTAT: v = get_le(fx, FX_FSW, 2U); break;
        case REG_FTAG:  v = fp_full_tag(fx); break;
        case REG_FISEG: v = get_le(fx, FX_FIP + 4U, 4U); break;
        case REG_FIOFF: v = get_le(fx, FX_FIP, 4U); break;
        case REG_FOSEG: v = get_le(fx, FX_FDP + 4U, 4U); break;
        case REG_FOOFF: v = get_le(fx, FX_FDP, 4U); break;
        case REG_FOP:   v = get_le(fx, FX_FOP, 2U) & 0x7FFU; break;
        default:        v = get_le(fx, FX_MXCSR, 4U); break;
        }

        put_le(v, 4U, value);
    }
}

/* Write floating point register 'regno' (st0 and above). */
static void fp_register_set(struct jet_fp_store* fp, uint32_t regno,
    const uint8_t* value)
{
    uint8_t* fx = fp->data;

    if (regno < REG_FCTRL)
    {
        copy_bytes(fx, FX_ST0 + ((regno - REG_ST0) * FX_STRIDE), value, 0U,
            ST_SIZE);
    }
    else if ((regno >= REG_XMM0) && (regno < REG_MXCSR))
    {
        copy_bytes(fx, FX_XMM0 + ((regno - REG_XMM0) * FX_STRIDE), value, 0U,
            FX_STRIDE);
    }
    else
    {
        uint64_t v = get_le(value, 0U, 4U);
        uint64_t mask;

        switch (regno)
        {
        case REG_FCTRL: put_le(v, 2U, &fx[FX_FCW]); break;
        case REG_FSTAT: put_le(v, 2U, &fx[FX_FSW]); break;
        case REG_FTAG:
            fx[FX_FTW] = fp_abridged_tag((uint32_t)(v & 0xFFFFU));
            break;
        case REG_FISEG: put_le(v, 4U, &fx[FX_FIP + 4U]); break;
        case REG_FIOFF: put_le(v, 4U, &fx[FX_FIP]); break;
        case REG_FOSEG: put_le(v, 4U, &fx[FX_FDP + 4U]); break;
        case REG_FOOFF: put_le(v, 4U, &fx[FX_FDP]); break;
        case REG_FOP:   put_le(v & 0x7FFU, 2U, &fx[FX_FOP]); break;
        default:
            // Reserved MXCSR bits would make fxrstor fault.
            mask = get_le(fx, FX_MXCSR_MASK, 4U);
            if (mask == 0U) {
                mask = MXCSR_MASK_DEFAULT;
            }
            put_le(v & mask, 4U, &fx[FX_MXCSR]);
            break;
        }
    }
}

pok_bool_t ja_gdb_register_get(const struct jet_interrupt_context* ctx,
    const struct jet_fp_store* fp, uint32_t regno, uint8_t* value)
{
    pok_bool_t available = FALSE;
    size_t size = ja_gdb_register_size(regno);

    if (regno <= REG_SS)
    {
        if (ctx != NULL)
        {
            put_le(frame_register_get(ctx, regno), size, value);
            available = TRUE;
        }
    }
    else if (regno < REG_ST0)
    {
        // ds, es, fs and gs aren't saved on entry to the kernel.
    }
    else if ((fp != NULL) && (size != 0U))
    {
        fp_register_get(fp, regno, value);
        available = TRUE;
    }
    else
    {
        // No floating point registers, or no such register.
    }

    return available;
}

pok_bool_t ja_gdb_register_set(struct jet_interrupt_context* ctx,
    struct jet_fp_store* fp, uint32_t regno, const uint8_t* value)
{
    pok_bool_t written = FALSE;
    size_t size = ja_gdb_register_size(regno);

    if (regno <= REG_EFLAGS)
    {
        if (ctx != NULL)
        {
            frame_register_set(ctx, regno, get_le(value, 0U, size));
            written = TRUE;
        }
    }
    else if (regno < REG_ST0)
    {
        // Segment registers define the privilege level: never changed.
    }
    else if ((fp != NULL) && (size != 0U))
    {
        fp_register_set(fp, regno, value);
        written = TRUE;
    }
    else
    {
        // No floating point registers, or no such register.
    }

    return written;
}

uintptr_t ja_gdb_pc_get(const struct jet_interrupt_context* ctx)
{
    return ctx->rip;
}

void ja_gdb_pc_set(struct jet_interrupt_context* ctx, uintptr_t pc)
{
    ctx->rip = pc;
}

/*
 * Whether interrupts have been disabled for a single step of kernel code
 * (they are enabled again when the step is done).
 */
static pok_bool_t step_enable_interrupts = FALSE;

/*
 * Whether the instruction at the PC of 'ctx' may change the interrupt flag
 * itself: cli, sti, popf or iret, maybe after a REX or operand-size
 * prefix. If it can't be read, assume it may.
 *
 * MISRA C:2025 Rule 11.5 deviation: ja_addr_to_gdb_ro() returns the
 * kernel address of the code as a pointer to void.
 */
static pok_bool_t insn_changes_if(const struct jet_interrupt_context* ctx)
{
    jet_space_id space_id = ja_gdb_context_is_user(ctx)
        ? ja_space_get_current() : 0U;
    const uint8_t* code = ja_addr_to_gdb_ro(ctx->rip, 2U, space_id);
    pok_bool_t changes = TRUE;

    if (code != NULL)
    {
        uint32_t opcode = code[0];

        if (((opcode >= 0x40U) && (opcode <= 0x4FU)) || (opcode == 0x66U)) {
            opcode = code[1];
        }
        changes = (opcode == 0xFAU) || (opcode == 0xFBU) || (opcode == 0x9DU)
            || (opcode == 0xCFU);
    }

    return changes;
}

void ja_gdb_single_step(struct jet_interrupt_context* ctx,
    pok_bool_t enable)
{
    if (enable)
    {
        ctx->rflags |= RFLAGS_TF;

        /*
         * GDB steps one thread and expects no other to run meanwhile:
         * with interrupts disabled (the timer interrupt is pending after a
         * stop), the step is done before the scheduler may switch.
         */
        step_enable_interrupts = FALSE;
        if (((ctx->rflags & RFLAGS_IF) != 0U) && !insn_changes_if(ctx))
        {
            ctx->rflags &= ~(uint64_t)RFLAGS_IF;
            step_enable_interrupts = !ja_gdb_context_is_user(ctx);
        }
    }
    else
    {
        ctx->rflags &= ~(uint64_t)RFLAGS_TF;
    }
}

pok_bool_t ja_gdb_context_is_user(const struct jet_interrupt_context* ctx)
{
    return (ctx->cs & 3U) == 3U;
}

void ja_gdb_sync_icache(void* addr, size_t size)
{
    /*
     * Instruction fetch sees stores on x86, and the return from the
     * exception (iretq) serializes the processor.
     */
    (void) addr;
    (void) size;
}

/* Hardware breakpoint or watchpoint in a debug address register. */
struct hw_slot
{
    pok_bool_t used;
    enum ja_gdb_hw_type type;
    uintptr_t addr;
    size_t len;
    jet_space_id space_id;
};

static struct hw_slot hw_slots[JA_GDB_HW_SLOTS];

/* Whether the debug registers can watch 'len' bytes at 'addr'. */
static pok_bool_t hw_area_supported(enum ja_gdb_hw_type type,
    uintptr_t addr, size_t len)
{
    pok_bool_t supported;

    if (type == JA_GDB_HW_EXEC) {
        // GDB's breakpoint kind on x86 is 1 (the size of int3).
        supported = (len == 1U);
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

    if (hw_area_supported(type, addr, len))
    {
        for (uint32_t i = 0U; i < JA_GDB_HW_SLOTS; i++)
        {
            if (hw_slot_is(&hw_slots[i], type, addr, len, space_id)) {
                inserted = TRUE; // Already there.
            }
        }

        for (uint32_t i = 0U; (i < JA_GDB_HW_SLOTS) && !inserted; i++)
        {
            if (!hw_slots[i].used)
            {
                hw_slots[i].used = TRUE;
                hw_slots[i].type = type;
                hw_slots[i].addr = addr;
                hw_slots[i].len = len;
                hw_slots[i].space_id = space_id;
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

    for (uint32_t i = 0U; i < JA_GDB_HW_SLOTS; i++)
    {
        if (hw_slot_is(&hw_slots[i], type, addr, len, space_id))
        {
            hw_slots[i].used = FALSE;
            removed = TRUE;
        }
    }

    return removed;
}

void ja_gdb_hw_remove_all(void)
{
    for (uint32_t i = 0U; i < JA_GDB_HW_SLOTS; i++)
    {
        hw_slots[i].used = FALSE;
    }
}

void ja_gdb_hw_enter(void)
{
    // The GDB server itself mustn't hit them.
    cpu_write_dr7(0U);
}

/* DR7 R/W and LEN fields of a slot. */
static uint64_t hw_dr7_bits(const struct hw_slot* slot, uint32_t index)
{
    uint64_t rw;
    uint64_t len;

    switch (slot->type)
    {
    case JA_GDB_HW_EXEC:  rw = 0U; break;
    case JA_GDB_HW_WRITE: rw = 1U; break;
    // x86 can't watch reads only: reads are watched with writes.
    default:              rw = 3U; break;
    }

    switch (slot->len)
    {
    case 2U: len = 1U; break;
    case 4U: len = 3U; break;
    case 8U: len = 2U; break;
    default: len = 0U; break;
    }

    uint32_t enable_shift = 2U * index;
    uint32_t rw_shift = 16U + (4U * index);
    uint32_t len_shift = 18U + (4U * index);
    uint64_t enable = (uint64_t)1U << enable_shift;

    return enable | (rw << rw_shift) | (len << len_shift);
}

void ja_gdb_hw_leave(void)
{
    uint64_t dr7 = 0U;

    for (uint32_t i = 0U; i < JA_GDB_HW_SLOTS; i++)
    {
        if (hw_slots[i].used)
        {
            cpu_write_dr_addr(i, hw_slots[i].addr);
            dr7 |= hw_dr7_bits(&hw_slots[i], i);
        }
    }

    if (dr7 != 0U) {
        dr7 |= DR7_LE;
    }

    cpu_write_dr7(dr7);
}

/* Reason of a stop at a hardware breakpoint or watchpoint. */
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

/* Breakpoint exception (int3): rip is after the instruction. */
static pok_bool_t breakpoint_trap(interrupt_frame* frame,
    pok_bool_t from_user)
{
    uintptr_t pc = frame->rip - JA_GDB_BREAKPOINT_SIZE;
    pok_bool_t handled;

    if (!from_user && (pc == pok_trap_addr()))
    {
        handled = jet_gdb_trap(frame, JET_GDB_STOP_REQUEST, pc);
    }
    else if (jet_gdb_is_breakpoint(trap_space(from_user), pc))
    {
        // GDB expects the PC at the breakpoint it has inserted.
        frame->rip = pc;
        handled = jet_gdb_trap(frame, JET_GDB_STOP_SW_BREAKPOINT, pc);
        if (!handled) {
            frame->rip = pc + JA_GDB_BREAKPOINT_SIZE;
        }
    }
    else
    {
        handled = jet_gdb_trap(frame, JET_GDB_STOP_TRAP, pc);
    }

    return handled;
}

/* Debug exception: single step or debug registers. */
static pok_bool_t debug_trap(interrupt_frame* frame, pok_bool_t from_user)
{
    uint64_t dr6 = cpu_read_dr6();
    pok_bool_t handled = FALSE;
    pok_bool_t hit = FALSE;

    cpu_write_dr6(DR6_CLEAR);

    for (uint32_t i = 0U; (i < JA_GDB_HW_SLOTS) && !hit; i++)
    {
        const struct hw_slot* slot = &hw_slots[i];

        if (((dr6 & ((uint64_t)1U << i)) != 0U) && slot->used)
        {
            if (slot->type == JA_GDB_HW_EXEC) {
                // An instruction breakpoint is a fault: let it run.
                frame->rflags |= RFLAGS_RF;
            }

            // All partitions use the same virtual addresses.
            if ((slot->space_id == 0U)
                || (from_user && (slot->space_id == trap_space(from_user))))
            {
                hit = TRUE;
                handled = jet_gdb_trap(frame, hw_stop_reason(slot->type),
                    slot->addr);
            }
            else
            {
                handled = TRUE; // Another partition: go on.
            }
        }
    }

    if (!hit && ((dr6 & DR6_BS) != 0U))
    {
        frame->rflags &= ~(uint64_t)RFLAGS_TF;

        /*
         * Enable interrupts again after a step (ja_gdb_single_step()):
         * user code always runs with them enabled (a step which GDB has
         * given up may end later, in another stop), kernel code as before
         * the step.
         */
        if (from_user || step_enable_interrupts) {
            frame->rflags |= RFLAGS_IF;
        }
        step_enable_interrupts = FALSE;

        handled = jet_gdb_trap(frame, JET_GDB_STOP_STEP, frame->rip);
    }

    return handled;
}

pok_bool_t ja_gdb_trap(interrupt_frame* frame, uint32_t vector,
    pok_bool_t from_user)
{
    pok_bool_t handled;

    if (vector == EXCEPTION_BREAKPOINT) {
        handled = breakpoint_trap(frame, from_user);
    } else {
        handled = debug_trap(frame, from_user);
    }

    return handled;
}

#endif /* POK_NEEDS_GDB */
