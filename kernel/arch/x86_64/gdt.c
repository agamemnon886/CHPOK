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

void ja_gdt_init(void)
{
   uint64_t base = (uint64_t)&pok_tss;
   uint64_t limit = sizeof(pok_tss) - 1;
   sysdesc_t sysdesc;

   memset(&pok_tss, 0, sizeof(pok_tss));
   /* No I/O permission bitmap: partitions run with IOPL 3 (see cswitch.S). */
   pok_tss.iomap_base = sizeof(pok_tss);

   /* Available 64-bit TSS, present. */
   ja_gdt[GDT_TSS / 8] = (limit & 0xffff)
      | ((base & 0xffffff) << 16)
      | (0x89ULL << 40)
      | (((limit >> 16) & 0xf) << 48)
      | (((base >> 24) & 0xff) << 56);
   ja_gdt[GDT_TSS / 8 + 1] = base >> 32;

   /* Reload with the 64-bit base (entry.S has loaded a 32-bit one). */
   sysdesc.limit = 7 * 8 - 1;
   sysdesc.base = (uint64_t)ja_gdt;
   asm volatile ("lgdt %0" : : "m" (sysdesc));

   asm volatile ("ltr %w0" : : "r" (GDT_TSS));
}

void ja_tss_set_rsp0(uint64_t rsp0)
{
   pok_tss.rsp0 = rsp0;
}
