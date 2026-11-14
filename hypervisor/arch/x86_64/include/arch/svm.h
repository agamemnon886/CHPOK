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
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from
 * hypervisor/include/hv/ to arch/x86_64/include/arch/.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of svm.c are here; the
 * types and macros are documented.
 */

/*
 * AMD SVM: the virtual machine control block (VMCB), its intercepts and
 * exit codes (AMD64 Architecture Programmer's Manual, volume 2, appendix
 * B). The layout is checked with static assertions in svm.c.
 */

#ifndef HV_SVM_H
#define HV_SVM_H

#include <stdint.h>

/**
 * @macroname `VMCB_SIZE`
 *
 * @description
 * The size of a VMCB, in bytes (one page).
 */
#define VMCB_SIZE       4096U

/**
 * @name SVM intercepts of the instructions and events (misc 1)
 * The bits of intercept_misc1 of the control area.
 * @{
 */

/**
 * @macroname `SVM_INTERCEPT_INTR`
 *
 * @description
 * Intercept physical interrupts.
 */
#define SVM_INTERCEPT_INTR      ((uint32_t) 1U << 0)

/**
 * @macroname `SVM_INTERCEPT_CPUID`
 *
 * @description
 * Intercept the CPUID instruction.
 */
#define SVM_INTERCEPT_CPUID     ((uint32_t) 1U << 18)

/**
 * @macroname `SVM_INTERCEPT_HLT`
 *
 * @description
 * Intercept the HLT instruction.
 */
#define SVM_INTERCEPT_HLT       ((uint32_t) 1U << 24)

/**
 * @macroname `SVM_INTERCEPT_INVLPGA`
 *
 * @description
 * Intercept the INVLPGA instruction.
 */
#define SVM_INTERCEPT_INVLPGA   ((uint32_t) 1U << 26)

/**
 * @macroname `SVM_INTERCEPT_IOIO`
 *
 * @description
 * Intercept the IN and OUT instructions (those on the ports of the I/O
 * permission map).
 */
#define SVM_INTERCEPT_IOIO      ((uint32_t) 1U << 27)

/**
 * @macroname `SVM_INTERCEPT_MSR`
 *
 * @description
 * Intercept the RDMSR and WRMSR instructions (those on the MSRs of the MSR
 * permission map).
 */
#define SVM_INTERCEPT_MSR       ((uint32_t) 1U << 28)

/**
 * @macroname `SVM_INTERCEPT_SHUTDOWN`
 *
 * @description
 * Intercept the shutdown (triple fault) event.
 */
#define SVM_INTERCEPT_SHUTDOWN  ((uint32_t) 1U << 31)

/** @} */

/**
 * @name SVM intercepts of the SVM instructions (misc 2)
 * The bits of intercept_misc2 of the control area.
 * @{
 */

/**
 * @macroname `SVM_INTERCEPT_VMRUN`
 *
 * @description
 * Intercept the VMRUN instruction.
 */
#define SVM_INTERCEPT_VMRUN     ((uint32_t) 1U << 0)

/**
 * @macroname `SVM_INTERCEPT_VMMCALL`
 *
 * @description
 * Intercept the VMMCALL instruction.
 */
#define SVM_INTERCEPT_VMMCALL   ((uint32_t) 1U << 1)

/**
 * @macroname `SVM_INTERCEPT_VMLOAD`
 *
 * @description
 * Intercept the VMLOAD instruction.
 */
#define SVM_INTERCEPT_VMLOAD    ((uint32_t) 1U << 2)

/**
 * @macroname `SVM_INTERCEPT_VMSAVE`
 *
 * @description
 * Intercept the VMSAVE instruction.
 */
#define SVM_INTERCEPT_VMSAVE    ((uint32_t) 1U << 3)

/**
 * @macroname `SVM_INTERCEPT_STGI`
 *
 * @description
 * Intercept the STGI instruction.
 */
#define SVM_INTERCEPT_STGI      ((uint32_t) 1U << 4)

