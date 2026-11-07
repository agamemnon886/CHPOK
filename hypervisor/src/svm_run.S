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
 * void svm_vmrun(hv_guest_regs_t *regs, uint64_t vmcb_pa)
 *
 * Runs the guest until the next #VMEXIT. VMRUN keeps RAX, RSP and RIP of
 * the guest in the VMCB; the other general-purpose registers are loaded
 * from and stored to *regs here (offsets of hv_guest_regs_t in
 * svm.h).
 *
 * The guest's FS, GS, TR, LDTR and system-call MSRs are loaded with
 * VMLOAD and saved with VMSAVE, so the VMCB always holds them. The
 * hypervisor doesn't use them itself.
 *
 * Interrupts: with V_INTR_MASKING, physical interrupts reach the host
 * while the guest runs only if the host's IF is set, and then cause an
 * INTR #VMEXIT. So GIF is cleared, IF set, and the guest run; #VMEXIT
 * leaves GIF clear, and STGI then lets the pending interrupt be taken by
 * the host's handler before IF is cleared again.
 */

#define G_RBX 0x00
#define G_RCX 0x08
#define G_RDX 0x10
#define G_RSI 0x18
#define G_RDI 0x20
#define G_RBP 0x28
#define G_R8  0x30
#define G_R9  0x38
#define G_R10 0x40
#define G_R11 0x48
#define G_R12 0x50
#define G_R13 0x58
#define G_R14 0x60
#define G_R15 0x68

.section .text

.globl svm_vmrun
svm_vmrun:
  pushq %rbx
  pushq %rbp
  pushq %r12
  pushq %r13
  pushq %r14
  pushq %r15
  pushq %rdi                    // regs

  movq %rsi, %rax               // VMCB physical address
  movq G_RBX(%rdi), %rbx
  movq G_RCX(%rdi), %rcx
  movq G_RDX(%rdi), %rdx
  movq G_RSI(%rdi), %rsi
  movq G_RBP(%rdi), %rbp
  movq G_R8(%rdi), %r8
  movq G_R9(%rdi), %r9
  movq G_R10(%rdi), %r10
  movq G_R11(%rdi), %r11
  movq G_R12(%rdi), %r12
  movq G_R13(%rdi), %r13
  movq G_R14(%rdi), %r14
  movq G_R15(%rdi), %r15
  movq G_RDI(%rdi), %rdi

  clgi
  sti
  vmload %rax
  vmrun %rax
  vmsave %rax

  pushq %rdi                    // guest RDI
  movq 8(%rsp), %rdi            // regs
  movq %rbx, G_RBX(%rdi)
  movq %rcx, G_RCX(%rdi)
  movq %rdx, G_RDX(%rdi)
  movq %rsi, G_RSI(%rdi)
  movq %rbp, G_RBP(%rdi)
  movq %r8, G_R8(%rdi)
  movq %r9, G_R9(%rdi)
  movq %r10, G_R10(%rdi)
  movq %r11, G_R11(%rdi)
  movq %r12, G_R12(%rdi)
  movq %r13, G_R13(%rdi)
  movq %r14, G_R14(%rdi)
  movq %r15, G_R15(%rdi)
  popq %rax
  movq %rax, G_RDI(%rdi)
  addq $8, %rsp                 // regs

  stgi
  nop
  cli

  popq %r15
  popq %r14
  popq %r13
  popq %r12
  popq %rbp
  popq %rbx
  ret
