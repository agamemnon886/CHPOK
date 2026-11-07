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
 * AMD SVM back end: enabling SVM on a CPU, the initial guest state (as a
 * Multiboot loader leaves it), the run loop of a vCPU and the handling of
 * #VMEXITs.
 *
 * QEMU's TCG emulates SVM with nested paging, but not the next-RIP save
 * or decode assists: the intercepted instructions are skipped by their
 * fixed lengths (IOIO exits give the next RIP), and nothing is decoded.
 */

#include <hv/hv.h>
#include <hv/x86.h>

#define CPUID_EXT_FEATURES      0x80000001U
#define CPUID_EXT_ECX_SVM       ((uint32_t) 1U << 2)
#define CPUID_SVM_FEATURES      0x8000000AU
#define CPUID_SVM_EDX_NPT       ((uint32_t) 1U << 0)

#define CPUID_1_ECX_MONITOR     ((uint32_t) 1U << 3)
#define CPUID_1_ECX_VMX         ((uint32_t) 1U << 5)
#define CPUID_1_ECX_X2APIC      ((uint32_t) 1U << 21)
#define CPUID_1_ECX_DEADLINE    ((uint32_t) 1U << 24)
#define CPUID_1_ECX_HYPERVISOR  ((uint32_t) 1U << 31)
#define CPUID_1_EDX_APIC        ((uint32_t) 1U << 9)
#define CPUID_EXT_ECX_SKINIT    ((uint32_t) 1U << 12)
#define CPUID_HV_BASE           0x40000000U
#define CPUID_HV_LAST           0x400000FFU

#define INSN_LEN_CPUID          2U
#define INSN_LEN_MSR            2U
#define INSN_LEN_HLT            1U

#define EXCEPTION_UD            6U

#define MULTIBOOT_BOOT_MAGIC    0x2BADB002U

#define GUEST_CODE_SELECTOR     0x08U
#define GUEST_DATA_SELECTOR     0x10U
#define SEG_ATTRIB_CODE32       0xC9BU  // G, D, P, code exec/read
#define SEG_ATTRIB_DATA32       0xC93U  // G, B, P, data read/write
#define SEG_ATTRIB_LDT          0x082U
#define SEG_ATTRIB_TSS_BUSY     0x08BU
#define GUEST_PAT               0x0007040600070406UL
#define GUEST_DR6               0xFFFF0FF0UL
#define GUEST_DR7               0x400UL
#define GUEST_RFLAGS            0x2UL

#define NB_LOGGED_MSRS          16U

static uint8_t msrpm[SVM_MSRPM_SIZE] HV_ALIGNED(4096);

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

// Physical address of a VMCB, save area or permission map (identity-mapped).
/*
 * MISRA C:2025 Rule 11.4 and Rule 11.6 deviation (svm_pa()): the VMCB,
 * the host save area and the permission maps are given to the CPU by
 * their physical addresses (identity-mapped), converted from pointers.
 */
static uint64_t svm_pa(const void *p)
{
    return (uint64_t) (uintptr_t) p;
}

/*
 * Check that the CPU has AMD SVM with nested paging, enabled by the firmware
 * (reports what is missing).
 */
bool svm_supported(void)
{
    uint32_t regs[4];
    bool svm;
    bool npt = false;

    // SVM in the extended features; NPT and VM_CR only exist with SVM.
    x86_cpuid(CPUID_EXT_FEATURES, 0U, regs);
    svm = (regs[2] & CPUID_EXT_ECX_SVM) != 0U;
    if (svm) {
        x86_cpuid(CPUID_SVM_FEATURES, 0U, regs);
        npt = (regs[3] & CPUID_SVM_EDX_NPT) != 0U;
        if ((x86_rdmsr(MSR_VM_CR) & VM_CR_SVMDIS) != 0U) {
            hv_printf("[hv] SVM is disabled by the firmware\n");
            svm = false;
        }
    }
    if (!svm) {
        hv_printf("[hv] the CPU has no AMD SVM\n");
    } else if (!npt) {
        hv_printf("[hv] the CPU has no nested paging (NPT)\n");
    } else {
        // Supported.
    }
    return svm && npt;
}

// Set up what all vCPUs share: the MSR permission map.
void svm_init(void)
{
    // Every MSR access exits (see handle_msr()).
    hv_memset(msrpm, 0xFFU, sizeof(msrpm));
}

