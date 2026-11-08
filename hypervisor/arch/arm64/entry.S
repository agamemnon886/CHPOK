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
 * Entry points of the arm64 hypervisor.
 *
 * Boot CPU: QEMU's ELF loader (-kernel) starts CPU 0 at hv_entry, at the
 * highest exception level (EL2 with -machine virt,virtualization=on), with
 * the MMU off; the other CPUs are off until PSCI CPU_ON. hv_entry masks
 * the exceptions, clears the BSS, sets the stack of CPU 0 and the EL2
 * exception vectors, and calls hv_main(), which turns the MMU on first.
 *
 * Other CPUs: hv_smp_start() starts them with PSCI CPU_ON at
 * hv_secondary_entry, with the CPU index as the context ID (x0). They
 * turn the MMU on with the boot CPU's tables (hv_el2_mmu, written before
 * the boot CPU's caches were on), then take their stack and call
 * hv_ap_main(index).
 */

#define STACK_SIZE      16384
#define MAX_CPUS        8           /* HV_MAX_CPUS */
#define CURRENT_EL_EL2  (2 << 2)

/* Physical address of the first PL011 (HV_HOST_UART0). */
#define UART0           0x09000000

    .section .text.boot, "ax"

    .globl hv_entry
hv_entry:
    msr daifset, #0xf
    mrs x1, CurrentEL
    cmp x1, #CURRENT_EL_EL2
    b.ne not_el2

    /* Stack of CPU 0: the first one in hv_cpu_stacks. */
    ldr x1, =hv_cpu_stacks + STACK_SIZE
    mov sp, x1

    /* Clear the BSS (8-byte aligned, hv.lds). */
    ldr x1, =__bss_start
    ldr x2, =__bss_end
1:  cmp x1, x2
    b.hs 2f
    str xzr, [x1], #8
    b 1b

2:  ldr x1, =hv_el2_vectors
    msr vbar_el2, x1
    isb

    /* Terminate the frame chain. */
    mov x29, #0
    mov x30, #0
    bl hv_main
    b hv_park

    /*
     * Not at EL2: no hypervisor without virtualization. The message is
     * written to the first PL011, which QEMU needs no setup for.
     */
not_el2:
    adr x1, not_el2_message
    ldr x2, =UART0
3:  ldrb w3, [x1], #1
    cbz w3, hv_park
    str w3, [x2]
    b 3b

hv_park:
    wfi
    b hv_park

not_el2_message:
    .ascii "JetOS hypervisor: EL2 is needed "
    .asciz "(QEMU: -machine virt,virtualization=on)\r\n"
    .balign 4

    .text

    /* Other CPUs, started by PSCI CPU_ON: x0 is the CPU index. */
    .globl hv_secondary_entry
hv_secondary_entry:
    msr daifset, #0xf
    mov x19, x0
    ldr x0, =hv_el2_mmu
    bl hv_mmu_enable

    /* Stack: hv_cpu_stacks[index]. */
    ldr x1, =hv_cpu_stacks + STACK_SIZE
    mov x2, #STACK_SIZE
    madd x1, x19, x2, x1
    mov sp, x1

    ldr x1, =hv_el2_vectors
    msr vbar_el2, x1
    isb

    mov x29, #0
    mov x30, #0
    mov x0, x19
    bl hv_ap_main
    b hv_park

    /*
     * hv_mmu_enable(regs): load MAIR_EL2, TCR_EL2 and TTBR0_EL2 from
     * regs (hv_mmu_regs_t), invalidate the EL2 TLB, then write SCTLR_EL2
     * (MMU and caches on). Uses no stack and no memory but regs.
     */
    .globl hv_mmu_enable
hv_mmu_enable:
    ldp x1, x2, [x0]
    msr mair_el2, x1
    msr tcr_el2, x2
    ldp x1, x2, [x0, #16]
    msr ttbr0_el2, x1
    isb
    tlbi alle2
    dsb nsh
    isb
    msr sctlr_el2, x2
    isb
    ret

    .section .bss
    .balign 16
    .globl hv_cpu_stacks
hv_cpu_stacks:
    .space STACK_SIZE * MAX_CPUS
