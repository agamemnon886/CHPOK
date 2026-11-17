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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; U suffixes for unsigned constants; discarded
 * results are cast to void; the exception class is converted to uint32_t
 * explicitly.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized; deviations are documented where a
 * guideline can't be followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: breakpoint, step and
 * watchpoint exceptions are passed to the GDB server; the handlers give it
 * the registers of the interrupted code (global_thread_stack) and keep
 * debug exceptions masked where the exception frame is used.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: interrupt_frame is used by its
 * typedef name; the macros are moved to interrupt.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the string literals of the Design
 * steps are wrapped at their spaces, each in one code span (they were cut
 * inside names).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/exceptions.c (the CPU support packages are in
 * kernel/csps); sysreg.h is include/csps/arm64/cpu/sysreg.h, shared with the
 * hypervisor.
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
 */

/**
 * @file
 * C handlers for exceptions on arm64, called from vectors.S: synchronous
 * exceptions (syscalls, GDB traps, faults) and IRQs from the kernel and
 * user space, and unexpected exceptions.
 */

#include <libc.h>
#include <core/debug.h>
#include <core/error.h>
#include <core/sched.h>
#include <bsp/bsp.h>
#include <asp/gdb.h>

#include "interrupt.h"
#include <cpu/sysreg.h>

/*
 * MISRA C:2025 Dir 1.2 deviation (this file): read_sysreg() is a GNU
 * statement expression, see sysreg.h.
 */

/**
 * @funcname `dump_registers`
 *
 * @description
 * Prints the registers of an exception frame.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `const interrupt_frame *` | [in] The frame. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int32_t` | The number of a register |
 *
 * @hlr
 * - \hlrid{HLR_EXCEPTIONS_DUMP_REGISTERS_01}: `dump_registers()` shall print
 *   the registers of an exception frame.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_004}
 *
 * @llr
 * - \llrid{LLR_EXCEPTIONS_DUMP_REGISTERS_01}: For each even `i` from 0 to 28,
 *   `dump_registers()` shall call `printf()` once with a format made of `"x%-2d
 *   = 0x%016lx"` twice, with two spaces between them and `"\n"` after them, and
 *   `i`, `x[i]` of `frame`, `i` plus 1 and `x[i + 1]` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_DUMP_REGISTERS_01}
 * - \llrid{LLR_EXCEPTIONS_DUMP_REGISTERS_02}: For `i` equal to 30,
 *   `dump_registers()` shall call `printf()` once with `"x30 = 0x%016lx\n"` and
 *   `x[30]` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_DUMP_REGISTERS_01}
 * - \llrid{LLR_EXCEPTIONS_DUMP_REGISTERS_03}: `dump_registers()` shall then
 *   call `printf()` once with `"sp_el0 = 0x%016lx elr = 0x%016lx spsr =
 *   0x%%lx\n"` and `sp_el0`, `elr` and `spsr` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_DUMP_REGISTERS_01}
 *
 * @design
 * 1. Set `i` to `0`; then while `i` is less than `31`, repeat (after each pass:
 *    add `2` to `i`): \llrref{LLR_EXCEPTIONS_DUMP_REGISTERS_01},
 *    \llrref{LLR_EXCEPTIONS_DUMP_REGISTERS_02}
 *    1. If `i` is equal to `30`: \llrref{LLR_EXCEPTIONS_DUMP_REGISTERS_02}
 *       1. Call `printf()` with the arguments `"x30 = 0x%016lx\n"` and
 *          `(unsigned long)frame->x[30]`.
 *    2. Otherwise: \llrref{LLR_EXCEPTIONS_DUMP_REGISTERS_01}
 *       1. Call `printf()` with the arguments
 *          `"x%-2d = 0x%016lx x%-2d = 0x%016lx\n"`, `i`,
 *          `(unsigned long)frame->x[i]`, `i + 1` and
 *          `(unsigned long)frame->x[i + 1]`.
 * 2. Call `printf()` with the arguments
 *    `"sp_el0 = 0x%016lx elr = 0x%016lx spsr = 0x%%lx\n"`,
 *    `(unsigned long)frame->sp_el0`, `(unsigned long)frame->elr` and
 *    `(unsigned long)frame->spsr`. \llrref{LLR_EXCEPTIONS_DUMP_REGISTERS_03}
 *
 * @calls
 * - `printf()`
 */
