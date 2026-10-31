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
 * Interrupt entry points.
 *
 * Every stub pushes an error code (if the processor doesn't) and the
 * vector number, so all interrupts share the same 'interrupt_frame'
 * layout (arch/interrupt_frame.h).
 */

#include "interrupt.h"

/* Exceptions for which the processor pushes an error code. */
#define HAS_ERROR_CODE(v) ((v) == 8 || ((v) >= 10 && (v) <= 14) || (v) == 17 || (v) == 21 || (v) == 29 || (v) == 30)

/* Vectors with a stub in ja_interrupt_stubs: exceptions and IRQs. */
#define VECTORS 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47

  .text

interrupt_common:
  pushq %rax
  pushq %rbx
  pushq %rcx
  pushq %rdx
  pushq %rsi
  pushq %rdi
  pushq %rbp
  pushq %r8
  pushq %r9
  pushq %r10
  pushq %r11
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15
  /* The processor has aligned the stack on 16 before pushing its part. */
  movq %rsp, %rdi
  cld
  call ja_interrupt_handler
  popq %r15
  popq %r14
  popq %r13
  popq %r12
  popq %r11
  popq %r10
  popq %r9
  popq %r8
  popq %rbp
  popq %rdi
  popq %rsi
  popq %rdx
  popq %rcx
  popq %rbx
  popq %rax
  addq $16, %rsp /* vector and error code */
  iretq

.irp vector, VECTORS
  .balign 16
ja_interrupt_stub_\vector:
  .if HAS_ERROR_CODE(\vector)
  .else
  pushq $0
  .endif
  pushq $\vector
  jmp interrupt_common
.endr

  .balign 16
  .globl ja_interrupt_stub_syscall
ja_interrupt_stub_syscall:
  pushq $0
  pushq $EXCEPTION_SYSCALL
  jmp interrupt_common

/* Addresses of the stubs, indexed by vector. */
  .section .rodata
  .balign 8
  .globl ja_interrupt_stubs
ja_interrupt_stubs:
.irp vector, VECTORS
  .quad ja_interrupt_stub_\vector
.endr
