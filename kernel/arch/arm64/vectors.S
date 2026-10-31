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
 * Exception vector table (VBAR_EL1).
 *
 * Every handled exception saves an interrupt_frame on the current
 * stack (SP_EL1), calls a C handler with a pointer to it and restores
 * it. Exceptions from EL0 are taken on the kernel stack which was set
 * by ja_arm64_user_jump().
 */

/* Must correspond to 'interrupt_frame' (arch/interrupt_frame.h). */
#define FRAME_SIZE   272
#define FRAME_X30    240
#define FRAME_ELR    256

/* Kinds of unexpected exceptions, see ja_exception_unexpected(). */
#define KIND_EL1_SP0  0
#define KIND_EL1_FIQ  1
#define KIND_EL1_SERR 2
#define KIND_EL0_FIQ  3
#define KIND_EL0_SERR 4
#define KIND_EL0_A32  5

.macro SAVE_FRAME
    sub sp, sp, #FRAME_SIZE
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    mrs x21, sp_el0
    stp x30, x21, [sp, #FRAME_X30]
    mrs x22, elr_el1
    mrs x23, spsr_el1
    stp x22, x23, [sp, #FRAME_ELR]
    mov x29, #0 // Terminate frame chain for the handler.
.endm

.macro RESTORE_FRAME_AND_RETURN
    ldp x22, x23, [sp, #FRAME_ELR]
    msr elr_el1, x22
    msr spsr_el1, x23
    ldp x30, x21, [sp, #FRAME_X30]
    msr sp_el0, x21
    ldp x0, x1, [sp, #0]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x9, [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x19, [sp, #144]
    ldp x20, x21, [sp, #160]
    ldp x22, x23, [sp, #176]
    ldp x24, x25, [sp, #192]
    ldp x26, x27, [sp, #208]
    ldp x28, x29, [sp, #224]
    add sp, sp, #FRAME_SIZE
    eret
.endm

/* Handled exception: call 'handler(frame)', then return. */
.macro HANDLER name, handler
\name:
    SAVE_FRAME
    mov x0, sp
    bl \handler
    RESTORE_FRAME_AND_RETURN
.endm

/* Unexpected exception: call 'ja_exception_unexpected(frame, kind)', never returns. */
.macro UNEXPECTED name, kind
\name:
    SAVE_FRAME
    mov x0, sp
    mov x1, #\kind
    bl ja_exception_unexpected
0:  b 0b
.endm

.macro VENTRY target
    .balign 0x80
    b \target
.endm

    .text

    .balign 0x800
    .globl ja_exception_vectors
ja_exception_vectors:
    /* Current EL with SP_EL0: never used by the kernel. */
    VENTRY el1_sp0
    VENTRY el1_sp0
    VENTRY el1_sp0
    VENTRY el1_sp0
    /* Current EL with SP_ELx: exceptions in the kernel. */
    VENTRY el1_sync
    VENTRY el1_irq
    VENTRY el1_fiq
    VENTRY el1_serror
    /* Lower EL using AArch64: exceptions in partitions. */
    VENTRY el0_sync
    VENTRY el0_irq
    VENTRY el0_fiq
    VENTRY el0_serror
    /* Lower EL using AArch32: not supported. */
    VENTRY el0_aarch32
    VENTRY el0_aarch32
    VENTRY el0_aarch32
    VENTRY el0_aarch32

HANDLER el1_sync, ja_exception_sync_kernel
HANDLER el1_irq, ja_exception_irq_kernel
HANDLER el0_sync, ja_exception_sync_user
HANDLER el0_irq, ja_exception_irq_user

UNEXPECTED el1_sp0, KIND_EL1_SP0
UNEXPECTED el1_fiq, KIND_EL1_FIQ
UNEXPECTED el1_serror, KIND_EL1_SERR
UNEXPECTED el0_fiq, KIND_EL0_FIQ
UNEXPECTED el0_serror, KIND_EL0_SERR
UNEXPECTED el0_aarch32, KIND_EL0_A32
