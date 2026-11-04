/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/gdt.c to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; U suffixes for unsigned constants; discarded
 * results are cast to void; internal linkage for file-local definitions;
 * loading the GDT and the task register are small inline-assembly
 * functions; deviations are documented where a guideline can't be
 * followed.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized; deviations are documented where a
 * guideline can't be followed.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <types.h>
#include <libc.h>

#include "gdt.h"
#include "tss.h"
#include "sysdesc.h"

/* Defined in entry.S. */
extern uint64_t ja_gdt[];

static tss_t pok_tss __attribute__((aligned(16)));

/*
 * MISRA C:2025 Rule 11.4 deviation (this file): the GDT and the TSS are
 * described to the CPU by their addresses, as integers.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (the functions below):
 * privileged instructions are written as GNU inline assembly, each isolated
 * in a small function.
 */
static void cpu_load_gdt(const sysdesc_t* desc)
{
   asm volatile ("lgdt %0" : : "m" (*desc));
}

static void cpu_load_task_register(uint16_t selector)
{
   asm volatile ("ltr %w0" : : "r" (selector));
}

void ja_gdt_init(void)
{
   uint64_t base = (uint64_t)&pok_tss;
   uint64_t limit = sizeof(pok_tss) - 1U;
   sysdesc_t sysdesc;

   (void) memset(&pok_tss, 0, sizeof(pok_tss));
   /* No I/O permission bitmap: partitions run with IOPL 3 (see cswitch.S). */
   pok_tss.iomap_base = sizeof(pok_tss);

   /* Available 64-bit TSS, present. */
   ja_gdt[GDT_TSS / 8] = (limit & 0xffffU)
      | ((base & 0xffffffU) << 16)
      | (0x89ULL << 40)
      | (((limit >> 16) & 0xfU) << 48)
      | (((base >> 24) & 0xffU) << 56);
   ja_gdt[(GDT_TSS / 8) + 1] = base >> 32;

   /* Reload with the 64-bit base (entry.S has loaded a 32-bit one). */
   sysdesc.limit = (7 * 8) - 1;
   sysdesc.base = (uint64_t)ja_gdt;
   cpu_load_gdt(&sysdesc);

   cpu_load_task_register(GDT_TSS);
}

void ja_tss_set_rsp0(uint64_t rsp0)
{
   pok_tss.rsp0 = rsp0;
}
