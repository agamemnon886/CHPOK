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
 * kernel/arch/x86/sysdesc.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its types (with their members).
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_X86_64_SYSDESC_H
#define POK_X86_64_SYSDESC_H

#include <types.h>

/**
 * @typename `sysdesc_t`
 *
 * @description
 * Operand of lgdt/lidt in 64-bit mode.
 */
typedef struct
{
  uint16_t limit; /**< The size of the table, minus 1. */
  uint64_t base; /**< The address of the table. */
} __attribute__((packed)) sysdesc_t;

#endif /* !POK_X86_64_SYSDESC_H */
