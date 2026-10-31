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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/cswitch.S for arm64.
 */

/* Must correspond to 'struct jet_context' (thread.h). */
#define CONTEXT_SIZE 112
#define CONTEXT_FP   80
#define CONTEXT_DAIF 96

    .text

/* Restore context from current stack. */
CONTEXT_RESTORE:
    ldp x19, x20, [sp, #0]
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #CONTEXT_FP]
    ldr x9, [sp, #CONTEXT_DAIF]
    add sp, sp, #CONTEXT_SIZE
    msr daif, x9
    ret

/* x0: old_sp_p, x1: new_sp */
.globl ja_context_switch
ja_context_switch:
    sub sp, sp, #CONTEXT_SIZE
    stp x19, x20, [sp, #0]
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #CONTEXT_FP]
    mrs x9, daif
    str x9, [sp, #CONTEXT_DAIF]
    /* Store current stack to old_sp */
    mov x9, sp
    str x9, [x0]
    /* Switch stacks. */
    mov sp, x1
    b CONTEXT_RESTORE
    .size ja_context_switch, . - ja_context_switch

/* x0: new_sp */
.globl ja_context_jump
ja_context_jump:
    mov sp, x0
    b CONTEXT_RESTORE
    .size ja_context_jump, . - ja_context_jump

/*
 * Wrapper around the first function in the kernel thread.
 *
 * This function is shown first in the backchain, while returning to
 * it should never occures.
 *
 * sp is assumed pointed to the beginning of the stack,
 * x19 contains address of the function to be called,
 * x29 should be 0,
 * other general registers are unspecified.
 */
.globl ja_kernel_thread
ja_kernel_thread:
    blr x19
    // TODO: assert "no return"
0:  wfi
    b 0b
    .size ja_kernel_thread, . - ja_kernel_thread

/* x0: stack, x1: entry */
.globl ja_context_restart
ja_context_restart:
    mov sp, x0
    mov x19, x1 // x19 is used in ja_kernel_thread
    mov x29, #0 // Mark current frame as first.
    mov x30, #0
    b ja_kernel_thread
    .size ja_context_restart, . - ja_context_restart

/* x0: stack, x1: entry, x2: new_sp_p */
.globl ja_context_restart_and_save
ja_context_restart_and_save:
    sub x9, x0, #CONTEXT_SIZE
    str x9, [x2] // *new_sp_p = stack - CONTEXT_SIZE (any non-NULL value)
    b ja_context_restart // Just jump to common restart procedure.
    .size ja_context_restart_and_save, . - ja_context_restart_and_save

/*
 * Jump to user space.
 *
 * x0: kernel stack, x1: user entry, x2: user stack.
 *
 * The kernel stack becomes SP_EL1, so it is used for exceptions taken
 * from user space.
 */
.globl ja_arm64_user_jump
ja_arm64_user_jump:
    mov sp, x0
    msr sp_el0, x2
    msr elr_el1, x1
    msr spsr_el1, xzr // EL0t, interrupts enabled
    /* Don't leak kernel values to user space. */
    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x5, #0
    mov x6, #0
    mov x7, #0
    mov x8, #0
    mov x9, #0
    mov x10, #0
    mov x11, #0
    mov x12, #0
    mov x13, #0
    mov x14, #0
    mov x15, #0
    mov x16, #0
    mov x17, #0
    mov x18, #0
    mov x19, #0
    mov x20, #0
    mov x21, #0
    mov x22, #0
    mov x23, #0
    mov x24, #0
    mov x25, #0
    mov x26, #0
    mov x27, #0
    mov x28, #0
    mov x29, #0
    mov x30, #0
    eret
    .size ja_arm64_user_jump, . - ja_arm64_user_jump