static void dump_registers(const interrupt_frame* frame)
{
    for (int32_t i = 0; i < 31; i += 2) {
        if (i == 30) {
            (void) printf("x30 = 0x%016lx\n", (unsigned long)frame->x[30]);
        } else {
            (void) printf("x%-2d = 0x%016lx  x%-2d = 0x%016lx\n",
                i, (unsigned long)frame->x[i],
                i + 1, (unsigned long)frame->x[i + 1]);
        }
    }
    (void) printf("sp_el0 = 0x%016lx elr = 0x%016lx spsr = 0x%lx\n",
        (unsigned long)frame->sp_el0, (unsigned long)frame->elr,
        (unsigned long)frame->spsr);
}

/**
 * @funcname `error_id_for`
 *
 * @description
 * Maps the exception class of a synchronous exception to an ARINC 653 error.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ec` | `uint32_t` | [in] The exception class (ESR_EL1.EC). |
 *
 * @return The error.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_EXCEPTIONS_ERROR_ID_FOR_01}: `error_id_for()` shall give the
 *   error of the health monitoring for a class of synchronous exception: a
 *   memory fault or a misalignment as a memory violation, a floating point
 *   exception as a numeric error, and others as an illegal request.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_003}
 *   - \srqref{SRQ_JET_HM_001}
 *   - \srqref{SRQ_JET_MEM_013}
 *
 * @llr
 * - \llrid{LLR_EXCEPTIONS_ERROR_ID_FOR_01}: If `ec` is `EC_IABORT_LOWER`,
 *   `EC_IABORT_CUR`, `EC_DABORT_LOWER`, `EC_DABORT_CUR`, `EC_PC_ALIGN` or
 *   `EC_SP_ALIGN`, `error_id_for()` shall return
 *   `POK_ERROR_ID_MEMORY_VIOLATION`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_ERROR_ID_FOR_01}
 * - \llrid{LLR_EXCEPTIONS_ERROR_ID_FOR_02}: If `ec` is `EC_FP64`,
 *   `error_id_for()` shall return `POK_ERROR_ID_NUMERIC_ERROR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_ERROR_ID_FOR_01}
 * - \llrid{LLR_EXCEPTIONS_ERROR_ID_FOR_03}: If `ec` is none of
 *   `EC_IABORT_LOWER`, `EC_IABORT_CUR`, `EC_DABORT_LOWER`, `EC_DABORT_CUR`,
 *   `EC_PC_ALIGN`, `EC_SP_ALIGN` and `EC_FP64`, `error_id_for()` shall return
 *   `POK_ERROR_ID_ILLEGAL_REQUEST`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_ERROR_ID_FOR_01}
 *
 * @design
 * 1. Depending on `ec`: \llrref{LLR_EXCEPTIONS_ERROR_ID_FOR_01},
 *    \llrref{LLR_EXCEPTIONS_ERROR_ID_FOR_02},
 *    \llrref{LLR_EXCEPTIONS_ERROR_ID_FOR_03}
 *    1. Case `EC_IABORT_LOWER` or `EC_IABORT_CUR` or `EC_DABORT_LOWER` or
 *       `EC_DABORT_CUR` or `EC_PC_ALIGN` or `EC_SP_ALIGN`:
 *       \llrref{LLR_EXCEPTIONS_ERROR_ID_FOR_01}
 *       1. Return `POK_ERROR_ID_MEMORY_VIOLATION`.
 *    2. Case `EC_FP64`: \llrref{LLR_EXCEPTIONS_ERROR_ID_FOR_02}
 *       1. Return `POK_ERROR_ID_NUMERIC_ERROR`.
 *    3. Otherwise (default): \llrref{LLR_EXCEPTIONS_ERROR_ID_FOR_03}
 *       1. Return `POK_ERROR_ID_ILLEGAL_REQUEST`.
 *
 * @calls
 * None.
 */