/**
 * @macroname `SVM_INTERCEPT_CLGI`
 *
 * @description
 * Intercept the CLGI instruction.
 */
#define SVM_INTERCEPT_CLGI      ((uint32_t) 1U << 5)

/**
 * @macroname `SVM_INTERCEPT_SKINIT`
 *
 * @description
 * Intercept the SKINIT instruction.
 */
#define SVM_INTERCEPT_SKINIT    ((uint32_t) 1U << 6)

/**
 * @macroname `SVM_INTERCEPT_MONITOR`
 *
 * @description
 * Intercept the MONITOR instruction.
 */
#define SVM_INTERCEPT_MONITOR   ((uint32_t) 1U << 10)

/**
 * @macroname `SVM_INTERCEPT_MWAIT`
 *
 * @description
 * Intercept the MWAIT instruction.
 */
#define SVM_INTERCEPT_MWAIT     ((uint32_t) 1U << 11)

/** @} */

/**
 * @macroname `SVM_INT_CTL_V_IRQ`
 *
 * @description
 * The bit of int_ctl that raises a virtual interrupt.
 */
#define SVM_INT_CTL_V_IRQ           ((uint32_t) 1U << 8)

/**
 * @macroname `SVM_INT_CTL_V_INTR_PRIO`
 *
 * @description
 * The priority field of the virtual interrupt.
 */
#define SVM_INT_CTL_V_INTR_PRIO     ((uint32_t) 0xFU << 16)

/**
 * @macroname `SVM_INT_CTL_V_IGN_TPR`
 *
 * @description
 * The virtual interrupt ignores the task priority register.
 */
#define SVM_INT_CTL_V_IGN_TPR       ((uint32_t) 1U << 20)

/**
 * @macroname `SVM_INT_CTL_V_INTR_MASKING`
 *
 * @description
 * The guest's interrupt flag masks the physical interrupts too (virtualized).
 */
#define SVM_INT_CTL_V_INTR_MASKING  ((uint32_t) 1U << 24)

/**
 * @macroname `SVM_INT_STATE_SHADOW`
 *
 * @description
 * The bit of int_state that tells the guest is in an interrupt shadow.
 */
#define SVM_INT_STATE_SHADOW        ((uint32_t) 1U << 0)

/**
 * @macroname `SVM_TLB_DO_NOTHING`
 *
 * @description
 * Do not flush the TLB at the next VMRUN.
 */
#define SVM_TLB_DO_NOTHING      0U

/**
 * @macroname `SVM_TLB_FLUSH_ALL`
 *
 * @description
 * Flush the TLB entries of all the address spaces at the next VMRUN.
 */
#define SVM_TLB_FLUSH_ALL       1U

/**
 * @macroname `SVM_NESTED_NP_ENABLE`
 *
 * @description
 * The bit of nested_ctl that enables nested paging.
 */
#define SVM_NESTED_NP_ENABLE    1U

/**
 * @macroname `SVM_EVENT_VALID`
 *
 * @description
 * The bit of event_inj that makes the event valid.
 */
#define SVM_EVENT_VALID         (1UL << 31)

/**
 * @macroname `SVM_EVENT_TYPE_EXCEPT`
 *
 * @description
 * The type of an event to inject: an exception.
 */
#define SVM_EVENT_TYPE_EXCEPT   (3UL << 8)

/**
 * @name SVM exit codes
 * The values of exit_code of the control area.
 * @{
 */

/**
 * @macroname `SVM_EXIT_EXCP_BASE`
 *
 * @description
 * The exit code of the exception 0 (the vector is added to it).
 */
#define SVM_EXIT_EXCP_BASE      0x040U

/**
 * @macroname `SVM_EXIT_INTR`
 *
 * @description
 * The exit code of a physical interrupt.
 */
#define SVM_EXIT_INTR           0x060U

/**
 * @macroname `SVM_EXIT_NMI`
 *
 * @description
 * The exit code of a non-maskable interrupt.
 */
