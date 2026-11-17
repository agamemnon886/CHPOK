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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/arch.c to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; ja_inf_loop() is declared _Noreturn; privileged
 * instructions are documented as isolated inline assembly; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: #include
 * directives come before the code.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/x86_64/arch.c (the CPU support packages are in kernel/csps); the
 * function headers follow outb() and inb(), which call x86_outb() and
 * x86_inb().
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
 * The architecture's initialization and CPU control on x86_64: the
 * interrupt flag, halting and resetting the CPU.
 */

#include <ioports.h>
#include <errno.h>
#include <core/partition.h>

#include "gdt.h"
#include "interrupt.h"
#include "space.h"
#include <bsp/bsp.h>
#include <asp/arch.h>
#include <asp/arch.h>

/**
 * @funcname `pok_arch_init`
 *
 * @description
 * Initializes the architecture at boot (from entry.S, in 64-bit mode): the GDT,
 * the IDT, the board and the memory spaces.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ARCH_X86_64_POK_ARCH_INIT_01}: `pok_arch_init()` shall
 *   initialize the processor and the board at boot: the GDT and task state, the
 *   interrupt table, the board, and the memory spaces with paging.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_002}
 *   - \srqref{SRQ_JET_MEM_002}
 * - \hlrid{HLR_ARCH_X86_64_POK_ARCH_INIT_02}: `pok_arch_init()` shall be the
 *   first C function of the kernel, which the entry code calls in 64-bit mode
 *   after a Multiboot boot loader started the kernel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_001}
 *
 * @llr
 * - \llrid{LLR_ARCH_X86_64_POK_ARCH_INIT_01}: `pok_arch_init()` shall call, in
 *   this order, `ja_gdt_init()`, `ja_idt_init()`, `ja_bsp_init()` and
 *   `ja_space_init()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_X86_64_POK_ARCH_INIT_01}
 * - \llrid{LLR_ARCH_X86_64_POK_ARCH_INIT_02}: `pok_arch_init()` shall be called
 *   once, by `entry.S`, before any other C function of the kernel.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_X86_64_POK_ARCH_INIT_02}
 *
 * @design
 * 1. Call `ja_gdt_init()`. \llrref{LLR_ARCH_X86_64_POK_ARCH_INIT_01},
 *    \llrref{LLR_ARCH_X86_64_POK_ARCH_INIT_02}
 * 2. Call `ja_idt_init()`. \llrref{LLR_ARCH_X86_64_POK_ARCH_INIT_01}
 * 3. Call `ja_bsp_init()`. \llrref{LLR_ARCH_X86_64_POK_ARCH_INIT_01}
 * 4. Call `ja_space_init()`. \llrref{LLR_ARCH_X86_64_POK_ARCH_INIT_01}
 *
 * @calls
 * - `ja_gdt_init()`
 * - `ja_idt_init()`
 * - `ja_bsp_init()`
 * - `ja_space_init()`
 */
void pok_arch_init (void)
{
  ja_gdt_init ();
  ja_idt_init ();
  // As on (32-bit) x86, board first: it initializes the console.
  ja_bsp_init();
  ja_space_init();
}

/**
 * @funcname `ja_preempt_disable`
 *
 * @description
 * Disables interrupts (cli).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ARCH_X86_64_JA_PREEMPT_DISABLE_01}: `ja_preempt_disable()` shall
 *   disable the interrupts of the processor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_002}
 *
 * @llr
 * - \llrid{LLR_ARCH_X86_64_JA_PREEMPT_DISABLE_01}: `ja_preempt_disable()` shall
 *   execute the instruction `cli` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_X86_64_JA_PREEMPT_DISABLE_01}
 *
 * @design
 * 1. Execute the assembly code `cli`.
 *    \llrref{LLR_ARCH_X86_64_JA_PREEMPT_DISABLE_01}
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
  asm volatile ("cli" ::: "memory");
}

/**
 * @funcname `ja_preempt_enable`
 *
 * @description
 * Enables interrupts (sti).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ARCH_X86_64_JA_PREEMPT_ENABLE_01}: `ja_preempt_enable()` shall
 *   enable the interrupts of the processor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_002}
 *
 * @llr
 * - \llrid{LLR_ARCH_X86_64_JA_PREEMPT_ENABLE_01}: `ja_preempt_enable()` shall
 *   execute the instruction `sti` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_X86_64_JA_PREEMPT_ENABLE_01}
 *
 * @design
 * 1. Execute the assembly code `sti`.
 *    \llrref{LLR_ARCH_X86_64_JA_PREEMPT_ENABLE_01}
 *
 * @calls
 * None.
 */
void ja_preempt_enable(void)
{
  asm volatile ("sti" ::: "memory");
}

