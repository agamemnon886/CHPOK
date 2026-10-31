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
 * kernel/arch/x86/gdt.h to x86_64 (64-bit).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef __POK_X86_64_GDT_H__
#define __POK_X86_64_GDT_H__

/* Segment selectors, see ja_gdt in entry.S. */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   (0x18 | 3)
#define GDT_USER_CODE   (0x20 | 3)
#define GDT_TSS         0x28

#ifndef __ASSEMBLER__

#include <types.h>

/* Load the GDT with its 64-bit address and the TSS. */
void ja_gdt_init(void);

/* Set the kernel stack used for interrupts from user space. */
void ja_tss_set_rsp0(uint64_t rsp0);

#endif /* __ASSEMBLER__ */

#endif /* !__POK_X86_64_GDT_H__ */
