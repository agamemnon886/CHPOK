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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef __POK_X86_64_THREAD_H__
#define __POK_X86_64_THREAD_H__

#include <types.h>
#include <asp/cswitch.h>

/*
 * Kernel thread context, stored on its stack by ja_context_switch().
 *
 * Only callee-saved registers are stored. Layout corresponds to the
 * push order in cswitch.S.
 */
struct jet_context
{
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t rbx;

  uint64_t rflags;

  uint64_t rbp;
  uint64_t ret;
};

#endif /* !__POK_X86_64_THREAD_H__ */
