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
 * AMD SVM: the virtual machine control block (VMCB), its intercepts and
 * exit codes (AMD64 Architecture Programmer's Manual, volume 2, appendix
 * B). The layout is checked with static assertions in svm.c.
 */

#ifndef HV_SVM_H
#define HV_SVM_H

#include <stdint.h>

#define VMCB_SIZE       4096U

// intercept_misc1
#define SVM_INTERCEPT_INTR      ((uint32_t) 1U << 0)
#define SVM_INTERCEPT_CPUID     ((uint32_t) 1U << 18)
#define SVM_INTERCEPT_HLT       ((uint32_t) 1U << 24)
#define SVM_INTERCEPT_INVLPGA   ((uint32_t) 1U << 26)
#define SVM_INTERCEPT_IOIO      ((uint32_t) 1U << 27)
#define SVM_INTERCEPT_MSR       ((uint32_t) 1U << 28)
#define SVM_INTERCEPT_SHUTDOWN  ((uint32_t) 1U << 31)

// intercept_misc2
#define SVM_INTERCEPT_VMRUN     ((uint32_t) 1U << 0)
#define SVM_INTERCEPT_VMMCALL   ((uint32_t) 1U << 1)
#define SVM_INTERCEPT_VMLOAD    ((uint32_t) 1U << 2)
#define SVM_INTERCEPT_VMSAVE    ((uint32_t) 1U << 3)
#define SVM_INTERCEPT_STGI      ((uint32_t) 1U << 4)
#define SVM_INTERCEPT_CLGI      ((uint32_t) 1U << 5)
#define SVM_INTERCEPT_SKINIT    ((uint32_t) 1U << 6)
#define SVM_INTERCEPT_MONITOR   ((uint32_t) 1U << 10)
#define SVM_INTERCEPT_MWAIT     ((uint32_t) 1U << 11)

// int_ctl
#define SVM_INT_CTL_V_IRQ           ((uint32_t) 1U << 8)
#define SVM_INT_CTL_V_INTR_PRIO     ((uint32_t) 0xFU << 16)
#define SVM_INT_CTL_V_IGN_TPR       ((uint32_t) 1U << 20)
#define SVM_INT_CTL_V_INTR_MASKING  ((uint32_t) 1U << 24)

// int_state
#define SVM_INT_STATE_SHADOW        ((uint32_t) 1U << 0)

// tlb_control
#define SVM_TLB_DO_NOTHING      0U
#define SVM_TLB_FLUSH_ALL       1U

// nested_ctl
#define SVM_NESTED_NP_ENABLE    1U

// event_inj
#define SVM_EVENT_VALID         (1UL << 31)
#define SVM_EVENT_TYPE_EXCEPT   (3UL << 8)

// Exit codes.
#define SVM_EXIT_EXCP_BASE      0x040U
#define SVM_EXIT_INTR           0x060U
#define SVM_EXIT_NMI            0x061U
#define SVM_EXIT_CPUID          0x072U
#define SVM_EXIT_HLT            0x078U
#define SVM_EXIT_INVLPGA        0x07AU
#define SVM_EXIT_IOIO           0x07BU
#define SVM_EXIT_MSR            0x07CU
#define SVM_EXIT_SHUTDOWN       0x07FU
#define SVM_EXIT_VMRUN          0x080U
#define SVM_EXIT_VMMCALL        0x081U
#define SVM_EXIT_VMLOAD         0x082U
#define SVM_EXIT_VMSAVE         0x083U
#define SVM_EXIT_STGI           0x084U
#define SVM_EXIT_CLGI           0x085U
#define SVM_EXIT_SKINIT         0x086U
#define SVM_EXIT_MONITOR        0x08AU
#define SVM_EXIT_MWAIT          0x08BU
#define SVM_EXIT_NPF            0x400U
#define SVM_EXIT_INVALID        0xFFFFFFFFFFFFFFFFUL

// EXITINFO1 of IOIO exits.
#define SVM_IOIO_IN             (1UL << 0)
#define SVM_IOIO_STR            (1UL << 2)
#define SVM_IOIO_REP            (1UL << 3)
#define SVM_IOIO_SZ8            (1UL << 4)
#define SVM_IOIO_SZ16           (1UL << 5)
#define SVM_IOIO_SZ32           (1UL << 6)
#define SVM_IOIO_PORT_SHIFT     16U

// Size of the I/O permission map (12 KiB) and MSR permission map (8 KiB).
#define SVM_IOPM_SIZE           0x3000U
#define SVM_MSRPM_SIZE          0x2000U

// Segment register in the save area.
typedef struct {
    uint16_t selector;
    uint16_t attrib;
    uint32_t limit;
    uint64_t base;
} vmcb_segment_t;