// Enable SVM on this CPU and give its vCPU a VMCB and an ASID.
void svm_enable_cpu(hv_cpu_t *cpu)
{
    static vmcb_t vmcbs[HV_MAX_CPUS] HV_ALIGNED(4096);
    static uint8_t host_save_areas[HV_MAX_CPUS][HV_PAGE_SIZE]
        HV_ALIGNED(4096);
    hv_vcpu_t *vcpu = &cpu->vcpu;

    // VMRUN keeps the host state in this CPU's save area.
    x86_wrmsr(MSR_EFER, x86_rdmsr(MSR_EFER) | EFER_SVME);
    x86_wrmsr(MSR_VM_HSAVE_PA, svm_pa(host_save_areas[cpu->index]));
    vcpu->vmcb = &vmcbs[cpu->index];
    vcpu->vmcb_pa = svm_pa(vcpu->vmcb);
    vcpu->asid = cpu->index + 1U;   // ASID 0 is the host's
}

// Set a segment register of the save area (base 0).
static void set_segment(vmcb_segment_t *seg, uint16_t selector,
                        uint16_t attrib, uint32_t limit)
{
    seg->selector = selector;
    seg->attrib = attrib;
    seg->limit = limit;
    seg->base = 0U;
}

/*
 * State after a Multiboot loader: 32-bit protected mode with flat
 * segments, paging off, interrupts off, EAX the Multiboot magic and EBX
 * the address of the Multiboot information.
 */
