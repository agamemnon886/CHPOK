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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; U suffixes for unsigned constants; ja_inf_loop() is
 * declared _Noreturn; privileged instructions are documented as isolated
 * inline assembly; deviations are documented where a guideline can't be
 * followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: the GDB server is initialized
 * (debug exceptions are enabled in the kernel).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/arch.c (the CPU support packages are in kernel/csps);
 * sysreg.h is include/csps/arm64/cpu/sysreg.h, shared with the hypervisor.
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
 * The architecture's initialization and CPU control on arm64: the IRQ
 * mask, waiting for interrupts and resetting the CPU.
 */

#include <errno.h>
#include <asp/arch.h>
#include <asp/entries.h>
#include <bsp/bsp.h>

#include "space.h"
#include <cpu/sysreg.h>
#include "interrupt.h"

/**
 * @funcname `pok_arch_init`
 *
 * @description
 * Initializes the architecture at boot (from entry.S, with the MMU off): the
 * consoles (they work with the MMU off, so configuration errors can be
 * reported), the MMU (until then all memory is device memory), the board and
 * the GDB server's debug setup.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ARCH_ARM64_POK_ARCH_INIT_01}: `pok_arch_init()` shall initialize
 *   the processor and the board at boot: first the consoles, so that errors can
 *   be reported, then the memory spaces with the MMU, the board and the debug
 *   support.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_002}
 *   - \srqref{SRQ_JET_BOOT_003}
 *   - \srqref{SRQ_JET_MEM_002}
 * - \hlrid{HLR_ARCH_ARM64_POK_ARCH_INIT_02}: `pok_arch_init()` shall be the
 *   first C function of the kernel, which the entry code calls at exception
 *   level 1 with the MMU off, after it has dropped from level 2 if the kernel
 *   was entered there.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_001}
 *
 * @llr
 * - \llrid{LLR_ARCH_ARM64_POK_ARCH_INIT_01}: `pok_arch_init()` shall call, in
 *   this order, `jet_console_init_all()`, `ja_space_init()`, `ja_bsp_init()`
 *   and `ja_gdb_init()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_ARM64_POK_ARCH_INIT_01}
 * - \llrid{LLR_ARCH_ARM64_POK_ARCH_INIT_02}: `pok_arch_init()` shall be called
 *   once, by `entry.S`, before any other C function of the kernel.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_ARM64_POK_ARCH_INIT_02}
 *
 * @design
 * 1. Call `jet_console_init_all()`. \llrref{LLR_ARCH_ARM64_POK_ARCH_INIT_01},
 *    \llrref{LLR_ARCH_ARM64_POK_ARCH_INIT_02}
 * 2. Call `ja_space_init()`. \llrref{LLR_ARCH_ARM64_POK_ARCH_INIT_01}
 * 3. Call `ja_bsp_init()`. \llrref{LLR_ARCH_ARM64_POK_ARCH_INIT_01}
 * 4. Call `ja_gdb_init()`. \llrref{LLR_ARCH_ARM64_POK_ARCH_INIT_01}
 *
 * @calls
 * - `jet_console_init_all()`
 * - `ja_space_init()`
 * - `ja_bsp_init()`
 * - `ja_gdb_init()`
 */
void pok_arch_init (void)
{
  // Console works with the MMU off, so configuration errors can be reported.
  jet_console_init_all();
  // MMU first: until it is enabled, all memory is treated as device memory.
  ja_space_init();
  ja_bsp_init();
#ifdef POK_NEEDS_GDB
  ja_gdb_init();
#endif /* POK_NEEDS_GDB */
}

/**
 * @funcname `ja_preempt_disable`
 *
 * @description
 * Masks IRQs (DAIF.I).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ARCH_ARM64_JA_PREEMPT_DISABLE_01}: `ja_preempt_disable()` shall
 *   mask the interrupts of the processor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_002}
 *
 * @llr
 * - \llrid{LLR_ARCH_ARM64_JA_PREEMPT_DISABLE_01}: `ja_preempt_disable()` shall
 *   execute the instruction `msr daifset, #2` once, which sets the I bit of
 *   DAIF.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_ARM64_JA_PREEMPT_DISABLE_01}
 *
 * @design
 * 1. Execute the assembly code `msr daifset, #2`.
 *    \llrref{LLR_ARCH_ARM64_JA_PREEMPT_DISABLE_01}
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (this file): privileged
 * instructions are written as GNU inline assembly, each isolated in a small
 * function.
 */