// Control area: VMCB offsets 0x000-0x3FF.
typedef struct {
    uint16_t intercept_cr_read;         // 0x000
    uint16_t intercept_cr_write;        // 0x002
    uint16_t intercept_dr_read;         // 0x004
    uint16_t intercept_dr_write;        // 0x006
    uint32_t intercept_exceptions;      // 0x008
    uint32_t intercept_misc1;           // 0x00C
    uint32_t intercept_misc2;           // 0x010
    uint8_t reserved_014[0x03CU - 0x014U];
    uint16_t pause_filter_thresh;       // 0x03C
    uint16_t pause_filter_count;        // 0x03E
    uint64_t iopm_base_pa;              // 0x040
    uint64_t msrpm_base_pa;             // 0x048
    uint64_t tsc_offset;                // 0x050
    uint32_t guest_asid;                // 0x058
    uint8_t tlb_control;                // 0x05C
    uint8_t reserved_05d[3];
    uint32_t int_ctl;                   // 0x060
    uint32_t int_vector;                // 0x064
    uint32_t int_state;                 // 0x068
    uint8_t reserved_06c[4];
    uint64_t exit_code;                 // 0x070
    uint64_t exit_info1;                // 0x078
    uint64_t exit_info2;                // 0x080
    uint64_t exit_int_info;             // 0x088
    uint64_t nested_ctl;                // 0x090
    uint64_t avic_apic_bar;             // 0x098
    uint64_t ghcb_pa;                   // 0x0A0
    uint64_t event_inj;                 // 0x0A8
    uint64_t nested_cr3;                // 0x0B0
    uint64_t virt_ext;                  // 0x0B8
    uint32_t clean_bits;                // 0x0C0
    uint8_t reserved_0c4[4];
    uint64_t next_rip;                  // 0x0C8
    uint8_t reserved_0d0[0x400U - 0x0D0U];
} vmcb_control_t;

/*
 * State save area: VMCB offsets 0x400-0xFFF. The comments give the VMCB
 * offsets; the array sizes are offsets from the start of this area.
 */
typedef struct {
    vmcb_segment_t es;             // 0x400
    vmcb_segment_t cs;             // 0x410
    vmcb_segment_t ss;             // 0x420
    vmcb_segment_t ds;             // 0x430
    vmcb_segment_t fs;             // 0x440
    vmcb_segment_t gs;             // 0x450
    vmcb_segment_t gdtr;           // 0x460
    vmcb_segment_t ldtr;           // 0x470
    vmcb_segment_t idtr;           // 0x480
    vmcb_segment_t tr;             // 0x490
    uint8_t reserved_4a0[0x0CBU - 0x0A0U];
    uint8_t cpl;                        // 0x4CB
    uint8_t reserved_4cc[4];
    uint64_t efer;                      // 0x4D0
    uint8_t reserved_4d8[0x148U - 0x0D8U];
    uint64_t cr4;                       // 0x548
    uint64_t cr3;                       // 0x550
    uint64_t cr0;                       // 0x558
    uint64_t dr7;                       // 0x560
    uint64_t dr6;                       // 0x568
    uint64_t rflags;                    // 0x570
    uint64_t rip;                       // 0x578
    uint8_t reserved_580[0x1D8U - 0x180U];
    uint64_t rsp;                       // 0x5D8
    uint8_t reserved_5e0[0x1F8U - 0x1E0U];
    uint64_t rax;                       // 0x5F8
    uint64_t star;                      // 0x600
    uint64_t lstar;                     // 0x608
    uint64_t cstar;                     // 0x610
    uint64_t sfmask;                    // 0x618
    uint64_t kernel_gs_base;            // 0x620
    uint64_t sysenter_cs;               // 0x628
    uint64_t sysenter_esp;              // 0x630
    uint64_t sysenter_eip;              // 0x638
    uint64_t cr2;                       // 0x640
    uint8_t reserved_648[0x268U - 0x248U];
    uint64_t g_pat;                     // 0x668
    uint64_t dbgctl;                    // 0x670
    uint8_t reserved_678[0xC00U - 0x278U];
} vmcb_save_t;

typedef struct {
    vmcb_control_t control;
    vmcb_save_t save;
} vmcb_t;

/*
 * General-purpose registers of a guest which VMRUN doesn't keep in the
 * VMCB (RAX, RSP and RIP are in the save area). The offsets are used by
 * svm_run.S.
 */
typedef struct {
    uint64_t rbx;       // 0x00
    uint64_t rcx;       // 0x08
    uint64_t rdx;       // 0x10
    uint64_t rsi;       // 0x18
    uint64_t rdi;       // 0x20
    uint64_t rbp;       // 0x28
    uint64_t r8;        // 0x30
    uint64_t r9;        // 0x38
    uint64_t r10;       // 0x40
    uint64_t r11;       // 0x48
    uint64_t r12;       // 0x50
    uint64_t r13;       // 0x58
    uint64_t r14;       // 0x60
    uint64_t r15;       // 0x68
} hv_guest_regs_t;

/*
 * Run the guest whose VMCB is at physical address vmcb_pa until the next
 * #VMEXIT (svm_run.S). Physical interrupts which arrived meanwhile are
 * handled before it returns, with interrupts disabled again.
 */
void svm_vmrun(hv_guest_regs_t *regs, uint64_t vmcb_pa);

#endif // HV_SVM_H
