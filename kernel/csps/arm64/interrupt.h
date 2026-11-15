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
 * Modified by Kirill Brazhnikov, 2026-11-05: the hooks of the GDB server
 * (gdb.c) for the exception handlers and the scheduler.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; the exception classes of exceptions.c and gdb.c (EC_*) are
 * defined here, with Doxygen headers; Doxygen comments for the file and its
 * macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/interrupt.h (the CPU support packages are in kernel/csps).
 */

#ifndef POK_ARM64_INTERRUPT_H
#define POK_ARM64_INTERRUPT_H

#include <config.h>
#include <types.h>
#include <asp/space.h>
#include <arch/stack.h>
#include <arch/interrupt_frame.h>

/**
 * @name Exception classes
 * Exception classes (ESR_EL1.EC): the reasons of a synchronous exception that
 * the kernel handles.
 * @{
 */

/**
 * @macroname `EC_UNKNOWN`
 *
 * @description
 * An unknown reason (e.g. an undefined instruction).
 */
#define EC_UNKNOWN          0x00

/**
 * @macroname `EC_ILLEGAL_STATE`
 *
 * @description
 * An illegal execution state.
 */
#define EC_ILLEGAL_STATE    0x0e

/**
 * @macroname `EC_SVC64`
 *
 * @description
 * An SVC instruction in AArch64 state (a syscall).
 */
#define EC_SVC64            0x15U

/**
 * @macroname `EC_IABORT_LOWER`
 *
 * @description
 * An instruction abort from a lower exception level (EL0: a partition).
 */
#define EC_IABORT_LOWER     0x20

/**
 * @macroname `EC_IABORT_CUR`
 *
 * @description
 * An instruction abort at the current exception level (EL1: the kernel).
 */
#define EC_IABORT_CUR       0x21

/**
 * @macroname `EC_PC_ALIGN`
 *
 * @description
 * A misaligned PC.
 */
#define EC_PC_ALIGN         0x22

/**
 * @macroname `EC_DABORT_LOWER`
 *
 * @description
 * A data abort from a lower exception level (EL0: a partition).
 */
#define EC_DABORT_LOWER     0x24

/**
 * @macroname `EC_DABORT_CUR`
 *
 * @description
 * A data abort at the current exception level (EL1: the kernel).
 */
#define EC_DABORT_CUR       0x25

/**
 * @macroname `EC_SP_ALIGN`
 *
 * @description
 * A misaligned SP.
 */
#define EC_SP_ALIGN         0x26

/**
 * @macroname `EC_FP64`
 *
 * @description
 * A trapped floating point exception (AArch64).
 */
#define EC_FP64             0x2c

/**
 * @macroname `EC_BRK64`
 *
 * @description
 * A BRK instruction in AArch64 state (a breakpoint).
 */
#define EC_BRK64            0x3c

/** @} */

/**
 * @name Debug exception classes
 * Exception classes (ESR_EL1.EC) of debug exceptions.
 * @{
 */

/**
 * @macroname `EC_BREAKPOINT_LOWER`
 *
 * @description
 * A hardware breakpoint from EL0 (a partition).
 */
#define EC_BREAKPOINT_LOWER 0x30U

/**
 * @macroname `EC_BREAKPOINT_CUR`
 *
 * @description
 * A hardware breakpoint at EL1 (the kernel).
 */
#define EC_BREAKPOINT_CUR   0x31U

/**
 * @macroname `EC_STEP_LOWER`
 *
 * @description
 * A software step from EL0.
 */
#define EC_STEP_LOWER       0x32U

/**
 * @macroname `EC_STEP_CUR`
 *
 * @description
 * A software step at EL1.
 */
#define EC_STEP_CUR         0x33U

/**
 * @macroname `EC_WATCHPOINT_LOWER`
 *
 * @description
 * A watchpoint from EL0.
 */
#define EC_WATCHPOINT_LOWER 0x34U

/**
 * @macroname `EC_WATCHPOINT_CUR`
 *
 * @description
 * A watchpoint at EL1.
 */
#define EC_WATCHPOINT_CUR   0x35U

/** @} */

/* Exception handlers called from vectors.S. */
/** \cond */
void ja_exception_sync_user(interrupt_frame* frame);
void ja_exception_sync_kernel(interrupt_frame* frame);
void ja_exception_irq_kernel(interrupt_frame* frame);
void ja_exception_irq_user(interrupt_frame* frame);
void ja_exception_unexpected(interrupt_frame* frame, int32_t kind);

/* Handle 'svc' from user space. */
void process_syscall(interrupt_frame* frame);
/** \endcond */

#ifdef POK_NEEDS_GDB
/*
 * Hooks of the GDB server (gdb.c) in the exception handlers.
 *
 * Debug exceptions are masked (PSTATE.D) in the vectors, in the GDB
 * server and wherever the processor state of the interrupted code is
 * held (between the restoring of the frame and the exception return).
 * The rest of the kernel runs with them unmasked, so that the hardware
 * breakpoints and watchpoints work in the kernel too.
 *
 * Every handler calls, with debug exceptions masked:
 *
 *  - ja_gdb_exception_enter() first: the single step of the interrupted
 *    code, if there is one, is over;
 *  - ja_gdb_exception_unmask() when it starts to handle an exception
 *    which isn't for the GDB server;
 *  - ja_gdb_exception_leave() last: debug exceptions are masked, and a
 *    requested single step starts with the return.
 */
/** \cond */
void ja_gdb_init(void);
void ja_gdb_exception_enter(interrupt_frame* frame);
void ja_gdb_exception_unmask(void);
void ja_gdb_exception_leave(interrupt_frame* frame);

/*
 * A thread (re)starts in user space with kernel stack 'stack_kernel'
 * (ja_user_space_jump()).
 */
void ja_gdb_user_jump(jet_stack_t stack_kernel);

/*
 * Synchronous exception of class 'ec' (ESR_EL1.EC): if it is a breakpoint,
 * single step or watchpoint, pass it to the GDB server.
 *
 * Return FALSE if it isn't for the GDB server, so it is handled as an
 * ordinary exception.
 */
pok_bool_t ja_gdb_trap(interrupt_frame* frame, uint32_t ec,
    pok_bool_t from_user);

/*
 * Enable the hardware breakpoints and watchpoints of the kernel and of
 * the partition in space 'space_id' (the one which is loaded).
 */
void ja_gdb_hw_program(jet_space_id space_id);
/** \endcond */
#endif /* POK_NEEDS_GDB */

#endif /* !POK_ARM64_INTERRUPT_H */