void ja_preempt_disable(void)
{
  asm volatile ("msr daifset, #2" ::: "memory");
}

/**
 * @funcname `ja_preempt_enable`
 *
 * @description
 * Unmasks IRQs (DAIF.I).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ARCH_ARM64_JA_PREEMPT_ENABLE_01}: `ja_preempt_enable()` shall
 *   unmask the interrupts of the processor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_002}
 *
 * @llr
 * - \llrid{LLR_ARCH_ARM64_JA_PREEMPT_ENABLE_01}: `ja_preempt_enable()` shall
 *   execute the instruction `msr daifclr, #2` once, which clears the I bit of
 *   DAIF.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_ARM64_JA_PREEMPT_ENABLE_01}
 *
 * @design
 * 1. Execute the assembly code `msr daifclr, #2`.
 *    \llrref{LLR_ARCH_ARM64_JA_PREEMPT_ENABLE_01}
 *
 * @calls
 * None.
 */
void ja_preempt_enable(void)
{
  asm volatile ("msr daifclr, #2" ::: "memory");
}

/**
 * @funcname `ja_preempt_enabled`
 *
 * @description
 * Tells whether IRQs are unmasked.
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
 * - \hlrid{HLR_ARCH_ARM64_JA_PREEMPT_ENABLED_01}: `ja_preempt_enabled()` shall
 *   tell whether the interrupts of the processor are unmasked.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_007}
 *
 * @llr
 * - \llrid{LLR_ARCH_ARM64_JA_PREEMPT_ENABLED_01}: If `DAIF_I` is not set in
 *   DAIF (`read_sysreg()`), `ja_preempt_enabled()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_ARM64_JA_PREEMPT_ENABLED_01}
 * - \llrid{LLR_ARCH_ARM64_JA_PREEMPT_ENABLED_02}: If `DAIF_I` is set in DAIF
 *   (`read_sysreg()`), `ja_preempt_enabled()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_ARM64_JA_PREEMPT_ENABLED_01}
 *
 * @design
 * 1. Return `((read_sysreg(daif) & DAIF_I) == 0U)`.
 *    \llrref{LLR_ARCH_ARM64_JA_PREEMPT_ENABLED_01},
 *    \llrref{LLR_ARCH_ARM64_JA_PREEMPT_ENABLED_02}
 *
 * @calls
 * None.
 */
pok_bool_t ja_preempt_enabled(void)
{
  return ((read_sysreg(daif) & DAIF_I) == 0U);
}

/**
 * @funcname `ja_inf_loop`
 *
 * @description
 * Waits for interrupts forever (wfi in a loop).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ARCH_ARM64_JA_INF_LOOP_01}: `ja_inf_loop()` shall halt the
 *   processor for ever.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_011}
 *   - \srqref{SRQ_JET_PART_007}
 *
 * @llr
 * - \llrid{LLR_ARCH_ARM64_JA_INF_LOOP_01}: `ja_inf_loop()` shall execute the
 *   instruction `wfi` again and again, without returning.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_ARM64_JA_INF_LOOP_01}
 *
 * @design
 * 1. While `true` is true: \llrref{LLR_ARCH_ARM64_JA_INF_LOOP_01}
 *    1. Execute the assembly code `wfi`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Dir 1.2 deviation: _Noreturn is a C11 keyword, accepted by GCC
 * in gnu99 mode; it tells the compiler and the reader that the function
 * doesn't return.
 */
_Noreturn void ja_inf_loop(void)
{
   while (true)
   {
      asm volatile ("wfi");
   }
}

/**
 * @funcname `ja_cpu_reset`
 *
 * @description
 * Resets the CPU (through the board: PSCI).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ARCH_ARM64_JA_CPU_RESET_01}: `ja_cpu_reset()` shall reset the
 *   module through the board.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_001}
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_ARCH_ARM64_JA_CPU_RESET_01}: `ja_cpu_reset()` shall call
 *   `ja_bsp_reset()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_ARM64_JA_CPU_RESET_01}
 *
 * @design
 * 1. Call `ja_bsp_reset()`. \llrref{LLR_ARCH_ARM64_JA_CPU_RESET_01}
 *
 * @calls
 * - `ja_bsp_reset()`
 */
void ja_cpu_reset(void)
{
    ja_bsp_reset();
}
