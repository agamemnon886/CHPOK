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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: the interface between the
 * GDB server and the architecture is replaced: registers are read and
 * written one at a time in GDB's numbering (including floating point
 * ones), and the architecture provides the breakpoint instruction,
 * single-stepping and hardware breakpoints and watchpoints.
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: the interface doesn't assume
 * how many hardware breakpoints and watchpoints there are, or that the
 * server runs with them disabled (arm64 ports the GDB server too).
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; enum ja_gdb_hw_type is the typedef ja_gdb_hw_type_t; the
 * structures and enums are used by their typedef names: interrupt_frame,
 * jet_fp_store_t; Doxygen comments for the file and its types (with their
 * members).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the type hw_slot_t of
 * kernel/arch/x86_64/gdb.c and kernel/arch/arm64/gdb.c (the same on both) is
 * moved here; the enum types are named with _e: ja_gdb_hw_type_e (was
 * ja_gdb_hw_type_t); the Doxygen comment of the file is removed (header files
 * are not documented).
 */

#ifndef JET_ASP_GDB_H
#define JET_ASP_GDB_H

#include <config.h>
#include <types.h>
#include <asp/space.h>

/*
 * The arch header defines the registers known to GDB and the sizes used
 * below:
 *
 * - JA_GDB_NUM_REGS: number of registers in the 'g' packet, numbered as
 *   in GDB's description of the architecture;
 * - JA_GDB_REGISTER_SIZE_MAX: size of the largest register;
 * - JA_GDB_REG_FP, JA_GDB_REG_SP, JA_GDB_REG_PC: frame pointer, stack
 *   pointer and program counter, sent with every stop reply;
 * - JA_GDB_BREAKPOINT_SIZE: size of the breakpoint instruction.
 *
 * It may define more for the architecture's own use, e.g. the number of
 * hardware breakpoints and watchpoints.
 */
#include <arch/gdb.h>


/*
 * Context from the last interrupt. Used for extract registers for
 * interrupted thread.
 *
 * Defined in arch-independent code. Should be assigned by arch code.
 *
 * TODO: Naming is bad.
 */
extern interrupt_frame* global_thread_stack;

/* Execute the breakpoint instruction: enter the GDB server. */
/** \cond */
void pok_trap(void);

/** \endcond */

#ifdef POK_NEEDS_GDB

/* Size, in bytes, of register 'regno' (less than JA_GDB_NUM_REGS). */
/** \cond */
size_t ja_gdb_register_size(uint32_t regno);

/*
 * Read register 'regno' of a thread into 'value' (in the target's byte
 * order, ja_gdb_register_size(regno) bytes).
 *
 * 'ctx' holds the integer registers of the thread, 'fp' its floating
 * point registers. Either may be NULL if the thread has no such
 * registers saved.
 *
 * Return FALSE if the register is not available.
 */
pok_bool_t ja_gdb_register_get(const interrupt_frame* ctx,
    const jet_fp_store_t* fp, uint32_t regno, uint8_t* value);

/*
 * Write register 'regno' of a thread from 'value'.
 *
 * Return FALSE if the register cannot be written (it is left unchanged).
 * Registers which define the privilege level of the thread are never
 * written.
 */
pok_bool_t ja_gdb_register_set(interrupt_frame* ctx,
    jet_fp_store_t* fp, uint32_t regno, const uint8_t* value);

/* Program counter of the context. */
uintptr_t ja_gdb_pc_get(const interrupt_frame* ctx);
void ja_gdb_pc_set(interrupt_frame* ctx, uintptr_t pc);

/*
 * Enable or disable single-stepping for the context: when it is resumed
 * with stepping enabled, it executes one instruction and traps.
 */
void ja_gdb_single_step(interrupt_frame* ctx,
    pok_bool_t enable);

/* Whether the context was saved on entry from user space. */
pok_bool_t ja_gdb_context_is_user(const interrupt_frame* ctx);

/** \endcond */

/* Breakpoint instruction, written over the code by the GDB server. */
extern const uint8_t ja_gdb_breakpoint_insn[JA_GDB_BREAKPOINT_SIZE];

/*
 * Make instructions written at 'addr' (a kernel address) visible to
 * instruction fetch.
 */
/** \cond */
void ja_gdb_sync_icache(void* addr, size_t size);

/** \endcond */

/**
 * @typename `ja_gdb_hw_type_e`
 *
 * @description
 * Kinds of hardware breakpoints and watchpoints.
 */
typedef enum {
    JA_GDB_HW_EXEC, /**< Breakpoint: execution of the instruction. */
    JA_GDB_HW_WRITE, /**< Watchpoint: write. */
    JA_GDB_HW_READ, /**< Watchpoint: read. */
    JA_GDB_HW_ACCESS /**< Watchpoint: read or write. */
} ja_gdb_hw_type_e;

/**
 * @typename `hw_slot_t`
 *
 * @description
 * Hardware breakpoint or watchpoint in a debug register, as the GDB server
 * set it; the same type on x86_64 and arm64. On x86_64
 * (kernel/arch/x86_64/gdb.c), hw_slots has one for each debug address register
 * (DR0 to DR3, JA_GDB_HW_SLOTS); on arm64 (kernel/arch/arm64/gdb.c),
 * hw_breakpoints has one for each breakpoint register (DBGBVRn) and
 * hw_watchpoints one for each watchpoint register (DBGWVRn).
 */
typedef struct
{
    pok_bool_t used;          /**< The slot is in use. */
    ja_gdb_hw_type_e type;    /**< Breakpoint or watchpoint type. */
    uintptr_t addr;           /**< The address watched. */
    size_t len;               /**< The length watched (on x86_64:
                                   1, 2, 4 or 8). */
    jet_space_id space_id;    /**< The space of the address. */
} hw_slot_t;

/*
 * Add a hardware breakpoint or watchpoint for 'len' bytes at virtual
 * address 'addr' of space 'space_id' (0: kernel, any context).
 *
 * The debug registers are programmed on ja_gdb_hw_leave().
 *
 * Return FALSE if there is no free slot or the area is not supported.
 */
/** \cond */
pok_bool_t ja_gdb_hw_insert(ja_gdb_hw_type_e type, uintptr_t addr,
    size_t len, jet_space_id space_id);

/* Remove a hardware breakpoint or watchpoint. FALSE if there is none. */
pok_bool_t ja_gdb_hw_remove(ja_gdb_hw_type_e type, uintptr_t addr,
    size_t len, jet_space_id space_id);

/* Remove all hardware breakpoints and watchpoints. */
void ja_gdb_hw_remove_all(void);

/*
 * Called when the GDB server gets the system (ja_gdb_hw_enter()) and
 * before the system resumes (ja_gdb_hw_leave()): the architecture makes
 * sure that the server itself doesn't hit a hardware breakpoint or
 * watchpoint, and that the ones which were inserted or removed take
 * effect.
 */
void ja_gdb_hw_enter(void);
void ja_gdb_hw_leave(void);

/** \endcond */

#endif /* POK_NEEDS_GDB */

#endif /* JET_ASP_GDB_H */
