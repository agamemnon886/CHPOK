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
 * kernel/arch/x86/cswitch.S to x86_64 (64-bit).
 */

#include "gdt.h"

/* Size of 'struct jet_context' (thread.h). */
#define CONTEXT_SIZE 64

/* IF (interrupts enabled) and IOPL 3: as on (32-bit) x86, partitions may use I/O ports. */
#define USER_RFLAGS 0x3202

    .text

/* Restore context from current stack. */
CONTEXT_RESTORE:
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    popfq
    pop %rbp
    ret

/* rdi: old_sp_p, rsi: new_sp */
.globl ja_context_switch
ja_context_switch:
    push %rbp
    pushfq
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    /* Store current stack to old_sp */
    mov %rsp, (%rdi)
    /* Switch stacks. */
    mov %rsi, %rsp

    jmp CONTEXT_RESTORE
    .size ja_context_switch, . - ja_context_switch

/* rdi: new_sp */
.globl ja_context_jump
ja_context_jump:
    mov %rdi, %rsp

    jmp CONTEXT_RESTORE
    .size ja_context_jump, . - ja_context_jump


/*
 * Wrapper around the first function in the kernel thread.
 *
 * This function is shown first in the backchain, while returning to
 * it should never occures.
 *
 * %rsp is assumed pointed to the beginning of the stack (16-byte aligned),
 * %rbx contains address of the function to be called,
 * %rbp should be 0,
 * other general registers are unspecified.
 */
.globl ja_kernel_thread
ja_kernel_thread:
    call *%rbx
    // TODO: assert "no return"
1:  hlt
    jmp 1b
    .size ja_kernel_thread, . - ja_kernel_thread


/* rdi: stack, rsi: entry */
.globl ja_context_restart
ja_context_restart:
    mov %rdi, %rsp
    mov %rsi, %rbx // %rbx is used in ja_kernel_thread
    xor %ebp, %ebp // Mark current frame as first.
    jmp ja_kernel_thread
    .size ja_context_restart, . - ja_context_restart

/* rdi: stack, rsi: entry, rdx: new_sp_p */
.globl ja_context_restart_and_save
ja_context_restart_and_save:
    // Fill pointer to new context
    lea -CONTEXT_SIZE(%rdi), %rax
    mov %rax, (%rdx) // *new_sp_p = stack - CONTEXT_SIZE (any non-NULL value)
    jmp ja_context_restart // Just jump to common restart procedure.
    .size ja_context_restart_and_save, . - ja_context_restart_and_save

/*
 * Jump to user space.
 *
 * rdi: kernel stack, rsi: user entry, rdx: user stack.
 *
 * The caller has already set the kernel stack in the TSS.
 */
.globl ja_x86_64_user_jump
ja_x86_64_user_jump:
    mov %rdi, %rsp
    /*
     * The entry is entered "as if" called: the System V ABI requires
     * %rsp + 8 to be 16-byte aligned on function entry.
     */
    sub $8, %rdx
    pushq $GDT_USER_DATA  /* ss */
    pushq %rdx            /* rsp */
    pushq $USER_RFLAGS    /* rflags */
    pushq $GDT_USER_CODE  /* cs */
    pushq %rsi            /* rip */
    mov $GDT_USER_DATA, %ax
    mov %ax, %ds
    mov %ax, %es
    /* Don't leak kernel values to user space. */
    xor %eax, %eax
    xor %ebx, %ebx
    xor %ecx, %ecx
    xor %edx, %edx
    xor %esi, %esi
    xor %edi, %edi
    xor %ebp, %ebp
    xor %r8d, %r8d
    xor %r9d, %r9d
    xor %r10d, %r10d
    xor %r11d, %r11d
    xor %r12d, %r12d
    xor %r13d, %r13d
    xor %r14d, %r14d
    xor %r15d, %r15d
    iretq
    .size ja_x86_64_user_jump, . - ja_x86_64_user_jump