static pok_error_id_t error_id_for(uint32_t ec)
{
    switch (ec) {
    case EC_IABORT_LOWER:
    case EC_IABORT_CUR:
    case EC_DABORT_LOWER:
    case EC_DABORT_CUR:
    case EC_PC_ALIGN:
    case EC_SP_ALIGN:
        return POK_ERROR_ID_MEMORY_VIOLATION;
    case EC_FP64:
        return POK_ERROR_ID_NUMERIC_ERROR;
    default:
        // EC_UNKNOWN (undefined instruction), EC_BRK64, ...
        return POK_ERROR_ID_ILLEGAL_REQUEST;
    }
}

/**
 * @funcname `return_user`
 *
 * @description
 * Called before returning to user space from any exception: the core switches
 * FP registers lazily, when returning to user space (the thread returned to may
 * differ from the interrupted one, e.g. after a partition switch on a timer
 * tick).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_EXCEPTIONS_RETURN_USER_01}: `return_user()` shall put the
 *   floating point state of the process that returns to user mode into the
 *   processor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_007}
 *   - \srqref{SRQ_JET_THR_023}
 *   - \srqref{SRQ_JET_THR_024}
 *
 * @llr
 * - \llrid{LLR_EXCEPTIONS_RETURN_USER_01}: `return_user()` shall call
 *   `pok_partition_return_user_fp()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_RETURN_USER_01}
 *
 * @design
 * 1. Call `pok_partition_return_user_fp()`.
 *    \llrref{LLR_EXCEPTIONS_RETURN_USER_01}
 *
 * @calls
 * - `pok_partition_return_user_fp()`
 */
static void return_user(void)
{
    pok_partition_return_user_fp();
}

/**
 * @funcname `ja_exception_sync_user`
 *
 * @description
 * Handles a synchronous exception from user space: a GDB trap, a syscall (svc)
 * or a fault, raised as a partition error.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `global_thread_stack` | `interrupt_frame *` | The frame of the interrupted thread, for the GDB server |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ec` | `uint32_t` | The exception class |
 * | `esr` | `uint64_t` | ESR_EL1 |
 * | `handled` | `pok_bool_t` | Whether GDB handled it |
 *
 * @hlr
 * - \hlrid{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_01}:
 *   `ja_exception_sync_user()` shall take a system call from the supervisor
 *   call instruction of user code.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_002}
 * - \hlrid{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_02}:
 *   `ja_exception_sync_user()` shall handle another synchronous exception from
 *   user code that is not a trap of the debugger as an error of the partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_003}
 * - \hlrid{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_03}:
 *   `ja_exception_sync_user()` shall give the debugger the traps that are its
 *   own.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_01}:
 *   `ja_exception_sync_user()` shall set `global_thread_stack` to `frame`, call
 *   `ja_gdb_exception_enter()` with `frame`, and then `ja_gdb_trap()` with
 *   `frame`, the exception class (the bits 31 to 26 of ESR_EL1) and `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_03}
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_02}: If `ja_gdb_trap()`
 *   returns true, `ja_exception_sync_user()` shall call none of
 *   `ja_gdb_exception_unmask()`, `process_syscall()` and `pok_raise_error()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_03}
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_03}: If `ja_gdb_trap()`
 *   returns false, `ja_exception_sync_user()` shall call
 *   `ja_gdb_exception_unmask()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_03}
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_04}: If `ja_gdb_trap()`
 *   returns false and the exception class is `EC_SVC64`,
 *   `ja_exception_sync_user()` shall call `process_syscall()` once with
 *   `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_01}
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_05}: If `ja_gdb_trap()`
 *   returns false and the exception class is not `EC_SVC64`,
 *   `ja_exception_sync_user()` shall call `printf_debug()` with `"[KERNEL]
 *   Exception in user space: ESR=0x%%lx FAR=0x%%lx ELR=0x%%lx\n"`, ESR_EL1,
 *   FAR_EL1 and `elr` of `frame`, and then `pok_raise_error()` once with the
 *   result of `error_id_for()` for the class, `TRUE` and `elr` of `frame` as
 *   the address.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_02}
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_06}:
 *   `ja_exception_sync_user()` shall end with the calls, in this order, of
 *   `return_user()` and of `ja_gdb_exception_leave()` with `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_01}
 *
 * @design
 * 1. Set `esr` to `read_sysreg(esr_el1)`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_01},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_05}
 * 2. Set `ec` to `(uint32_t) ((esr >> 26) & 0x3fU)`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_01},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_04},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_05}
 * 3. Set `handled` to `FALSE`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_02},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_03},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_04},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_05}
 * 4. Set `global_thread_stack` to `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_01}
 * 5. Call `ja_gdb_exception_enter()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_01}
 * 6. Call `ja_gdb_trap()` with the arguments `frame`, `ec` and `TRUE`, and
 *    store the result in `handled`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_01}
 * 7. If `handled` is false: \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_02},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_03}
 *    1. Call `ja_gdb_exception_unmask()`.
 *       \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_03}
 * 8. If `handled` is true: \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_02}
 * 9. Otherwise, if `ec` is equal to `EC_SVC64`:
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_04}
 *    1. Call `process_syscall()` with the argument `frame`.
 * 10. Otherwise: \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_05}
 *     1. Evaluate `printf_debug("[KERNEL] Exception in user space: ESR=0x%%lx
 *        FAR=0x%%lx ELR=0x%%lx\n", (unsigned long)esr,
 *        (unsigned long)read_sysreg(far_el1), (unsigned long)frame->elr)`.
 *     2. Call `pok_raise_error()` with the arguments `error_id_for(ec)`, `TRUE`
 *        and `(void*)frame->elr`.
 * 11. Call `return_user()`. \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_06}
 * 12. Call `ja_gdb_exception_leave()` with the argument `frame`.
 *     \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_USER_06}
 *
 * @calls
 * - `ja_gdb_exception_enter()`
 * - `ja_gdb_trap()`
 * - `ja_gdb_exception_unmask()`
 * - `process_syscall()`
 * - `printf()`
 * - `pok_raise_error()`
 * - `error_id_for()`
 * - `return_user()`
 * - `ja_gdb_exception_leave()`
 */
