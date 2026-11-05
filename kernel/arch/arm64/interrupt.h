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
 */

#ifndef POK_ARM64_INTERRUPT_H
#define POK_ARM64_INTERRUPT_H

#include <config.h>
#include <types.h>
#include <asp/space.h>
#include <arch/stack.h>
#include <arch/interrupt_frame.h>

/* Exception handlers called from vectors.S. */
void ja_exception_sync_user(interrupt_frame* frame);
void ja_exception_sync_kernel(interrupt_frame* frame);
void ja_exception_irq_kernel(interrupt_frame* frame);
void ja_exception_irq_user(interrupt_frame* frame);
void ja_exception_unexpected(interrupt_frame* frame, int32_t kind);

/* Handle 'svc' from user space. */
void process_syscall(interrupt_frame* frame);

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
#endif /* POK_NEEDS_GDB */

#endif /* !POK_ARM64_INTERRUPT_H */
