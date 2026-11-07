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

/*
 * Interrupt and exception entry stubs of the hypervisor: one 16-byte stub
 * per vector, starting at hv_isr_stubs (idt.c computes the addresses).
 * Each stub pushes a zero error code if the CPU didn't push one, then the
 * vector, and jumps to the common code, which saves the registers that C
 * functions may change and calls hv_interrupt() (hv_int_frame_t in
 * idt.c). The hypervisor runs only in ring 0 and doesn't use the FPU or
 * SSE, so nothing else is saved.
 */

.section .text

  .align 16
.globl hv_isr_stubs
hv_isr_stubs:
  .set vec, 0
  .rept 256
  .align 16
  .if (vec == 8) || ((vec >= 10) && (vec <= 14)) || (vec == 17) || \
      (vec == 21) || (vec == 29) || (vec == 30)
  .else
  pushq $0
  .endif
  pushq $vec
  jmp hv_isr_common
  .set vec, vec + 1
  .endr

hv_isr_common:
  pushq %rax
  pushq %rcx
  pushq %rdx
  pushq %rsi
  pushq %rdi
  pushq %r8
  pushq %r9
  pushq %r10
  pushq %r11
  cld
  movq %rsp, %rdi
  call hv_interrupt
  popq %r11
  popq %r10
  popq %r9
  popq %r8
  popq %rdi
  popq %rsi
  popq %rdx
  popq %rcx
  popq %rax
  addq $16, %rsp        // vector and error code
  iretq