void ja_exception_sync_user(interrupt_frame* frame)
{
    uint64_t esr = read_sysreg(esr_el1);
    uint32_t ec = (uint32_t) ((esr >> 26) & 0x3fU);
    pok_bool_t handled = FALSE;

#ifdef POK_NEEDS_GDB
    // Registers of the interrupted thread, for the GDB server.
    global_thread_stack = frame;
    ja_gdb_exception_enter(frame);
    handled = ja_gdb_trap(frame, ec, TRUE);
    if (!handled) {
        ja_gdb_exception_unmask();
    }
#endif /* POK_NEEDS_GDB */

    if (handled)
    {
        // The GDB server has handled a breakpoint, a step or a watchpoint.
    }
    else if (ec == EC_SVC64)
    {
        process_syscall(frame);
    }
    else {
        printf_debug("[KERNEL] Exception in user space: ESR=0x%lx FAR=0x%lx ELR=0x%lx\n",
            (unsigned long)esr, (unsigned long)read_sysreg(far_el1),
            (unsigned long)frame->elr);

        /* MISRA C:2025 Rule 11.6 deviation: the faulting instruction address is
         * reported as a pointer. */
        pok_raise_error(error_id_for(ec), TRUE, (void*)frame->elr);
    }

    return_user();

#ifdef POK_NEEDS_GDB
    ja_gdb_exception_leave(frame);
#endif /* POK_NEEDS_GDB */
}

