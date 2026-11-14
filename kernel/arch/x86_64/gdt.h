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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; Doxygen comments for the file and its macros.
 * Modified by Kirill Brazhnikov, 2026-11-14: the declaration of ja_gdt of
 * kernel/arch/x86_64/gdt.c is moved here; the Doxygen comment of the file is
 * removed (header files are not documented).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_X86_64_GDT_H
#define POK_X86_64_GDT_H

/**
 * @name Segment selectors
 * Segment selectors, see ja_gdt in entry.S.
 * @{
 */

/**
 * @macroname `GDT_KERNEL_CODE`
 *
 * @description
 * The kernel code segment.
 */
#define GDT_KERNEL_CODE 0x08U

/**
 * @macroname `GDT_KERNEL_DATA`
 *
 * @description
 * The kernel data segment.
 */
#define GDT_KERNEL_DATA 0x10

/**
 * @macroname `GDT_USER_DATA`
 *
 * @description
 * The user data segment (RPL 3).
 */
#define GDT_USER_DATA   (0x18 | 3)

/**
 * @macroname `GDT_USER_CODE`
 *
 * @description
 * The user code segment (RPL 3).
 */
#define GDT_USER_CODE   (0x20 | 3)

/**
 * @macroname `GDT_TSS`
 *
 * @description
 * The TSS.
 */
#define GDT_TSS         0x28

/** @} */


#ifndef __ASSEMBLER__

#include <types.h>
#include <stdint.h>

/* Load the GDT with its 64-bit address and the TSS. */
/** \cond */
void ja_gdt_init(void);

/* Set the kernel stack used for interrupts from user space. */
void ja_tss_set_rsp0(uint64_t rsp0);

/** \endcond */

/* The GDT. Defined in entry.S. */
extern uint64_t ja_gdt[];

#endif /* __ASSEMBLER__ */

#endif /* !POK_X86_64_GDT_H */
