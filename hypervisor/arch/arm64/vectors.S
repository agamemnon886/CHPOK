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
 * EL2 exception vectors and the switch to and from a guest.
 *
 * hv_guest_enter(regs) saves the hypervisor's callee-saved registers on
 * its stack and the stack pointer in regs->host_sp, loads the guest's
 * registers from regs (hv_guest_regs_t) and returns to the guest (eret).
 * TPIDR_EL2 points to the same regs (hv_arch_cpu_enable()).
 *
 * An exception from the guest (lower EL) saves the guest's x0-x30,
 * ELR_EL2 and SPSR_EL2 in regs, restores the hypervisor's registers and
 * returns from hv_guest_enter() with the kind of exit (HV_EXIT_*). The
 * hypervisor itself runs with all exceptions masked; an exception at EL2
 * is a fault, reported by hv_el2_exception().
 */

/* hv_guest_regs_t offsets (checked in vcpu.c). */
#define REGS_ELR        248
#define REGS_SPSR       256
#define REGS_HOST_SP    264

/* HV_EXIT_* (internal.h). */
#define EXIT_SYNC       0
#define EXIT_IRQ        1
#define EXIT_FIQ        2
#define EXIT_SERROR     3
#define EXIT_AARCH32    4

/* Hypervisor faults, for hv_el2_exception(). */
#define EL2_SYNC        0
#define EL2_IRQ         1
#define EL2_FIQ         2
#define EL2_SERROR      3

    .text

    /* uint64_t hv_guest_enter(hv_guest_regs_t *regs) */
    .globl hv_guest_enter
hv_guest_enter:
    stp x29, x30, [sp, #-96]!
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    stp x23, x24, [sp, #48]
    stp x25, x26, [sp, #64]
    stp x27, x28, [sp, #80]
    mov x1, sp
    str x1, [x0, #REGS_HOST_SP]

    ldp x1, x2, [x0, #REGS_ELR]
    msr elr_el2, x1
    msr spsr_el2, x2
    ldp x2, x3, [x0, #16]
    ldp x4, x5, [x0, #32]
    ldp x6, x7, [x0, #48]
    ldp x8, x9, [x0, #64]
    ldp x10, x11, [x0, #80]
    ldp x12, x13, [x0, #96]
    ldp x14, x15, [x0, #112]
    ldp x16, x17, [x0, #128]
    ldp x18, x19, [x0, #144]
    ldp x20, x21, [x0, #160]
    ldp x22, x23, [x0, #176]
    ldp x24, x25, [x0, #192]
    ldp x26, x27, [x0, #208]
    ldp x28, x29, [x0, #224]
    ldr x30, [x0, #240]
    ldp x0, x1, [x0]
    eret
    /* No speculation past eret. */
    dsb nsh
    isb

    /*
     * Exit from the guest: x0-x30 are the guest's, SP is the one saved by
     * hv_guest_enter() (the guest doesn't use SP_EL2).
     */
.macro guest_exit kind
    stp x0, x1, [sp, #-16]!
    mrs x0, tpidr_el2
    stp x2, x3, [x0, #16]
    stp x4, x5, [x0, #32]
    stp x6, x7, [x0, #48]
    stp x8, x9, [x0, #64]
    stp x10, x11, [x0, #80]
    stp x12, x13, [x0, #96]
    stp x14, x15, [x0, #112]
    stp x16, x17, [x0, #128]
    stp x18, x19, [x0, #144]
    stp x20, x21, [x0, #160]
    stp x22, x23, [x0, #176]
    stp x24, x25, [x0, #192]
    stp x26, x27, [x0, #208]
    stp x28, x29, [x0, #224]
    str x30, [x0, #240]
    ldp x2, x3, [sp], #16
    stp x2, x3, [x0]
    mov x1, #\kind
    b guest_exit_common
.endm

guest_exit_common:
    mrs x2, elr_el2
    mrs x3, spsr_el2
    stp x2, x3, [x0, #REGS_ELR]
    ldr x2, [x0, #REGS_HOST_SP]
    mov sp, x2
    ldp x19, x20, [sp, #16]
    ldp x21, x22, [sp, #32]
    ldp x23, x24, [sp, #48]
    ldp x25, x26, [sp, #64]
    ldp x27, x28, [sp, #80]
    ldp x29, x30, [sp], #96
    mov x0, x1
    ret

.macro el2_fault kind
    mov x0, #\kind
    b hv_el2_exception
.endm

    /* The vector table: 16 entries of 128 bytes, 2 KiB aligned. */
    .balign 2048
    .globl hv_el2_vectors
hv_el2_vectors:
    /* Current EL with SP_EL0 (never used). */
    .balign 128
    el2_fault EL2_SYNC
    .balign 128
    el2_fault EL2_IRQ
    .balign 128
    el2_fault EL2_FIQ
    .balign 128
    el2_fault EL2_SERROR
    /* Current EL with SP_EL2: faults of the hypervisor. */
    .balign 128
    el2_fault EL2_SYNC
    .balign 128
    el2_fault EL2_IRQ
    .balign 128
    el2_fault EL2_FIQ
    .balign 128
    el2_fault EL2_SERROR
    /* Lower EL, AArch64: the guest. */
    .balign 128
    guest_exit EXIT_SYNC
    .balign 128
    guest_exit EXIT_IRQ
    .balign 128
    guest_exit EXIT_FIQ
    .balign 128
    guest_exit EXIT_SERROR
    /* Lower EL, AArch32: not supported (the VM is stopped). */
    .balign 128
    guest_exit EXIT_AARCH32
    .balign 128
    guest_exit EXIT_AARCH32
    .balign 128
    guest_exit EXIT_AARCH32
    .balign 128
    guest_exit EXIT_AARCH32
    .balign 128
