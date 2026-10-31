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
 */

#include <errno.h>
#include <asp/arch.h>
#include <asp/entries.h>
#include <bsp/bsp.h>

#include "space.h"
#include "sysreg.h"

/* Called from entry.S with the MMU off. */
void pok_arch_init (void)
{
  // Console works with the MMU off, so configuration errors can be reported.
  jet_console_init_all();
  // MMU first: until it is enabled, all memory is treated as device memory.
  ja_space_init();
  ja_bsp_init();
}

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
  return !(read_sysreg(daif) & DAIF_I);
}

void ja_inf_loop(void)
{
   while (1)
   {
      asm volatile ("wfi");
   }
}

void ja_cpu_reset(void)
{
    ja_bsp_reset();
}
