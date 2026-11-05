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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; U suffixes for unsigned constants; ja_inf_loop() is
 * declared _Noreturn; privileged instructions are documented as isolated
 * inline assembly; deviations are documented where a guideline can't be
 * followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: the GDB server is initialized
 * (debug exceptions are enabled in the kernel).
 */

#include <errno.h>
#include <asp/arch.h>
#include <asp/entries.h>
#include <bsp/bsp.h>

#include "space.h"
#include "sysreg.h"
#include "interrupt.h"

/* Called from entry.S with the MMU off. */
void pok_arch_init (void)
{
  // Console works with the MMU off, so configuration errors can be reported.
  jet_console_init_all();
  // MMU first: until it is enabled, all memory is treated as device memory.
  ja_space_init();
  ja_bsp_init();
#ifdef POK_NEEDS_GDB
  ja_gdb_init();
#endif /* POK_NEEDS_GDB */
}

/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (this file): privileged
 * instructions are written as GNU inline assembly, each isolated in a small
 * function.
 */
void ja_preempt_disable(void)
{
  asm volatile ("msr daifset, #2" ::: "memory");
}

void ja_preempt_enable(void)
{
  asm volatile ("msr daifclr, #2" ::: "memory");
}

pok_bool_t ja_preempt_enabled(void)
{
  return ((read_sysreg(daif) & DAIF_I) == 0U);
}

/*
 * MISRA C:2025 Dir 1.2 deviation: _Noreturn is a C11 keyword, accepted by GCC
 * in gnu99 mode; it tells the compiler and the reader that the function
 * doesn't return.
 */
_Noreturn void ja_inf_loop(void)
{
   while (true)
   {
      asm volatile ("wfi");
   }
}

void ja_cpu_reset(void)
{
    ja_bsp_reset();
}