void svm_vcpu_init(hv_vcpu_t *vcpu)
{
    vmcb_t *vmcb = vcpu->vmcb;
    vmcb_control_t *c = &vmcb->control;
    vmcb_save_t *s = &vmcb->save;
    const hv_vm_t *vm = vcpu->vm;

    hv_memset(vmcb, 0U, sizeof(*vmcb));

    /*
     * Exits: physical interrupts, the instructions the hypervisor
     * emulates (CPUID, HLT, port I/O, MSRs), triple faults, and the SVM
     * and MONITOR/MWAIT instructions, which the guest doesn't get.
     */
    c->intercept_misc1 = SVM_INTERCEPT_INTR | SVM_INTERCEPT_CPUID
                         | SVM_INTERCEPT_HLT | SVM_INTERCEPT_INVLPGA
                         | SVM_INTERCEPT_IOIO | SVM_INTERCEPT_MSR
                         | SVM_INTERCEPT_SHUTDOWN;
    c->intercept_misc2 = SVM_INTERCEPT_VMRUN | SVM_INTERCEPT_VMMCALL
                         | SVM_INTERCEPT_VMLOAD | SVM_INTERCEPT_VMSAVE
                         | SVM_INTERCEPT_STGI | SVM_INTERCEPT_CLGI
                         | SVM_INTERCEPT_SKINIT | SVM_INTERCEPT_MONITOR
                         | SVM_INTERCEPT_MWAIT;
    c->iopm_base_pa = vm->iopm_pa;     // Per VM: its serial ports differ
    c->msrpm_base_pa = svm_pa(msrpm);
    c->tsc_offset = 0U - x86_rdtsc();   // The guest's TSC starts at 0
    c->guest_asid = vcpu->asid;
    // The guest's IF masks only virtual interrupts (V_IRQ, any priority).
    c->int_ctl = SVM_INT_CTL_V_INTR_MASKING | SVM_INT_CTL_V_INTR_PRIO
                 | SVM_INT_CTL_V_IGN_TPR;
    // Guest-physical memory is the VM's RAM (npt.c).
    c->nested_ctl = SVM_NESTED_NP_ENABLE;
    c->nested_cr3 = vm->npt_root;

    // Flat 4 GiB segments, as a Multiboot loader leaves them.
    set_segment(&s->cs, GUEST_CODE_SELECTOR, SEG_ATTRIB_CODE32,
                0xFFFFFFFFU);
    set_segment(&s->ds, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    set_segment(&s->es, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    set_segment(&s->fs, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    set_segment(&s->gs, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    set_segment(&s->ss, GUEST_DATA_SELECTOR, SEG_ATTRIB_DATA32,
                0xFFFFFFFFU);
    // No GDT yet: the guest loads its own before it reloads a segment.
    set_segment(&s->gdtr, 0U, 0U, 0U);
    set_segment(&s->idtr, 0U, 0U, 0x3FFU);
    set_segment(&s->ldtr, 0U, SEG_ATTRIB_LDT, 0xFFFFU);
    set_segment(&s->tr, 0U, SEG_ATTRIB_TSS_BUSY, 0xFFFFU);
    // Ring 0, protected mode without paging; VMRUN requires EFER.SVME.
    s->cpl = 0U;
    s->efer = EFER_SVME;
    s->cr0 = X86_CR0_PE | X86_CR0_ET;
    s->cr3 = 0U;
    s->cr4 = 0U;
    s->dr6 = GUEST_DR6;
    s->dr7 = GUEST_DR7;
    s->rflags = GUEST_RFLAGS;
    s->rip = vm->config->entry;
    s->rsp = 0U;
    s->rax = MULTIBOOT_BOOT_MAGIC;
    s->g_pat = GUEST_PAT;           // Power-on PAT

    // EBX: the Multiboot information written by vm_load().
    hv_memset(&vcpu->regs, 0U, sizeof(vcpu->regs));
    vcpu->regs.rbx = HV_GUEST_MBI;
    vcpu->flush_tlb = true;
}

// Skip the intercepted instruction (length bytes) and end its interrupt shadow.
static void skip_instruction(vmcb_t *vmcb, uint64_t length)
{
    vmcb->save.rip += length;
    vmcb->control.int_state &= ~SVM_INT_STATE_SHADOW;
}

/*
 * Make the guest take exception vector (without an error code) at the next
 * VMRUN.
 */
static void inject_exception(vmcb_t *vmcb, uint32_t vector)
{
    vmcb->control.event_inj = SVM_EVENT_VALID | SVM_EVENT_TYPE_EXCEPT
                              | (uint64_t) vector;
}

// Turn the LAPIC timer ticks counted on this CPU into virtual PIT IRQs.
static void deliver_ticks(hv_cpu_t *cpu, hv_vm_t *vm)
{
    // Interrupts are disabled here, so the counter doesn't change meanwhile.
    if (cpu->timer_ticks != 0U) {
        // Missed ticks are dropped: JetOS keeps its time with the TSC.
        cpu->timer_ticks = 0U;
        if (vpit_timer_running(&vm->pit)) {
            vpic_raise(&vm->pic, 0U);
        }
    }
}

/*
 * Request the next virtual PIC interrupt with V_IRQ: the CPU delivers it
 * when the guest can take it (RFLAGS.IF set, no interrupt shadow).
 */
static void inject_interrupt(hv_vcpu_t *vcpu)
{
    vmcb_control_t *c = &vcpu->vmcb->control;
    uint8_t vector;

    // One interrupt at a time: the next one after this one is taken.
    if (((c->int_ctl & SVM_INT_CTL_V_IRQ) == 0U)
        && vpic_next(&vcpu->vm->pic, &vector)) {
        c->int_vector = vector;
        c->int_ctl |= SVM_INT_CTL_V_IRQ;
    }
}

/*
 * Check whether the guest has an interrupt to take (requested with V_IRQ or
 * pending in the virtual PIC).
 */
static bool interrupt_ready(const hv_vcpu_t *vcpu)
{
    return ((vcpu->vmcb->control.int_ctl & SVM_INT_CTL_V_IRQ) != 0U)
           || vpic_pending(&vcpu->vm->pic);
}

/*
 * HLT exit: wait until the guest has an interrupt to take. A halt with
 * interrupts disabled stops the VM.
 */
static hv_action_t handle_hlt(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    vmcb_t *vmcb = vcpu->vmcb;
    hv_action_t action = HV_CONTINUE;

    skip_instruction(vmcb, INSN_LEN_HLT);
    if ((vmcb->save.rflags & X86_RFLAGS_IF) == 0U) {
        vm_print(vcpu->vm, "halted with interrupts disabled");
        action = HV_STOP;
    } else {
        // Sleep until a timer tick gives the guest an interrupt.
        while (!interrupt_ready(vcpu)) {
            x86_wait_interrupt();
            deliver_ticks(cpu, vcpu->vm);
        }
    }
    return action;
}

// Mask of the bits of an I/O access of size 1, 2 or 4 bytes.
static uint64_t io_mask(uint32_t size)
{
    uint64_t mask;

    if (size == 1U) {
        mask = 0xFFU;
    } else if (size == 2U) {
        mask = 0xFFFFU;
    } else {
        mask = 0xFFFFFFFFU;
    }
    return mask;
}

/*
 * IOIO exit: perform the port access (ioport.c), put the value read into RAX
 * and go on after the instruction. String I/O stops the VM.
 */
static hv_action_t handle_io(hv_vcpu_t *vcpu)
{
    vmcb_t *vmcb = vcpu->vmcb;
    uint64_t info = vmcb->control.exit_info1;
    uint16_t port = (uint16_t) (info >> SVM_IOIO_PORT_SHIFT);
    bool is_in = (info & SVM_IOIO_IN) != 0U;
    uint32_t size = 4U;
    uint32_t value = (uint32_t) vmcb->save.rax;
    hv_action_t action;

    // EXITINFO1: port, direction, size; EXITINFO2: the next RIP.
    if ((info & SVM_IOIO_SZ8) != 0U) {
        size = 1U;
    } else if ((info & SVM_IOIO_SZ16) != 0U) {
        size = 2U;
    } else {
        // 32-bit access.
    }

    if ((info & (SVM_IOIO_STR | SVM_IOIO_REP)) != 0U) {
        hv_printf("[hv] %s: string I/O on port 0x%x is not supported\n",
                  vcpu->vm->config->name, (uint32_t) port);
        action = HV_STOP;
    } else {
        value &= (uint32_t) io_mask(size);
        action = io_access(vcpu->vm, port, size, is_in, &value);
        if (is_in) {
            uint64_t mask = io_mask(size);

            if (size == 4U) {
                // 32-bit results are zero-extended.
                vmcb->save.rax = (uint64_t) value;
            } else {
                // 8- and 16-bit results replace only AL or AX.
                vmcb->save.rax = (vmcb->save.rax & ~mask)
                                 | ((uint64_t) value & mask);
            }
        }
        vmcb->save.rip = vmcb->control.exit_info2;
        vmcb->control.int_state &= ~SVM_INT_STATE_SHADOW;
    }
    return action;
}

/*
 * CPUID exit: the host's values, without SVM, the APIC, MONITOR and the
 * topology, with one CPU and the hypervisor vendor leaves.
 */
static void handle_cpuid(hv_vcpu_t *vcpu)
{
    vmcb_t *vmcb = vcpu->vmcb;
    uint32_t leaf = (uint32_t) vmcb->save.rax;
    uint32_t subleaf = (uint32_t) vcpu->regs.rcx;
    uint32_t r[4];

    // Start from the host's values and change what the guest must not see.
    x86_cpuid(leaf, subleaf, r);
    if (leaf == 1U) {
        // One logical CPU, APIC ID = vCPU number; no APIC, no VMX.
        r[1] = (r[1] & 0x0000FFFFU) | ((uint32_t) 1U << 16)
               | (vcpu->index << 24);
        r[2] &= ~(CPUID_1_ECX_MONITOR | CPUID_1_ECX_VMX
                  | CPUID_1_ECX_X2APIC | CPUID_1_ECX_DEADLINE);
        r[2] |= CPUID_1_ECX_HYPERVISOR;
        r[3] &= ~CPUID_1_EDX_APIC;
    } else if (leaf == 4U) {
        r[0] &= 0x00003FFFU;    // One core, no shared threads
    } else if ((leaf == 0xBU) || (leaf == 0x1FU)
               || (leaf == CPUID_SVM_FEATURES)) {
        r[0] = 0U;              // No topology, no SVM
        r[1] = 0U;
        r[2] = 0U;
        r[3] = 0U;
    } else if (leaf == CPUID_EXT_FEATURES) {
        r[2] &= ~(CPUID_EXT_ECX_SVM | CPUID_EXT_ECX_SKINIT);   // No nesting
    } else if (leaf == CPUID_HV_BASE) {
        r[0] = CPUID_HV_BASE;   // Vendor "JetOS HV    "
        r[1] = 0x4F74654AU;
        r[2] = 0x56482053U;
        r[3] = 0x20202020U;
    } else if ((leaf > CPUID_HV_BASE) && (leaf <= CPUID_HV_LAST)) {
        // No other hypervisor leaves.
        r[0] = 0U;
        r[1] = 0U;
        r[2] = 0U;
        r[3] = 0U;
    } else {
        // Other leaves are passed through.
    }
    // The results go to EAX-EDX (zero-extended to 64 bits).
    vmcb->save.rax = r[0];
    vcpu->regs.rbx = r[1];
    vcpu->regs.rcx = r[2];
    vcpu->regs.rdx = r[3];
    skip_instruction(vmcb, INSN_LEN_CPUID);
}

// Report an MSR access that is ignored, once per MSR (up to a limit).
static void log_msr(const hv_vcpu_t *vcpu, uint32_t msr, bool write)
{
    static uint32_t logged[NB_LOGGED_MSRS];
    static uint32_t nb_logged = 0U;
    bool found = false;

    for (uint32_t i = 0U; (i < nb_logged) && !found; i++) {
        found = (logged[i] == msr);
    }
    // Report each MSR once; beyond the table, nothing more is reported.
    if (!found && (nb_logged < NB_LOGGED_MSRS)) {
        logged[nb_logged] = msr;
        nb_logged++;
        hv_printf("[hv] %s: %s of MSR 0x%x ignored\n",
                  vcpu->vm->config->name, write ? "write" : "read", msr);
    }
}

// Guest MSRs kept in the VMCB; NULL for the others.
static uint64_t *vmcb_msr(vmcb_save_t *s, uint32_t msr)
{
    uint64_t *field;

    switch (msr) {
        case MSR_PAT:
            field = &s->g_pat;
            break;
        case MSR_STAR:
            field = &s->star;
            break;
        case MSR_LSTAR:
            field = &s->lstar;
            break;
        case MSR_CSTAR:
            field = &s->cstar;
            break;
        case MSR_SFMASK:
            field = &s->sfmask;
            break;
        case MSR_KERNEL_GS_BASE:
            field = &s->kernel_gs_base;
            break;
        case MSR_FS_BASE:
            field = &s->fs.base;
            break;
        case MSR_GS_BASE:
            field = &s->gs.base;
            break;
        case MSR_SYSENTER_CS:
            field = &s->sysenter_cs;
            break;
        case MSR_SYSENTER_ESP:
            field = &s->sysenter_esp;
            break;
        case MSR_SYSENTER_EIP:
            field = &s->sysenter_eip;
            break;
        default:
            field = NULL;
            break;
    }
    return field;
}

/*
 * RDMSR/WRMSR exit: EFER (SVME hidden), the TSC (through the offset), the MSRs
 * kept in the VMCB; others read 0 and ignore writes.
 */
static void handle_msr(hv_vcpu_t *vcpu)
{
    vmcb_t *vmcb = vcpu->vmcb;
    uint32_t msr = (uint32_t) vcpu->regs.rcx;
    bool write = vmcb->control.exit_info1 == 1U;   // 0: RDMSR, 1: WRMSR
    // WRMSR writes EDX:EAX.
    uint64_t value = (vcpu->regs.rdx << 32)
                     | (vmcb->save.rax & 0xFFFFFFFFU);
    uint64_t *field = vmcb_msr(&vmcb->save, msr);

    if (msr == MSR_EFER) {
        // SVME stays set in the guest's EFER, but is hidden.
        if (write) {
            vmcb->save.efer = value | EFER_SVME;
        } else {
            value = vmcb->save.efer & ~EFER_SVME;
        }
    } else if (msr == MSR_TSC) {
        // The guest's TSC is the host's plus the VMCB offset.
        if (write) {
            vmcb->control.tsc_offset = value - x86_rdtsc();
        } else {
            value = x86_rdtsc() + vmcb->control.tsc_offset;
        }
    } else if (field != NULL) {
        // VMLOAD/VMRUN load the field into the CPU at the next VMRUN.
        if (write) {
            *field = value;
        } else {
            value = *field;
        }
    } else {
        log_msr(vcpu, msr, write);
        value = 0U;
    }
    // RDMSR returns EDX:EAX.
    if (!write) {
        vmcb->save.rax = value & 0xFFFFFFFFU;
        vcpu->regs.rdx = value >> 32;
    }
    skip_instruction(vmcb, INSN_LEN_MSR);
}

// Print the main registers of a vCPU (when its VM stops).
static void report_state(const hv_vcpu_t *vcpu)
{
    const vmcb_save_t *s = &vcpu->vmcb->save;

    hv_printf("[hv] %s: RIP 0x%lx RSP 0x%lx RFLAGS 0x%lx CS 0x%x\n"
              "[hv] %s: CR0 0x%lx CR3 0x%lx CR4 0x%lx EFER 0x%lx\n",
              vcpu->vm->config->name, s->rip, s->rsp, s->rflags,
              (uint32_t) s->cs.selector, vcpu->vm->config->name, s->cr0,
              s->cr3, s->cr4, s->efer);
}

// Handle the #VMEXIT of a vCPU; returns what the run loop does next.
static hv_action_t handle_exit(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    vmcb_t *vmcb = vcpu->vmcb;
    vmcb_control_t *c = &vmcb->control;
    const char *name = vcpu->vm->config->name;
    hv_action_t action = HV_CONTINUE;

    // An event whose delivery was interrupted is delivered again.
    if ((c->exit_int_info & SVM_EVENT_VALID) != 0U) {
        c->event_inj = c->exit_int_info;
    }

    switch (c->exit_code) {
        case SVM_EXIT_INTR:
            // Handled by the host when svm_vmrun() set GIF.
            break;
        case SVM_EXIT_HLT:
            action = handle_hlt(cpu);
            break;
        case SVM_EXIT_IOIO:
            action = handle_io(vcpu);
            break;
        case SVM_EXIT_CPUID:
            handle_cpuid(vcpu);
            break;
        case SVM_EXIT_MSR:
            handle_msr(vcpu);
            break;
        case SVM_EXIT_VMRUN:
        case SVM_EXIT_VMMCALL:
        case SVM_EXIT_VMLOAD:
        case SVM_EXIT_VMSAVE:
        case SVM_EXIT_STGI:
        case SVM_EXIT_CLGI:
        case SVM_EXIT_SKINIT:
        case SVM_EXIT_INVLPGA:
        case SVM_EXIT_MONITOR:
        case SVM_EXIT_MWAIT:
            inject_exception(vmcb, EXCEPTION_UD);
            break;
        case SVM_EXIT_NPF:
            hv_printf("[hv] %s: access to guest-physical address 0x%lx "
                      "outside its RAM (error code 0x%lx)\n", name,
                      c->exit_info2, c->exit_info1);
            report_state(vcpu);
            action = HV_STOP;
            break;
        case SVM_EXIT_SHUTDOWN:
            hv_printf("[hv] %s: shutdown (triple fault)\n", name);
            report_state(vcpu);
            action = HV_STOP;
            break;
        case SVM_EXIT_INVALID:
            hv_printf("[hv] %s: invalid guest state\n", name);
            report_state(vcpu);
            action = HV_STOP;
            break;
        default:
            hv_printf("[hv] %s: unexpected exit 0x%lx (0x%lx, 0x%lx)\n",
                      name, c->exit_code, c->exit_info1, c->exit_info2);
            report_state(vcpu);
            action = HV_STOP;
            break;
    }
    return action;
}

/*
 * Run loop of a vCPU: deliver the virtual timer ticks and interrupts, run the
 * guest, handle its exit; a reset reloads the VM, a stop halts the CPU. Doesn't
 * return.
 */
void svm_vcpu_run(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    hv_vm_t *vm = vcpu->vm;

    while (true) {
        vmcb_control_t *c = &vcpu->vmcb->control;
        hv_action_t action;

        // Timer ticks become IRQ 0, the next PIC interrupt goes to V_IRQ.
        deliver_ticks(cpu, vm);
        inject_interrupt(vcpu);
        // The guest's TLB is flushed after a (re)start only.
        c->tlb_control = vcpu->flush_tlb ? SVM_TLB_FLUSH_ALL
                                         : SVM_TLB_DO_NOTHING;
        vcpu->flush_tlb = false;

        svm_vmrun(&vcpu->regs, vcpu->vmcb_pa);

        // An injected event was delivered (or is in EXITINTINFO).
        c->event_inj = 0U;
        action = handle_exit(cpu);
        if (action == HV_RESET) {
            // As at power-on: the image is loaded again and restarts.
            vm->resets++;
            vm_print(vm, "reset");
            vm_load(vm);
            svm_vcpu_init(vcpu);
        } else if (action == HV_STOP) {
            // Only this VM stops; the other CPUs go on.
            lapic_timer_stop();
            vm_print(vm, "stopped");
            hv_halt_forever();
        } else {
            // Run the guest again.
        }
    }
}