/**
 * @funcname `ja_exception_sync_kernel`
 *
 * @description
 * Handles a synchronous exception in the kernel: a GDB trap returns, anything
 * else is reported and fatal.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `global_thread_stack` | `interrupt_frame *` | The frame of the interrupted code, for the GDB server |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ec` | `uint32_t` | The exception class |
 * | `esr` | `uint64_t` | ESR_EL1 |
 * | `previous_frame` | `interrupt_frame *` | The frame before |
 *
 * @hlr
 * - \hlrid{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_01}:
 *   `ja_exception_sync_kernel()` shall report a synchronous exception in the
 *   kernel that is not a trap of the debugger, with the registers, and stop the
 *   kernel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_004}
 * - \hlrid{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_02}:
 *   `ja_exception_sync_kernel()` shall give the debugger the traps in the
 *   kernel that are its own, and go on after them.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_014}
 *
 * @llr
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_01}:
 *   `ja_exception_sync_kernel()` shall set `global_thread_stack` to `frame`,
 *   call `ja_gdb_exception_enter()` with `frame`, and then `ja_gdb_trap()` with
 *   `frame`, the exception class (the bits 31 to 26 of ESR_EL1) and `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_02}
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_02}: If `ja_gdb_trap()`
 *   returns true, `ja_exception_sync_kernel()` shall set `global_thread_stack`
 *   back to its value at entry, call `ja_gdb_exception_leave()` with `frame`
 *   and return, without calling `pok_fatal()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_02}
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_03}: If `ja_gdb_trap()`
 *   returns false, `ja_exception_sync_kernel()` shall call, in this order,
 *   `ja_gdb_exception_unmask()`, `printf()` with `"[KERNEL] Exception in
 *   kernel: ESR=0x%%lx FAR=0x%%lx ELR=0x%%lx\n"`, ESR_EL1, FAR_EL1 and `elr` of
 *   `frame`, `dump_registers()` with `frame`, `pok_raise_error()` with the
 *   result of `error_id_for()` for the class, `FALSE` and `elr` of `frame` as
 *   the address, and `pok_fatal()` with `"Unrecoverable exception in kernel"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_01}
 *
 * @design
 * 1. Set `esr` to `read_sysreg(esr_el1)`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_01},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_03}
 * 2. Set `ec` to `(uint32_t) ((esr >> 26) & 0x3fU)`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_01},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_03}
 * 3. Set `previous_frame` to `global_thread_stack`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_02}
 * 4. Set `global_thread_stack` to `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_01}
 * 5. Call `ja_gdb_exception_enter()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_01}
 * 6. If `ja_gdb_trap(frame, ec, FALSE)` is true:
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_01},
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_02}
 *    1. Set `global_thread_stack` to `previous_frame`.
 *       \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_02}
 *    2. Call `ja_gdb_exception_leave()` with the argument `frame`.
 *       \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_02}
 *    3. Return. \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_02}
 * 7. Call `ja_gdb_exception_unmask()`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_03}
 * 8. Call `printf()` with the arguments
 *    `"[KERNEL] Exception in kernel: ESR=0x%%lx FAR=0x%%lx ELR=0x%%lx\n"`,
 *    `(unsigned long)esr`, `(unsigned long)read_sysreg(far_el1)` and
 *    `(unsigned long)frame->elr`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_03}
 * 9. Call `dump_registers()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_03}
 * 10. Call `pok_raise_error()` with the arguments `error_id_for(ec)`, `FALSE`
 *     and `(void*)frame->elr`.
 *     \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_03}
 * 11. Call `pok_fatal()` with the argument
 *     `"Unrecoverable exception in kernel"`.
 *     \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_SYNC_KERNEL_03}
 *
 * @calls
 * - `ja_gdb_exception_enter()`
 * - `ja_gdb_trap()`
 * - `ja_gdb_exception_leave()`
 * - `ja_gdb_exception_unmask()`
 * - `printf()`
 * - `dump_registers()`
 * - `pok_raise_error()`
 * - `error_id_for()`
 * - `pok_fatal()`
 */
void ja_exception_sync_kernel(interrupt_frame* frame)
{
    uint64_t esr = read_sysreg(esr_el1);
    uint32_t ec = (uint32_t) ((esr >> 26) & 0x3fU);

#ifdef POK_NEEDS_GDB
    // Registers of the interrupted code, for the GDB server.
    interrupt_frame* previous_frame = global_thread_stack;
    global_thread_stack = frame;
    ja_gdb_exception_enter(frame);

    if (ja_gdb_trap(frame, ec, FALSE))
    {
        // Back in the kernel code which was interrupted, and to its frame.
        global_thread_stack = previous_frame;
        ja_gdb_exception_leave(frame);
        return;
    }

    ja_gdb_exception_unmask();
#endif /* POK_NEEDS_GDB */

    (void) printf(
        "[KERNEL] Exception in kernel: ESR=0x%lx FAR=0x%lx ELR=0x%lx\n",
        (unsigned long)esr, (unsigned long)read_sysreg(far_el1),
        (unsigned long)frame->elr);
    dump_registers(frame);

    /* MISRA C:2025 Rule 11.6 deviation: the faulting instruction address is
     * reported as a pointer. */
    pok_raise_error(error_id_for(ec), FALSE, (void*)frame->elr);

    // Returning would re-execute the faulting instruction.
    pok_fatal("Unrecoverable exception in kernel");
}

