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
 * kernel/arch/x86/interrupt.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 * Modified by Kirill Brazhnikov, 2026-11-04: vectors of the debug and
 * breakpoint exceptions; ja_gdb_trap() for the GDB server.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_INTERRUPT_H
#define POK_INTERRUPT_H

/* Vector of the syscall gate (int $0x80), see libpok/arch/x86_64/syscall.c. */
#define EXCEPTION_SYSCALL 0x80U

/* Debug exception (single step, debug registers) and breakpoint (int3). */
#define EXCEPTION_DEBUG 1U
#define EXCEPTION_BREAKPOINT 3U

/* Vectors of the exceptions and IRQs (remapped by the PIC, see boards/). */
#define EXCEPTION_COUNT 32
#define IRQ_VECTOR_FIRST 32U
#define IRQ_VECTOR_LAST  47U

#ifndef __ASSEMBLER__

#include <config.h>
#include <types.h>
#include <arch/interrupt_frame.h>

/* Fill the IDT and load it. */
void ja_idt_init(void);

/* Called from interrupt_entries.S for every interrupt. */
void ja_interrupt_handler(interrupt_frame* frame);

void process_syscall(interrupt_frame* frame);

#ifdef POK_NEEDS_GDB
/*
 * Debug or breakpoint exception: pass it to the GDB server (gdb.c).
 *
 * Return FALSE if it isn't for the GDB server, so it is handled as an
 * ordinary exception.
 */
pok_bool_t ja_gdb_trap(interrupt_frame* frame, uint32_t vector,
    pok_bool_t from_user);
#endif /* POK_NEEDS_GDB */

#endif /* __ASSEMBLER__ */

#endif /* !POK_INTERRUPT_H */