#define SVM_EXIT_NMI            0x061U

/**
 * @macroname `SVM_EXIT_CPUID`
 *
 * @description
 * The exit code of the CPUID instruction.
 */
#define SVM_EXIT_CPUID          0x072U

/**
 * @macroname `SVM_EXIT_HLT`
 *
 * @description
 * The exit code of the HLT instruction.
 */
#define SVM_EXIT_HLT            0x078U

/**
 * @macroname `SVM_EXIT_INVLPGA`
 *
 * @description
 * The exit code of the INVLPGA instruction.
 */
#define SVM_EXIT_INVLPGA        0x07AU

/**
 * @macroname `SVM_EXIT_IOIO`
 *
 * @description
 * The exit code of an IN or OUT instruction.
 */
#define SVM_EXIT_IOIO           0x07BU

/**
 * @macroname `SVM_EXIT_MSR`
 *
 * @description
 * The exit code of an RDMSR or WRMSR instruction.
 */
#define SVM_EXIT_MSR            0x07CU

/**
 * @macroname `SVM_EXIT_SHUTDOWN`
 *
 * @description
 * The exit code of a shutdown (triple fault).
 */
#define SVM_EXIT_SHUTDOWN       0x07FU

/**
 * @macroname `SVM_EXIT_VMRUN`
 *
 * @description
 * The exit code of the VMRUN instruction.
 */
#define SVM_EXIT_VMRUN          0x080U

/**
 * @macroname `SVM_EXIT_VMMCALL`
 *
 * @description
 * The exit code of the VMMCALL instruction.
 */
#define SVM_EXIT_VMMCALL        0x081U

/**
 * @macroname `SVM_EXIT_VMLOAD`
 *
 * @description
 * The exit code of the VMLOAD instruction.
 */
#define SVM_EXIT_VMLOAD         0x082U

/**
 * @macroname `SVM_EXIT_VMSAVE`
 *
 * @description
 * The exit code of the VMSAVE instruction.
 */
#define SVM_EXIT_VMSAVE         0x083U

/**
 * @macroname `SVM_EXIT_STGI`
 *
 * @description
 * The exit code of the STGI instruction.
 */
#define SVM_EXIT_STGI           0x084U

/**
 * @macroname `SVM_EXIT_CLGI`
 *
 * @description
 * The exit code of the CLGI instruction.
 */
#define SVM_EXIT_CLGI           0x085U

/**
 * @macroname `SVM_EXIT_SKINIT`
 *
 * @description
 * The exit code of the SKINIT instruction.
 */
#define SVM_EXIT_SKINIT         0x086U

/**
 * @macroname `SVM_EXIT_MONITOR`
 *
 * @description
 * The exit code of the MONITOR instruction.
 */
#define SVM_EXIT_MONITOR        0x08AU

/**
 * @macroname `SVM_EXIT_MWAIT`
 *
 * @description
 * The exit code of the MWAIT instruction.
 */
#define SVM_EXIT_MWAIT          0x08BU

/**
 * @macroname `SVM_EXIT_NPF`
 *
 * @description
 * The exit code of a nested page fault.
 */
#define SVM_EXIT_NPF            0x400U

/**
 * @macroname `SVM_EXIT_INVALID`
 *
 * @description
 * The exit code of an invalid VMCB (VMRUN failed).
 */
#define SVM_EXIT_INVALID        0xFFFFFFFFFFFFFFFFUL

/** @} */

/**
 * @name SVM IOIO exit information
 * The bits of EXITINFO1 of an IOIO exit.
 * @{
 */

/**
 * @macroname `SVM_IOIO_IN`
 *
 * @description
 * The access is an IN; otherwise it is an OUT.
 */
#define SVM_IOIO_IN             (1UL << 0)

/**
 * @macroname `SVM_IOIO_STR`
 *
 * @description
 * The instruction is a string instruction (INS or OUTS).
 */
