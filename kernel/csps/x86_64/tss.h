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
 * kernel/arch/x86/tss.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its types (with their members).
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/x86_64/tss.h (the CPU support packages are in kernel/csps).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_X86_64_TSS_H
#define POK_X86_64_TSS_H

#include <types.h>

/**
 * @typename `tss_t`
 *
 * @description
 * 64-bit task state segment: only stack pointers are used.
 */
typedef struct
{
   uint32_t reserved0; /**< Reserved. */
   uint64_t rsp0; /**< The stack pointer for interrupts from user mode. */
   uint64_t rsp1; /**< The stack pointer for ring 1 (not used). */
   uint64_t rsp2; /**< The stack pointer for ring 2 (not used). */
   uint64_t reserved1; /**< Reserved. */
   uint64_t ist[7]; /**< The interrupt stack table (not used). */
   uint64_t reserved2; /**< Reserved. */
   uint16_t reserved3; /**< Reserved. */
   uint16_t iomap_base; /**< The offset of the I/O permission bitmap. */
} __attribute__((packed)) tss_t;

#endif /* !POK_X86_64_TSS_H */