/**
 * @funcname `ja_exception_irq_kernel`
 *
 * @description
 * Handles an IRQ that interrupted the kernel (the board's IRQ handler).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `global_thread_stack` | `interrupt_frame *` | The frame of the interrupted code, for the GDB server |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `previous_frame` | `interrupt_frame *` | The frame before |
 *
 * @hlr
 * - \hlrid{HLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}:
 *   `ja_exception_irq_kernel()` shall handle an interrupt that interrupted the
 *   kernel by the board's handler.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_001}
 *
 * @llr
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}:
 *   `ja_exception_irq_kernel()` shall set `global_thread_stack` to `frame`,
 *   call, in this order, `ja_gdb_exception_enter()` with `frame`,
 *   `ja_gdb_exception_unmask()` and `ja_bsp_process_irq()` with `frame`, then
 *   set `global_thread_stack` back to its value at entry and call
 *   `ja_gdb_exception_leave()` with `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}
 *
 * @design
 * 1. Set `previous_frame` to `global_thread_stack`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}
 * 2. Set `global_thread_stack` to `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}
 * 3. Call `ja_gdb_exception_enter()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}
 * 4. Call `ja_gdb_exception_unmask()`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}
 * 5. Call `ja_bsp_process_irq()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}
 * 6. Set `global_thread_stack` to `previous_frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}
 * 7. Call `ja_gdb_exception_leave()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_KERNEL_01}
 *
 * @calls
 * - `ja_gdb_exception_enter()`
 * - `ja_gdb_exception_unmask()`
 * - `ja_bsp_process_irq()`
 * - `ja_gdb_exception_leave()`
 */
void ja_exception_irq_kernel(interrupt_frame* frame)
{
#ifdef POK_NEEDS_GDB
    // Registers of the interrupted code, for the GDB server.
    interrupt_frame* previous_frame = global_thread_stack;
    global_thread_stack = frame;
    ja_gdb_exception_enter(frame);
    ja_gdb_exception_unmask();
#endif /* POK_NEEDS_GDB */

    ja_bsp_process_irq(frame);

#ifdef POK_NEEDS_GDB
    // Back in the kernel code which was interrupted, and to its frame.
    global_thread_stack = previous_frame;
    ja_gdb_exception_leave(frame);
#endif /* POK_NEEDS_GDB */
}

/**
 * @funcname `ja_exception_irq_user`
 *
 * @description
 * Handles an IRQ that interrupted user space (the board's IRQ handler), then
 * prepares the return to user space.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `global_thread_stack` | `interrupt_frame *` | The frame of the interrupted thread, for the GDB server |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_EXCEPTIONS_JA_EXCEPTION_IRQ_USER_01}: `ja_exception_irq_user()`
 *   shall handle an interrupt that interrupted user code by the board's
 *   handler, and prepare the return to user mode.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_001}
 *   - \srqref{SRQ_JET_SYS_007}
 *
 * @llr
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_USER_01}: `ja_exception_irq_user()`
 *   shall set `global_thread_stack` to `frame` and call, in this order,
 *   `ja_gdb_exception_enter()` with `frame`, `ja_gdb_exception_unmask()`,
 *   `ja_bsp_process_irq()` with `frame`, `return_user()` and
 *   `ja_gdb_exception_leave()` with `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_IRQ_USER_01}
 *
 * @design
 * 1. Set `global_thread_stack` to `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_USER_01}
 * 2. Call `ja_gdb_exception_enter()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_USER_01}
 * 3. Call `ja_gdb_exception_unmask()`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_USER_01}
 * 4. Call `ja_bsp_process_irq()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_USER_01}
 * 5. Call `return_user()`. \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_USER_01}
 * 6. Call `ja_gdb_exception_leave()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_IRQ_USER_01}
 *
 * @calls
 * - `ja_gdb_exception_enter()`
 * - `ja_gdb_exception_unmask()`
 * - `ja_bsp_process_irq()`
 * - `return_user()`
 * - `ja_gdb_exception_leave()`
 */