#define SVM_IOIO_STR            (1UL << 2)

/**
 * @macroname `SVM_IOIO_REP`
 *
 * @description
 * The instruction has a REP prefix.
 */
#define SVM_IOIO_REP            (1UL << 3)

/**
 * @macroname `SVM_IOIO_SZ8`
 *
 * @description
 * The access is 8 bits wide.
 */
#define SVM_IOIO_SZ8            (1UL << 4)

/**
 * @macroname `SVM_IOIO_SZ16`
 *
 * @description
 * The access is 16 bits wide.
 */
#define SVM_IOIO_SZ16           (1UL << 5)

/**
 * @macroname `SVM_IOIO_SZ32`
 *
 * @description
 * The access is 32 bits wide.
 */
#define SVM_IOIO_SZ32           (1UL << 6)

/**
 * @macroname `SVM_IOIO_PORT_SHIFT`
 *
 * @description
 * The position of the port number in EXITINFO1.
 */
#define SVM_IOIO_PORT_SHIFT     16U

/** @} */

/**
 * @macroname `SVM_IOPM_SIZE`
 *
 * @description
 * The size of the I/O permission map, in bytes (12 KiB).
 */
#define SVM_IOPM_SIZE           0x3000U

/**
 * @macroname `SVM_MSRPM_SIZE`
 *
 * @description
 * The size of the MSR permission map, in bytes (8 KiB).
 */
#define SVM_MSRPM_SIZE          0x2000U

/**
 * @typename `vmcb_segment_t`
 *
 * @description
 * Segment register in the save area.
 */
typedef struct {
    uint16_t selector; /**< The segment selector. */
    uint16_t attrib; /**< The attributes of the segment. */
    uint32_t limit; /**< Its limit. */
    uint64_t base; /**< Its base address. */
} vmcb_segment_t;

/**
 * @typename `vmcb_control_t`
 *
 * @description
 * Control area: VMCB offsets 0x000-0x3FF.
 */
