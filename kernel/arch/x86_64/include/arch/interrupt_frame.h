/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/include/arch/interrupt_frame.h to x86_64 (64-bit).
 */

#ifndef __JA_X86_64_INTERRUPT_FRAME
#define __JA_X86_64_INTERRUPT_FRAME

#include <stdint.h>

/*
 * Registers saved on interrupt entry (see interrupt_entries.S).
 *
 * The order is the reverse of the push order.
 */
typedef struct jet_interrupt_context
{
  /* Pushed by interrupt_common. */
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rbp;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t rbx;
  uint64_t rax;

  /* Pushed by the per-vector stub. */
  uint64_t vector;
  uint64_t error;	/* Error code or 0 */

  /* Pushed by the processor (always, in 64-bit mode). */
  uint64_t rip;
  uint64_t cs;
  uint64_t rflags;
  uint64_t rsp;
  uint64_t ss;
} interrupt_frame;

#endif /* __JA_X86_64_INTERRUPT_FRAME */
