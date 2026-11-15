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
 *
 * @llr
 *
 * @design
 * 1. Call `ja_gdt_init()`.
 * 2. Call `ja_idt_init()`.
 * 3. Call `ja_bsp_init()`.
 * 4. Call `ja_space_init()`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `cli`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `sti`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `pushfq;`.
 * 2. Return `ret`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `get_flags()`, and store the result in `flags`.
 * 2. Return `((flags & (1UL << 9)) != 0U)`.
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
 *
 * @llr
 *
 * @design
 * 1. While `true` is true:
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
 *
 * @llr
 *
 * @design
 * 1. Set `good` to `0x02`.
 * 2. While `(good & 0x02U)` is not equal to `0U`:
 *    1. Set `good` to `inb(0x64)`.
 * 3. Use the macro `outb(0x64, 0xFE)`.
 * 4. Call `ja_inf_loop()`.
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