typedef struct {
    /**
     * The reads of control registers that are intercepted (bit n: CRn) (offset
     * 0x000).
     */
    uint16_t intercept_cr_read;
    /**
     * The writes of control registers that are intercepted (bit n: CRn) (offset
     * 0x002).
     */
    uint16_t intercept_cr_write;
    /**
     * The reads of debug registers that are intercepted (bit n: DRn) (offset
     * 0x004).
     */
    uint16_t intercept_dr_read;
    /**
     * The writes of debug registers that are intercepted (bit n: DRn) (offset
     * 0x006).
     */
    uint16_t intercept_dr_write;
    /**
     * The exceptions that are intercepted (bit n: vector n) (offset 0x008).
     */
    uint32_t intercept_exceptions;
    /**
     * The intercepted instructions and events (SVM_INTERCEPT_*, first word)
     * (offset 0x00C).
     */
    uint32_t intercept_misc1;
    /**
     * The intercepted SVM instructions (SVM_INTERCEPT_*, second word) (offset
     * 0x010).
     */
    uint32_t intercept_misc2;
    uint8_t reserved_014[0x03CU - 0x014U]; /**< Reserved. */
    /**
     * The threshold of the PAUSE filter (offset 0x03C).
     */
    uint16_t pause_filter_thresh;
    /**
     * The count of the PAUSE filter (offset 0x03E).
     */
    uint16_t pause_filter_count;
    /**
     * The physical address of the I/O permission map (offset 0x040).
     */
    uint64_t iopm_base_pa;
    /**
     * The physical address of the MSR permission map (offset 0x048).
     */
    uint64_t msrpm_base_pa;
    /**
     * The offset added to the time stamp counter the guest reads (offset
     * 0x050).
     */
    uint64_t tsc_offset;
    /**
     * The address space identifier of the guest (offset 0x058).
     */
    uint32_t guest_asid;
    /**
     * The TLB control at the next VMRUN (SVM_TLB_*) (offset 0x05C).
     */
    uint8_t tlb_control;
    uint8_t reserved_05d[3]; /**< Reserved. */
    /**
     * The virtual interrupt control (SVM_INT_CTL_*) (offset 0x060).
     */
    uint32_t int_ctl;
    /**
     * The vector of the virtual interrupt (offset 0x064).
     */
    uint32_t int_vector;
    /**
     * The interrupt shadow state (SVM_INT_STATE_*) (offset 0x068).
     */
    uint32_t int_state;
    uint8_t reserved_06c[4]; /**< Reserved. */
    /**
     * The reason of the \#VMEXIT (SVM_EXIT_*) (offset 0x070).
     */
    uint64_t exit_code;
    /**
     * The first word of information about the exit (offset 0x078).
     */
    uint64_t exit_info1;
    /**
     * The second word of information about the exit (offset 0x080).
     */
    uint64_t exit_info2;
    /**
     * The event that was being delivered when the exit happened (offset 0x088).
     */
    uint64_t exit_int_info;
    /**
     * The nested paging control (SVM_NESTED_*) (offset 0x090).
     */
    uint64_t nested_ctl;
    /**
     * The base address of the AVIC APIC (not used) (offset 0x098).
     */
    uint64_t avic_apic_bar;
    /**
     * The physical address of the GHCB (not used) (offset 0x0A0).
     */
    uint64_t ghcb_pa;
    /**
     * The event to inject into the guest at the next VMRUN (SVM_EVENT_*)
     * (offset 0x0A8).
     */
    uint64_t event_inj;
    /**
     * The physical address of the root of the nested page tables (offset
     * 0x0B0).
     */
    uint64_t nested_cr3;
    /**
     * The virtualization extensions control (offset 0x0B8).
     */
    uint64_t virt_ext;
    uint32_t clean_bits; /**< The VMCB clean bits (offset 0x0C0). */
    uint8_t reserved_0c4[4]; /**< Reserved. */
    /**
     * The RIP of the instruction after the intercepted one, if the CPU saves it
     * (offset 0x0C8).
     */
    uint64_t next_rip;
    uint8_t reserved_0d0[0x400U - 0x0D0U]; /**< Reserved. */
} vmcb_control_t;

/**
 * @typename `vmcb_save_t`
 *
 * @description
 * State save area: VMCB offsets 0x400-0xFFF. The comments give the VMCB
 * offsets; the array sizes are offsets from the start of this area.
 */