/**
 * @funcname `get_flags`
 *
 * @description
 * Reads RFLAGS.
 *
 * @return RFLAGS.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `uintptr_t` | RFLAGS |
 *
 * @hlr
 * - \hlrid{HLR_ARCH_GET_FLAGS_01}: `get_flags()` shall give the flags register
 *   of the processor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_007}
 *
 * @llr
 * - \llrid{LLR_ARCH_GET_FLAGS_01}: `get_flags()` shall return the value of
 *   RFLAGS, taken with the instructions `pushfq` and `pop`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_GET_FLAGS_01}
 *
 * @design
 * 1. Execute the assembly code `pushfq;`. \llrref{LLR_ARCH_GET_FLAGS_01}
 * 2. Return `ret`. \llrref{LLR_ARCH_GET_FLAGS_01}
 *
 * @calls
 * None.
 */
static uintptr_t get_flags(void)
{
  uintptr_t ret;

  asm volatile(
    "pushfq\n"
    "pop %0"
    : "=rm" (ret)
    :
    : "memory"
  );

  return ret;
}

/**
 * @funcname `ja_preempt_enabled`
 *
 * @description
 * Tells whether interrupts are enabled (the IF flag of RFLAGS).
 *
 * @return TRUE if they are enabled.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `flags` | `uintptr_t` | RFLAGS |
 *
 * @hlr
 * - \hlrid{HLR_ARCH_X86_64_JA_PREEMPT_ENABLED_01}: `ja_preempt_enabled()` shall
 *   tell whether the interrupts of the processor are enabled.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_007}
 *
 * @llr
 * - \llrid{LLR_ARCH_X86_64_JA_PREEMPT_ENABLED_01}: If bit 9 (IF) is set in the
 *   result of `get_flags()`, `ja_preempt_enabled()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_X86_64_JA_PREEMPT_ENABLED_01}
 * - \llrid{LLR_ARCH_X86_64_JA_PREEMPT_ENABLED_02}: If bit 9 (IF) is not set in
 *   the result of `get_flags()`, `ja_preempt_enabled()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_X86_64_JA_PREEMPT_ENABLED_01}
 *
 * @design
 * 1. Call `get_flags()`, and store the result in `flags`.
 *    \llrref{LLR_ARCH_X86_64_JA_PREEMPT_ENABLED_01},
 *    \llrref{LLR_ARCH_X86_64_JA_PREEMPT_ENABLED_02}
 * 2. Return `((flags & (1UL << 9)) != 0U)`.
 *    \llrref{LLR_ARCH_X86_64_JA_PREEMPT_ENABLED_01},
 *    \llrref{LLR_ARCH_X86_64_JA_PREEMPT_ENABLED_02}
 *
 * @calls
 * - `get_flags()`
 */
pok_bool_t ja_preempt_enabled(void)
{
  uintptr_t flags = get_flags();
  return ((flags & (1UL << 9)) != 0U);
}

/**
 * @funcname `ja_inf_loop`
 *
 * @description
 * Halts the CPU forever (hlt in a loop).
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_ARCH_X86_64_JA_INF_LOOP_01}: `ja_inf_loop()` shall halt the
 *   processor for ever.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_011}
 *   - \srqref{SRQ_JET_PART_007}
 *
 * @llr
 * - \llrid{LLR_ARCH_X86_64_JA_INF_LOOP_01}: `ja_inf_loop()` shall execute the
 *   instruction `hlt` again and again, without returning.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_X86_64_JA_INF_LOOP_01}
 *
 * @design
 * 1. While `true` is true: \llrref{LLR_ARCH_X86_64_JA_INF_LOOP_01}
 *    1. Execute the assembly code `hlt`.
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
      asm volatile ("hlt");
   }
}

/**
 * @funcname `ja_cpu_reset`
 *
 * @description
 * Resets the CPU through the keyboard controller (pulse of the reset line once
 * its input buffer is empty).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `good` | `uint8_t` | The keyboard controller's status |
 *
 * @hlr
 * - \hlrid{HLR_ARCH_X86_64_JA_CPU_RESET_01}: `ja_cpu_reset()` shall reset the
 *   module through the keyboard controller.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_001}
 *   - \srqref{SRQ_JET_DBG_008}
 *
 * @llr
 * - \llrid{LLR_ARCH_X86_64_JA_CPU_RESET_01}: `ja_cpu_reset()` shall read the
 *   port `0x64` with `inb()` until bit 1 (`0x02`) of the byte read is 0, then
 *   write `0xFE` to the port `0x64` with `outb()` and call `ja_inf_loop()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_ARCH_X86_64_JA_CPU_RESET_01}
 *
 * @design
 * 1. Set `good` to `0x02`. \llrref{LLR_ARCH_X86_64_JA_CPU_RESET_01}
 * 2. While `(good & 0x02U)` is not equal to `0U`:
 *    \llrref{LLR_ARCH_X86_64_JA_CPU_RESET_01}
 *    1. Set `good` to `inb(0x64)`.
 * 3. Use the macro `outb(0x64, 0xFE)`. \llrref{LLR_ARCH_X86_64_JA_CPU_RESET_01}
 * 4. Call `ja_inf_loop()`. \llrref{LLR_ARCH_X86_64_JA_CPU_RESET_01}
 *
 * @calls
 * - `x86_inb()`
 * - `x86_outb()`
 * - `ja_inf_loop()`
 */
void ja_cpu_reset(void)
{
    uint8_t good = 0x02;
    while ((good & 0x02U) != 0U) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
    ja_inf_loop();
}
