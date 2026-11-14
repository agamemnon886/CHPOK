/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen comments for the file
 * and the fields of its type.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: struct jet_context is used as the
 * typedef jet_context_t (declared in asp/cswitch.h); ja_kernel_thread() is
 * declared here.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef POK_ARM64_THREAD_H
#define POK_ARM64_THREAD_H

#include <types.h>
#include <asp/cswitch.h>

/*
 * Kernel thread context, stored on its stack by ja_context_switch().
 *
 * Only callee-saved registers (AAPCS64) are stored. Layout is used by
 * cswitch.S. Size is a multiple of 16, so the stack stays aligned.
 * Documented with its x86_64 definition (kernel/arch/x86_64/thread.h): the
 * types defined for each architecture have one header.
 */
struct jet_context
{
  uint64_t x19_x28[10]; /**< arm64: x19 to x28 (callee-saved; x19 is the
                             function that a new thread runs, for
                             ja_kernel_thread). */
  uint64_t fp;    /**< arm64: x29, the frame pointer. */
  uint64_t lr;    /**< arm64: x30, where ja_context_switch() returns to. */
  uint64_t daif;  /**< arm64: the interrupt mask (DAIF). */
  uint64_t pad;   /**< arm64: padding to a multiple of 16 bytes. */
};

/* This function starts kernel thread and is defined in cswitch.S.*/
/** \cond */
void ja_kernel_thread(void);
/** \endcond */

#endif /* !POK_ARM64_THREAD_H */