typedef struct {
    vmcb_segment_t es; /**< The ES segment register (offset 0x400). */
    vmcb_segment_t cs; /**< The CS segment register (offset 0x410). */
    vmcb_segment_t ss; /**< The SS segment register (offset 0x420). */
    vmcb_segment_t ds; /**< The DS segment register (offset 0x430). */
    vmcb_segment_t fs; /**< The FS segment register (offset 0x440). */
    vmcb_segment_t gs; /**< The GS segment register (offset 0x450). */
    vmcb_segment_t gdtr; /**< The GDT register (offset 0x460). */
    vmcb_segment_t ldtr; /**< The LDT register (offset 0x470). */
    vmcb_segment_t idtr; /**< The IDT register (offset 0x480). */
    vmcb_segment_t tr; /**< The task register (offset 0x490). */
    uint8_t reserved_4a0[0x0CBU - 0x0A0U]; /**< Reserved. */
    uint8_t cpl; /**< The current privilege level (offset 0x4CB). */
    uint8_t reserved_4cc[4]; /**< Reserved. */
    uint64_t efer; /**< The EFER register (offset 0x4D0). */
    uint8_t reserved_4d8[0x148U - 0x0D8U]; /**< Reserved. */
    uint64_t cr4; /**< The CR4 register (offset 0x548). */
    uint64_t cr3; /**< The CR3 register (offset 0x550). */
    uint64_t cr0; /**< The CR0 register (offset 0x558). */
    uint64_t dr7; /**< The DR7 register (offset 0x560). */
    uint64_t dr6; /**< The DR6 register (offset 0x568). */
    uint64_t rflags; /**< The RFLAGS register (offset 0x570). */
    uint64_t rip; /**< The RIP register (offset 0x578). */
    uint8_t reserved_580[0x1D8U - 0x180U]; /**< Reserved. */
    uint64_t rsp; /**< The RSP register (offset 0x5D8). */
    uint8_t reserved_5e0[0x1F8U - 0x1E0U]; /**< Reserved. */
    uint64_t rax; /**< The RAX register (offset 0x5F8). */
    uint64_t star; /**< The STAR MSR (offset 0x600). */
    uint64_t lstar; /**< The LSTAR MSR (offset 0x608). */
    uint64_t cstar; /**< The CSTAR MSR (offset 0x610). */
    uint64_t sfmask; /**< The SFMASK MSR (offset 0x618). */
    uint64_t kernel_gs_base; /**< The KernelGSbase MSR (offset 0x620). */
    uint64_t sysenter_cs; /**< The SYSENTER_CS MSR (offset 0x628). */
    uint64_t sysenter_esp; /**< The SYSENTER_ESP MSR (offset 0x630). */
    uint64_t sysenter_eip; /**< The SYSENTER_EIP MSR (offset 0x638). */
    uint64_t cr2; /**< The CR2 register (offset 0x640). */
    uint8_t reserved_648[0x268U - 0x248U]; /**< Reserved. */
    uint64_t g_pat; /**< The guest PAT MSR (offset 0x668). */
    uint64_t dbgctl; /**< The DBGCTL MSR (offset 0x670). */
    uint8_t reserved_678[0xC00U - 0x278U]; /**< Reserved. */
} vmcb_save_t;

/**
 * @typename `vmcb_t`
 *
 * @description
 * A VMCB, the virtual machine control block: the control area and the state
 * save area.
 */
typedef struct {
    vmcb_control_t control; /**< The control area. */
    vmcb_save_t save; /**< The state save area. */
} vmcb_t;

/**
 * @typename `hv_guest_regs_t`
 *
 * @description
 * The general-purpose registers of a guest that the hypervisor keeps while it
 * runs. x86_64: those that VMRUN does not keep in the VMCB (RAX, RSP and RIP
 * are in the save area); svm_run.S uses the offsets of the members. arm64:
 * X0-X30, ELR_EL2 and SPSR_EL2 of the guest, and the hypervisor's stack pointer
 * while the guest runs; vectors.S uses the offsets of the members (checked in
 * vcpu.c).
 */
typedef struct {
    uint64_t rbx; /**< x86_64: RBX (offset 0x00). */
    uint64_t rcx; /**< x86_64: RCX (offset 0x08). */
    uint64_t rdx; /**< x86_64: RDX (offset 0x10). */
    uint64_t rsi; /**< x86_64: RSI (offset 0x18). */
    uint64_t rdi; /**< x86_64: RDI (offset 0x20). */
    uint64_t rbp; /**< x86_64: RBP (offset 0x28). */
    uint64_t r8; /**< x86_64: R8 (offset 0x30). */
    uint64_t r9; /**< x86_64: R9 (offset 0x38). */
    uint64_t r10; /**< x86_64: R10 (offset 0x40). */
    uint64_t r11; /**< x86_64: R11 (offset 0x48). */
    uint64_t r12; /**< x86_64: R12 (offset 0x50). */
    uint64_t r13; /**< x86_64: R13 (offset 0x58). */
    uint64_t r14; /**< x86_64: R14 (offset 0x60). */
    uint64_t r15; /**< x86_64: R15 (offset 0x68). */
} hv_guest_regs_t;

/**
 * @name CPUID bits
 * The functions and bits of CPUID that the hypervisor reads or reports.
 * @{
 */

/**
 * @macroname `CPUID_EXT_FEATURES`
 *
 * @description
 * The CPUID function that reports the extended features (SVM, SKINIT).
 */
