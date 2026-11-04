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

/* Called from entry.S, in 64-bit mode. */
void pok_arch_init (void)
{
  ja_gdt_init ();
  ja_idt_init ();
  // As on (32-bit) x86, board first: it initializes the console.
  ja_bsp_init();
  ja_space_init();
}

/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (this file): privileged
 * instructions are written as GNU inline assembly, each isolated in a small
 * function.
 */
void ja_preempt_disable(void)
{
  asm volatile ("cli" ::: "memory");
}

void ja_preempt_enable(void)
{
  asm volatile ("sti" ::: "memory");
}

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

pok_bool_t ja_preempt_enabled(void)
{
  uintptr_t flags = get_flags();
  return ((flags & (1UL << 9)) != 0U);
}

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

void ja_cpu_reset(void)
{
    uint8_t good = 0x02;
    while ((good & 0x02U) != 0U) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
    ja_inf_loop();
}
