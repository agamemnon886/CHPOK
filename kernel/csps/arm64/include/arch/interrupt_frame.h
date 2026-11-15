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
 * Modified by Kirill Brazhnikov, 2026-11-13: struct jet_interrupt_context is
 * defined here without a typedef: its typedef, interrupt_frame, is declared in
 * asp/cswitch.h (included here), which the rest of the kernel uses by pointer;
 * Doxygen comments for the file and the members of its types; the types and
 * macros defined in the same way for x86_64 are documented with their x86_64
 * definitions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/include/arch/interrupt_frame.h (the CPU support packages
 * are in kernel/csps); the comments name the new paths.
 */

#ifndef JA_ARM64_INTERRUPT_FRAME
#define JA_ARM64_INTERRUPT_FRAME

#include <stdint.h>
#include <asp/cswitch.h>

/*
 * Registers saved on exception entry (see vectors.S).
 *
 * Layout is used by assembler code: keep INTERRUPT_FRAME_* in
 * vectors.S in sync. Size is a multiple of 16, so the stack stays
 * aligned.
 *
 * Documented with its x86_64 definition:
 * kernel/csps/x86_64/include/arch/interrupt_frame.h.
 */
struct jet_interrupt_context
{
  uint64_t x[31]; /**< arm64: x0 - x30. */
  uint64_t sp_el0; /**< arm64: user stack pointer. */
  uint64_t elr; /**< arm64: return address. */
  uint64_t spsr; /**< arm64: saved processor state. */
};

#endif /* JA_ARM64_INTERRUPT_FRAME */