#define CPUID_EXT_FEATURES      0x80000001U

/**
 * @macroname `CPUID_EXT_ECX_SVM`
 *
 * @description
 * The SVM bit of ECX of CPUID function 0x80000001.
 */
#define CPUID_EXT_ECX_SVM       ((uint32_t) 1U << 2)

/**
 * @macroname `CPUID_SVM_FEATURES`
 *
 * @description
 * The CPUID function that reports the SVM features.
 */
#define CPUID_SVM_FEATURES      0x8000000AU

/**
 * @macroname `CPUID_SVM_EDX_NPT`
 *
 * @description
 * The nested paging bit of EDX of the SVM features function.
 */
#define CPUID_SVM_EDX_NPT       ((uint32_t) 1U << 0)

/**
 * @macroname `CPUID_1_ECX_MONITOR`
 *
 * @description
 * The MONITOR/MWAIT bit of ECX of CPUID function 1.
 */
#define CPUID_1_ECX_MONITOR     ((uint32_t) 1U << 3)

/**
 * @macroname `CPUID_1_ECX_VMX`
 *
 * @description
 * The VMX bit of ECX of CPUID function 1.
 */
#define CPUID_1_ECX_VMX         ((uint32_t) 1U << 5)

/**
 * @macroname `CPUID_1_ECX_X2APIC`
 *
 * @description
 * The x2APIC bit of ECX of CPUID function 1.
 */
#define CPUID_1_ECX_X2APIC      ((uint32_t) 1U << 21)

/**
 * @macroname `CPUID_1_ECX_DEADLINE`
 *
 * @description
 * The TSC deadline timer bit of ECX of CPUID function 1.
 */
#define CPUID_1_ECX_DEADLINE    ((uint32_t) 1U << 24)

/**
 * @macroname `CPUID_1_ECX_HYPERVISOR`
 *
 * @description
 * The hypervisor present bit of ECX of CPUID function 1.
 */
#define CPUID_1_ECX_HYPERVISOR  ((uint32_t) 1U << 31)

/**
 * @macroname `CPUID_1_EDX_APIC`
 *
 * @description
 * The local APIC bit of EDX of CPUID function 1.
 */
#define CPUID_1_EDX_APIC        ((uint32_t) 1U << 9)

/**
 * @macroname `CPUID_EXT_ECX_SKINIT`
 *
 * @description
 * The SKINIT bit of ECX of CPUID function 0x80000001.
 */
#define CPUID_EXT_ECX_SKINIT    ((uint32_t) 1U << 12)

/**
 * @macroname `CPUID_HV_BASE`
 *
 * @description
 * The first CPUID function of the hypervisor range.
 */
#define CPUID_HV_BASE           0x40000000U

/**
 * @macroname `CPUID_HV_LAST`
 *
 * @description
 * The last CPUID function of the hypervisor range.
 */
#define CPUID_HV_LAST           0x400000FFU

/** @} */

/**
 * @name Instruction lengths
 * The lengths of the intercepted instructions, which the hypervisor skips.
 * @{
 */

/**
 * @macroname `INSN_LEN_CPUID`
 *
 * @description
 * The length of the CPUID instruction, in bytes.
 */
#define INSN_LEN_CPUID          2U

/**
 * @macroname `INSN_LEN_MSR`
 *
 * @description
 * The length of the RDMSR and WRMSR instructions, in bytes.
 */
#define INSN_LEN_MSR            2U

/**
 * @macroname `INSN_LEN_HLT`
 *
 * @description
 * The length of the HLT instruction, in bytes.
 */
#define INSN_LEN_HLT            1U

/** @} */

/**
 * @macroname `EXCEPTION_UD`
 *
 * @description
 * The vector of the invalid opcode exception.
 */
#define EXCEPTION_UD            6U

/**
 * @macroname `MULTIBOOT_BOOT_MAGIC`
 *
 * @description
 * The value of EAX when a Multiboot boot loader starts the guest.
 */
