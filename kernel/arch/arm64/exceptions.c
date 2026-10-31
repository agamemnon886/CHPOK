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
 */

/* C handlers for exceptions, called from vectors.S. */

#include <libc.h>
#include <core/debug.h>
#include <core/error.h>
#include <core/sched.h>
#include <bsp/bsp.h>

#include "interrupt.h"
#include "sysreg.h"

/* Exception classes (ESR_EL1.EC). */
#define EC_UNKNOWN          0x00
#define EC_ILLEGAL_STATE    0x0e
#define EC_SVC64            0x15
#define EC_IABORT_LOWER     0x20
#define EC_IABORT_CUR       0x21
#define EC_PC_ALIGN         0x22
#define EC_DABORT_LOWER     0x24
#define EC_DABORT_CUR       0x25
#define EC_SP_ALIGN         0x26
#define EC_FP64             0x2c
#define EC_BRK64            0x3c

static void dump_registers(const interrupt_frame* frame)
{
    for (int i = 0; i < 31; i += 2) {
        if (i == 30)
            printf("x30 = 0x%016lx\n", (unsigned long)frame->x[30]);
        else
            printf("x%-2d = 0x%016lx  x%-2d = 0x%016lx\n",
                i, (unsigned long)frame->x[i],
                i + 1, (unsigned long)frame->x[i + 1]);
    }
    printf("sp_el0 = 0x%016lx elr = 0x%016lx spsr = 0x%lx\n",
        (unsigned long)frame->sp_el0, (unsigned long)frame->elr,
        (unsigned long)frame->spsr);
}

/* Map exception class of a synchronous exception to an ARINC error. */
static pok_error_id_t error_id_for(unsigned ec)
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

/*
 * Called before returning to user space from any exception.
 *
 * The core switches FP registers lazily, when returning to user space:
 * the thread we return to may differ from the interrupted one (e.g.
 * after a partition switch on timer tick).
 */
static void return_user(void)
{
    pok_partition_return_user_fp();
}

void ja_exception_sync_user(interrupt_frame* frame)
{
    uint64_t esr = read_sysreg(esr_el1);
    unsigned ec = (esr >> 26) & 0x3f;

    if (ec == EC_SVC64) {
        process_syscall(frame);
    }
    else {
        printf_debug("[KERNEL] Exception in user space: ESR=0x%lx FAR=0x%lx ELR=0x%lx\n",
            (unsigned long)esr, (unsigned long)read_sysreg(far_el1),
            (unsigned long)frame->elr);

        pok_raise_error(error_id_for(ec), TRUE, (void*)frame->elr);
    }

    return_user();
}

void ja_exception_sync_kernel(interrupt_frame* frame)
{
    uint64_t esr = read_sysreg(esr_el1);
    unsigned ec = (esr >> 26) & 0x3f;

    printf("[KERNEL] Exception in kernel: ESR=0x%lx FAR=0x%lx ELR=0x%lx\n",
        (unsigned long)esr, (unsigned long)read_sysreg(far_el1),
        (unsigned long)frame->elr);
    dump_registers(frame);

    pok_raise_error(error_id_for(ec), FALSE, (void*)frame->elr);

    // Returning would re-execute the faulting instruction.
    pok_fatal("Unrecoverable exception in kernel");
}

void ja_exception_irq_kernel(interrupt_frame* frame)
{
    ja_bsp_process_irq(frame);
}

void ja_exception_irq_user(interrupt_frame* frame)
{
    ja_bsp_process_irq(frame);
    return_user();
}

void ja_exception_unexpected(interrupt_frame* frame, int kind)
{
    static const char* const names[] = {
        "exception with SP_EL0 in kernel",
        "FIQ in kernel",
        "SError in kernel",
        "FIQ in user space",
        "SError in user space",
        "exception from AArch32 user space",
    };

    printf("[KERNEL] Unexpected %s: ESR=0x%lx FAR=0x%lx\n", names[kind],
        (unsigned long)read_sysreg(esr_el1), (unsigned long)read_sysreg(far_el1));
    dump_registers(frame);
    pok_fatal("Unexpected exception");
}