void ja_exception_irq_user(interrupt_frame* frame)
{
#ifdef POK_NEEDS_GDB
    // Registers of the interrupted thread, for the GDB server.
    global_thread_stack = frame;
    ja_gdb_exception_enter(frame);
    ja_gdb_exception_unmask();
#endif /* POK_NEEDS_GDB */

    ja_bsp_process_irq(frame);
    return_user();

#ifdef POK_NEEDS_GDB
    ja_gdb_exception_leave(frame);
#endif /* POK_NEEDS_GDB */
}

/**
 * @funcname `ja_exception_unexpected`
 *
 * @description
 * Reports an unexpected exception (an exception with SP_EL0 in the kernel, FIQ,
 * SError, AArch32) and stops the kernel.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in] The exception frame. |
 * | `kind` | `int32_t` | [in] The kind of exception (from vectors.S). |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `names` | `static const char *const[6]` | The names of the kinds |
 *
 * @hlr
 * - \hlrid{HLR_EXCEPTIONS_JA_EXCEPTION_UNEXPECTED_01}:
 *   `ja_exception_unexpected()` shall report an unexpected exception (one with
 *   SP_EL0 in the kernel, an FIQ, a system error, or one from 32-bit code),
 *   with the registers, and stop the kernel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_004}
 *
 * @llr
 * - \llrid{LLR_EXCEPTIONS_JA_EXCEPTION_UNEXPECTED_01}:
 *   `ja_exception_unexpected()` shall call, in this order, `printf()` with
 *   `"[KERNEL] Unexpected %%s: ESR=0x%%lx FAR=0x%%lx\n"`, the name
 *   `names[kind]`, ESR_EL1 and FAR_EL1, `dump_registers()` with `frame`, and
 *   `pok_fatal()` with `"Unexpected exception"`; the names are `"exception with
 *   SP_EL0 in kernel"` for 0, `"FIQ in kernel"` for 1, `"SError in kernel"` for
 *   2, `"FIQ in user space"` for 3, `"SError in user space"` for 4 and
 *   `"exception from AArch32 user space"` for 5.
 *   <br>Traces to:
 *   - \hlrref{HLR_EXCEPTIONS_JA_EXCEPTION_UNEXPECTED_01}
 *
 * @design
 * 1. Call `printf()` with the arguments
 *    `"[KERNEL] Unexpected %%s: ESR=0x%%lx FAR=0x%%lx\n"`, `names[kind]`,
 *    `(unsigned long)read_sysreg(esr_el1)` and
 *    `(unsigned long)read_sysreg(far_el1)`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_UNEXPECTED_01}
 * 2. Call `dump_registers()` with the argument `frame`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_UNEXPECTED_01}
 * 3. Call `pok_fatal()` with the argument `"Unexpected exception"`.
 *    \llrref{LLR_EXCEPTIONS_JA_EXCEPTION_UNEXPECTED_01}
 *
 * @calls
 * - `printf()`
 * - `dump_registers()`
 * - `pok_fatal()`
 */
void ja_exception_unexpected(interrupt_frame* frame, int32_t kind)
{
    static const char* const names[] = {
        "exception with SP_EL0 in kernel",
        "FIQ in kernel",
        "SError in kernel",
        "FIQ in user space",
        "SError in user space",
        "exception from AArch32 user space",
    };

    (void) printf("[KERNEL] Unexpected %s: ESR=0x%lx FAR=0x%lx\n", names[kind],
        (unsigned long)read_sysreg(esr_el1), (unsigned long)read_sysreg(far_el1));
    dump_registers(frame);
    pok_fatal("Unexpected exception");
}