#define MULTIBOOT_BOOT_MAGIC    0x2BADB002U

/**
 * @name Guest start state
 * The state of the guest at its start (the reset values of an x86 CPU, in
 * 32-bit protected mode).
 * @{
 */

/**
 * @macroname `GUEST_CODE_SELECTOR`
 *
 * @description
 * The code segment selector of the guest at its start.
 */
#define GUEST_CODE_SELECTOR     0x08U

/**
 * @macroname `GUEST_DATA_SELECTOR`
 *
 * @description
 * The data segment selector of the guest at its start.
 */
#define GUEST_DATA_SELECTOR     0x10U

/**
 * @macroname `SEG_ATTRIB_CODE32`
 *
 * @description
 * The attributes of a 32-bit code segment: granularity, default size, present,
 * execute and read.
 */
#define SEG_ATTRIB_CODE32       0xC9BU

/**
 * @macroname `SEG_ATTRIB_DATA32`
 *
 * @description
 * The attributes of a 32-bit data segment: granularity, big, present, read and
 * write.
 */
#define SEG_ATTRIB_DATA32       0xC93U

/**
 * @macroname `SEG_ATTRIB_LDT`
 *
 * @description
 * The attributes of the guest's LDT register at its start.
 */
#define SEG_ATTRIB_LDT          0x082U

/**
 * @macroname `SEG_ATTRIB_TSS_BUSY`
 *
 * @description
 * The attributes of the guest's task register at its start (a busy TSS).
 */
#define SEG_ATTRIB_TSS_BUSY     0x08BU

/**
 * @macroname `GUEST_PAT`
 *
 * @description
 * The value of the guest's PAT MSR at its start (the reset value).
 */
#define GUEST_PAT               0x0007040600070406UL

/**
 * @macroname `GUEST_DR6`
 *
 * @description
 * The value of the guest's DR6 at its start (the reset value).
 */
#define GUEST_DR6               0xFFFF0FF0UL

/**
 * @macroname `GUEST_DR7`
 *
 * @description
 * The value of the guest's DR7 at its start (the reset value).
 */
#define GUEST_DR7               0x400UL

/**
 * @macroname `GUEST_RFLAGS`
 *
 * @description
 * The value of the guest's RFLAGS at its start (only the reserved bit set).
 */
#define GUEST_RFLAGS            0x2UL

/** @} */

/**
 * @macroname `NB_LOGGED_MSRS`
 *
 * @description
 * The number of accesses to unknown MSRs that are logged.
 */
#define NB_LOGGED_MSRS          16U

/* MISRA C:2025 Dir 1.2 deviation: _Static_assert is a C11 keyword,
 * accepted by GCC in gnu99 mode; it checks the VMCB layout. */
_Static_assert(sizeof(vmcb_t) == VMCB_SIZE, "VMCB size");
_Static_assert(offsetof(vmcb_t, control.exit_code) == 0x070U,
                        "VMCB exit_code");
_Static_assert(offsetof(vmcb_t, control.nested_cr3) == 0x0B0U,
                        "VMCB nested_cr3");
_Static_assert(offsetof(vmcb_t, save.efer) == 0x4D0U, "VMCB efer");
_Static_assert(offsetof(vmcb_t, save.rip) == 0x578U, "VMCB rip");
_Static_assert(offsetof(vmcb_t, save.rax) == 0x5F8U, "VMCB rax");
_Static_assert(offsetof(vmcb_t, save.g_pat) == 0x668U, "VMCB g_pat");

/*
 * Run the guest whose VMCB is at physical address vmcb_pa until the next
 * #VMEXIT (svm_run.S). Physical interrupts which arrived meanwhile are
 * handled before it returns, with interrupts disabled again.
 */
/** \cond */
void svm_vmrun(hv_guest_regs_t *regs, uint64_t vmcb_pa);
/** \endcond */

#endif // HV_SVM_H
