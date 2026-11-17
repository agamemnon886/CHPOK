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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/thread.c for arm64.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: discarded
 * results are cast to void; deviations are documented where a guideline
 * can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: struct jet_context is used as the
 * typedef jet_context_t (declared in asp/cswitch.h); the declaration of
 * ja_kernel_thread() is moved to thread.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/thread.c (the CPU support packages are in kernel/csps);
 * sysreg.h is include/csps/arm64/cpu/sysreg.h, shared with the hypervisor.
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design.
 */

/**
 * @file
 * Kernel thread contexts on arm64.
 */

#include <asp/arch.h>

#include <libc.h>
#include <asp/cswitch.h>

#include "thread.h"
#include <cpu/sysreg.h>

/*
 * MISRA C:2025 Dir 1.2 deviation (this file): read_sysreg() is a GNU
 * statement expression, see sysreg.h.
 */


/**
 * @funcname `ja_context_init`
 *
 * @description
 * Creates the context of a new kernel thread at the top of its stack: it starts
 * in ja_kernel_thread(), which calls the entry, with the current interrupt mask
 * (the same guess as on x86).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `sp` | `jet_stack_t` | [in] The stack. |
 * | `entry` | `void (*)(void)` | [in] The thread's entry. |
 *
 * @return The context.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctx` | `jet_context_t *` | The context |
 *
 * @hlr
 * - \hlrid{HLR_THREAD_ARM64_JA_CONTEXT_INIT_01}: `ja_context_init()` shall
 *   create the context of a new kernel thread at the top of its stack, so that
 *   switching to it calls the entry, with the interrupt mask that the processor
 *   has now.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_013}
 *
 * @llr
 * - \llrid{LLR_THREAD_ARM64_JA_CONTEXT_INIT_01}: `ja_context_init()` shall set
 *   every byte of a context (`jet_context_t`) that ends at `sp` to 0 with
 *   `memset()`, set its `daif` to the value of DAIF (`read_sysreg()`), its `fp`
 *   to 0, its `x19_x28[0]` to `entry` and its `lr` to the address of
 *   `ja_kernel_thread`, and return its address (`sp` minus its size).
 *   <br>Traces to:
 *   - \hlrref{HLR_THREAD_ARM64_JA_CONTEXT_INIT_01}
 *
 * @design
 * 1. Set `ctx` to `(jet_context_t*)(sp - sizeof(*ctx))`.
 *    \llrref{LLR_THREAD_ARM64_JA_CONTEXT_INIT_01}
 * 2. Call `memset()` with the arguments `ctx`, `0` and `sizeof (*ctx)`.
 *    \llrref{LLR_THREAD_ARM64_JA_CONTEXT_INIT_01}
 * 3. Set `ctx->daif` to `read_sysreg(daif)`.
 *    \llrref{LLR_THREAD_ARM64_JA_CONTEXT_INIT_01}
 * 4. Set `ctx->fp` to `0`. \llrref{LLR_THREAD_ARM64_JA_CONTEXT_INIT_01}
 * 5. Set `ctx->x19_x28[0]` to `(uint64_t)entry`.
 *    \llrref{LLR_THREAD_ARM64_JA_CONTEXT_INIT_01}
 * 6. Set `ctx->lr` to `(uint64_t)ja_kernel_thread`.
 *    \llrref{LLR_THREAD_ARM64_JA_CONTEXT_INIT_01}
 * 7. Return `ctx`. \llrref{LLR_THREAD_ARM64_JA_CONTEXT_INIT_01}
 *
 * @calls
 * - `memset()`
 */
/*
 * MISRA C:2025 Rule 11.1 and Rule 11.4 deviation: the initial context is
 * built on the new thread's stack, which is given as an address, and the
 * entry point is stored as an integer in the register slot that the
 * context switch code jumps to.
 */
jet_context_t* ja_context_init (jet_stack_t sp, void (*entry)(void))
{
    jet_context_t* ctx = (jet_context_t*)(sp - sizeof(*ctx));

    (void) memset (ctx, 0, sizeof (*ctx));
    /*
     * Same guess as on x86: interrupt mask for the switched context is
     * the same as the current one.
     */
    ctx->daif = read_sysreg(daif);
    // Registers below are filled in accordance to ja_kernel_thread().
    ctx->fp = 0;
    ctx->x19_x28[0] = (uint64_t)entry;
    ctx->lr = (uint64_t)ja_kernel_thread;

    return ctx;
}
