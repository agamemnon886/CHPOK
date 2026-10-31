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
 * Kernel entry point.
 *
 * QEMU loads the ELF image and jumps here at EL1 (or at EL2 when
 * virtualization is enabled) with the MMU off.
 */

#define BOOT_STACK_SIZE 16384

    .section .text.boot, "ax"

    .globl _core_entry
_core_entry:
    /* Only the boot CPU runs the kernel. */
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    cbz x0, 1f
0:  wfe
    b 0b

1:  mrs x0, CurrentEL
    lsr x0, x0, #2
    cmp x0, #2
    b.ne el1_entry

    /* Drop from EL2 to EL1. */
    mov x0, #(1 << 31)          /* HCR_EL2.RW: EL1 is AArch64 */
    msr hcr_el2, x0
    mov x0, #3                  /* CNTHCTL_EL2: EL1 may use the counter and timer */
    msr cnthctl_el2, x0
    msr cntvoff_el2, xzr
    mov x0, #0x3c5              /* SPSR: EL1h, all exceptions masked */
    msr spsr_el2, x0
    adr x0, el1_entry
    msr elr_el2, x0
    mov x0, #1
    ldr x1, =ja_boot_from_el2
    strb w0, [x1]
    eret

el1_entry:
    ldr x0, =boot_stack_top
    mov sp, x0

    /*
     * CPACR_EL1.FPEN = 0b11: don't trap FP/SIMD at EL0 and EL1.
     * The kernel itself never uses these registers (-mgeneral-regs-only).
     */
    mov x0, #(3 << 20)
    msr cpacr_el1, x0

    ldr x0, =ja_exception_vectors
    msr vbar_el1, x0
    isb

    /* Terminate the frame chain. */
    mov x29, #0
    mov x30, #0

    bl pok_arch_init
    bl jet_boot
2:  wfi
    b 2b

    .section .data
    .globl ja_boot_from_el2
ja_boot_from_el2:
    .byte 0

    .section .bss
    .balign 16
boot_stack:
    .space BOOT_STACK_SIZE
boot_stack_top:

    .text
    .globl pok_trap
pok_trap:
    brk #0
    ret
