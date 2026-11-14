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
 * kernel/arch/x86/thread.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen headers for the file
 * and its types.
 * Modified by Kirill Brazhnikov, 2026-11-13: struct jet_context is used as the
 * typedef jet_context_t (declared in asp/cswitch.h); ja_kernel_thread() is
 * declared here; the header of struct jet_context names no group.
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_X86_64_THREAD_H
#define POK_X86_64_THREAD_H

#include <types.h>
#include <asp/cswitch.h>

/**
 * @typename `jet_context_t`
 *
 * @description
 * The context of a kernel thread that isn't running, stored on its kernel
 * stack by ja_context_switch() (cswitch.S) and restored when the thread
 * runs again; pok_thread_t points to it (sp). Only the callee-saved
 * registers are stored, with the flags register (whose interrupt flag is
 * part of the context) and the address the thread returns to. The layout
 * corresponds to the order in which cswitch.S stores the registers:
 * kernel/arch/x86_64/thread.h defines it for x86_64 (64 bytes) and
 * kernel/arch/arm64/thread.h for arm64 (112 bytes, a multiple of 16).
 */
struct jet_context
{
  uint64_t r15;    /**< x86_64: r15 (callee-saved). */
  uint64_t r14;    /**< x86_64: r14 (callee-saved). */
  uint64_t r13;    /**< x86_64: r13 (callee-saved). */
  uint64_t r12;    /**< x86_64: r12 (callee-saved). */
  uint64_t rbx;    /**< x86_64: rbx (callee-saved; the function that a new
                        thread runs, for ja_kernel_thread). */

  uint64_t rflags; /**< x86_64: the flags register (interrupt flag). */

  uint64_t rbp;    /**< x86_64: rbp, the frame pointer (callee-saved). */
  uint64_t ret;    /**< x86_64: the return address, where the thread
                        continues. */
};

/* This function starts kernel thread and is defined in cswitch.S.*/
/** \cond */
void ja_kernel_thread(void);
/** \endcond */

#endif /* !POK_X86_64_THREAD_H */
