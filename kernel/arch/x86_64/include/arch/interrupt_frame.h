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
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: struct jet_interrupt_context is
 * defined here without a typedef: its typedef, interrupt_frame, is declared in
 * asp/cswitch.h (included here), which the rest of the kernel uses by pointer;
 * Doxygen comments for the file and its types (with their members).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef JA_X86_64_INTERRUPT_FRAME
#define JA_X86_64_INTERRUPT_FRAME

#include <stdint.h>
#include <asp/cswitch.h>

/*
 * Registers saved on interrupt entry (see interrupt_entries.S).
 *
 * The order is the reverse of the push order.
 */
/**
 * @typename `interrupt_frame`
 *
 * @description
 * The registers saved on an interrupt or exception (an interrupt frame), on the
 * kernel stack. On x86_64 (kernel/arch/x86_64/include/arch/interrupt_frame.h):
 * the general registers pushed by interrupt_common, the vector and error code
 * pushed by the per-vector stub, and what the processor pushes (always, in
 * 64-bit mode). On arm64 (kernel/arch/arm64/include/arch/interrupt_frame.h): x0
 * to x30, the user stack pointer, the return address and the saved state.
 */
struct jet_interrupt_context
{
  /* Pushed by interrupt_common. */
  uint64_t r15; /**< x86_64: r15, pushed by interrupt_common. */
  uint64_t r14; /**< x86_64: r14, pushed by interrupt_common. */
  uint64_t r13; /**< x86_64: r13, pushed by interrupt_common. */
  uint64_t r12; /**< x86_64: r12, pushed by interrupt_common. */
  uint64_t r11; /**< x86_64: r11, pushed by interrupt_common. */
  uint64_t r10; /**< x86_64: r10, pushed by interrupt_common. */
  uint64_t r9; /**< x86_64: r9, pushed by interrupt_common. */
  uint64_t r8; /**< x86_64: r8, pushed by interrupt_common. */
  uint64_t rbp; /**< x86_64: rbp, pushed by interrupt_common. */
  uint64_t rdi; /**< x86_64: rdi, pushed by interrupt_common. */
  uint64_t rsi; /**< x86_64: rsi, pushed by interrupt_common. */
  uint64_t rdx; /**< x86_64: rdx, pushed by interrupt_common. */
  uint64_t rcx; /**< x86_64: rcx, pushed by interrupt_common. */
  uint64_t rbx; /**< x86_64: rbx, pushed by interrupt_common. */
  uint64_t rax; /**< x86_64: rax, pushed by interrupt_common. */

  /* Pushed by the per-vector stub. */
  uint64_t vector; /**< x86_64: the vector, pushed by the per-vector stub. */
  uint64_t error; /**< x86_64: the error code or 0, pushed by the stub. */

  /* Pushed by the processor (always, in 64-bit mode). */
  uint64_t rip; /**< x86_64: rip, pushed by the processor. */
  uint64_t cs; /**< x86_64: cs, pushed by the processor. */
  uint64_t rflags; /**< x86_64: rflags, pushed by the processor. */
  uint64_t rsp; /**< x86_64: rsp, pushed by the processor. */
  uint64_t ss; /**< x86_64: ss, pushed by the processor. */
};

#endif /* JA_X86_64_INTERRUPT_FRAME */
