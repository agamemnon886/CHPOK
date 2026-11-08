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
 * The vCPU of a CPU: the EL2 controls for running its guest, the guest's
 * state at power-on (as QEMU starts a kernel at EL1: MMU off, exceptions
 * masked, x0 the device tree), the run loop and the exits.
 *
 * Each CPU runs one vCPU, so the guest's EL1 registers stay in the CPU;
 * they are only set to their reset values when the VM (re)starts. The
 * guest has its CPU's virtual timer and counter, its FP/SIMD registers
 * (the hypervisor never uses them) and its debug registers; its physical
 * interrupts, SError and SMC exit to the hypervisor, WFI and WFE don't.
 */

#include <arch/internal.h>

// HCR_EL2: stage 2, interrupts and SError to EL2, SMC trapped, EL1 AArch64.
#define HCR_VM              (1UL << 0)
#define HCR_SWIO            (1UL << 1)
#define HCR_FMO             (1UL << 3)
#define HCR_IMO             (1UL << 4)
#define HCR_AMO             (1UL << 5)
#define HCR_TSC             (1UL << 19)
#define HCR_RW              (1UL << 31)
#define HCR_VALUE           (HCR_VM | HCR_SWIO | HCR_FMO | HCR_IMO \
                             | HCR_AMO | HCR_TSC | HCR_RW)

// VTCR_EL2: 32-bit guest-physical addresses from level 1, 4 KiB granule.
#define VTCR_T0SZ           32UL
#define VTCR_SL0_LEVEL1     (1UL << 6)
#define VTCR_IRGN0_WBWA     (1UL << 8)
#define VTCR_ORGN0_WBWA     (1UL << 10)
#define VTCR_SH0_INNER      (3UL << 12)
#define VTCR_PS_40BIT       (2UL << 16)
#define VTCR_RES1           (1UL << 31)
#define VTCR_VALUE          (VTCR_T0SZ | VTCR_SL0_LEVEL1 | VTCR_IRGN0_WBWA \
                             | VTCR_ORGN0_WBWA | VTCR_SH0_INNER \
                             | VTCR_PS_40BIT | VTCR_RES1)

#define CPTR_EL2_RES1       0x33FFUL        // FP/SIMD not trapped
#define CNTHCTL_EL1_ACCESS  3UL             // Physical counter and timer
#define PMCR_N_SHIFT        11U
#define PMCR_N_MASK         0x1FUL
#define VMPIDR_RES1         (1UL << 31)

#define SCTLR_EL1_RESET     0x30D00800UL    // RES1 bits: MMU, caches off
#define SPSR_EL1H_MASKED    0x3C5UL         // EL1h, D, A, I, F masked

// ESR_EL2 exception classes.
#define ESR_EC_SHIFT        26U
#define ESR_EC_MASK         0x3FUL
#define ESR_ISS_IMM16       0xFFFFUL
#define EC_WFX              0x01U
#define EC_HVC64            0x16U
#define EC_SMC64            0x17U
#define EC_IABT_LOW         0x20U
#define EC_DABT_LOW         0x24U

#define HPFAR_FIPA_SHIFT    4U
#define PAGE_OFFSET_MASK    0xFFFUL

/* MISRA C:2025 Dir 1.2 deviation: _Static_assert is a C11 keyword,
 * accepted by GCC in gnu99 mode; it checks the offsets vectors.S uses. */
_Static_assert(offsetof(hv_guest_regs_t, elr) == 248U, "regs elr");
_Static_assert(offsetof(hv_guest_regs_t, spsr) == 256U, "regs spsr");
_Static_assert(offsetof(hv_guest_regs_t, host_sp) == 264U,
               "regs host_sp");
_Static_assert(offsetof(hv_arch_vcpu_t, regs) == 0U, "vcpu regs");

/*
 * Prepare this CPU to run the guest of its vCPU: EL2 controls, the VM's
 * stage 2, the identity the guest sees (VPIDR, VMPIDR: the vCPU index)
 * and the interrupt controller.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (hv_arch_cpu_enable()): TPIDR_EL2
 * holds the address of the vCPU's registers for vectors.S, converted from
 * a pointer.
 */
void hv_arch_cpu_enable(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    uint64_t pmcr = HV_READ_SYSREG(pmcr_el0);

    HV_WRITE_SYSREG(tpidr_el2, (uint64_t) (uintptr_t) &vcpu->arch.regs);
    HV_WRITE_SYSREG(vtcr_el2, VTCR_VALUE);
    HV_WRITE_SYSREG(vttbr_el2, vcpu->vm->arch.vttbr);
    HV_WRITE_SYSREG(hcr_el2, HCR_VALUE);
    HV_WRITE_SYSREG(cptr_el2, CPTR_EL2_RES1);
    HV_WRITE_SYSREG(hstr_el2, 0U);
    // The guest has all the performance counters; debug stays at EL1.
    HV_WRITE_SYSREG(mdcr_el2, (pmcr >> PMCR_N_SHIFT) & PMCR_N_MASK);
    HV_WRITE_SYSREG(cnthctl_el2, CNTHCTL_EL1_ACCESS);
    HV_WRITE_SYSREG(vpidr_el2, HV_READ_SYSREG(midr_el1));
    HV_WRITE_SYSREG(vmpidr_el2, VMPIDR_RES1 | vcpu->index);
    arm64_isb();
    gic_init_cpu();
}

// EL1 registers as at reset (those a kernel may rely on, and the timers).
static void reset_el1(void)
{
    HV_WRITE_SYSREG(sctlr_el1, SCTLR_EL1_RESET);
    HV_WRITE_SYSREG(tcr_el1, 0U);
    HV_WRITE_SYSREG(ttbr0_el1, 0U);
    HV_WRITE_SYSREG(ttbr1_el1, 0U);
    HV_WRITE_SYSREG(mair_el1, 0U);
    HV_WRITE_SYSREG(amair_el1, 0U);
    HV_WRITE_SYSREG(vbar_el1, 0U);
    HV_WRITE_SYSREG(cpacr_el1, 0U);
    HV_WRITE_SYSREG(contextidr_el1, 0U);
    HV_WRITE_SYSREG(tpidr_el1, 0U);
    HV_WRITE_SYSREG(tpidr_el0, 0U);
    HV_WRITE_SYSREG(tpidrro_el0, 0U);
    HV_WRITE_SYSREG(sp_el0, 0U);
    HV_WRITE_SYSREG(sp_el1, 0U);
    HV_WRITE_SYSREG(elr_el1, 0U);
    HV_WRITE_SYSREG(spsr_el1, 0U);
    HV_WRITE_SYSREG(esr_el1, 0U);
    HV_WRITE_SYSREG(far_el1, 0U);
    HV_WRITE_SYSREG(afsr0_el1, 0U);
    HV_WRITE_SYSREG(afsr1_el1, 0U);
    HV_WRITE_SYSREG(par_el1, 0U);
    HV_WRITE_SYSREG(csselr_el1, 0U);
    HV_WRITE_SYSREG(mdscr_el1, 0U);
    HV_WRITE_SYSREG(cntkctl_el1, 0U);
    HV_WRITE_SYSREG(cntv_ctl_el0, 0U);
    HV_WRITE_SYSREG(cntv_cval_el0, 0U);
    HV_WRITE_SYSREG(cntp_ctl_el0, 0U);
    HV_WRITE_SYSREG(cntp_cval_el0, 0U);
}

/*
 * The guest's state at power-on: at the entry point at EL1 with the MMU
 * off and exceptions masked, x0 the device tree; EL1 registers and the
 * interrupt state reset, its virtual counter from 0, its old
 * translations gone.
 */
void hv_arch_vcpu_init(hv_vcpu_t *vcpu)
{
    hv_guest_regs_t *regs = &vcpu->arch.regs;
    uint64_t host_sp = regs->host_sp;

    hv_memset(regs, 0U, sizeof(*regs));
    regs->host_sp = host_sp;
    regs->x[0] = HV_GUEST_RAM;
    regs->elr = vcpu->vm->config->entry;
    regs->spsr = SPSR_EL1H_MASKED;

    reset_el1();
    gic_vcpu_reset();
    arm64_isb();
    HV_WRITE_SYSREG(cntvoff_el2, HV_READ_SYSREG(cntpct_el0));
    arm64_tlbi_vmalls12e1();
    arm64_dsb_ish();
    arm64_isb();
}

// Print the main registers of a vCPU (when its VM stops).
void hv_report_state(const hv_vcpu_t *vcpu)
{
    const hv_guest_regs_t *regs = &vcpu->arch.regs;
    const char *name = vcpu->vm->config->name;

    hv_printf("[hv] %s: PC 0x%lx PSTATE 0x%lx ESR 0x%lx FAR 0x%lx\n"
              "[hv] %s: SCTLR_EL1 0x%lx TTBR0_EL1 0x%lx VBAR_EL1 0x%lx\n",
              name, regs->elr, regs->spsr, HV_READ_SYSREG(esr_el2),
              HV_READ_SYSREG(far_el2), name, HV_READ_SYSREG(sctlr_el1),
              HV_READ_SYSREG(ttbr0_el1), HV_READ_SYSREG(vbar_el1));
}

// Report an instruction fetch of the guest outside its RAM.
static void report_fetch(const hv_vcpu_t *vcpu, uint64_t esr)
{
    uint64_t ipa = ((HV_READ_SYSREG(hpfar_el2) >> HPFAR_FIPA_SHIFT) << 12)
                   | (HV_READ_SYSREG(far_el2) & PAGE_OFFSET_MASK);

    hv_printf("[hv] %s: instruction fetch from guest-physical address "
              "0x%lx outside its RAM (ESR 0x%lx)\n", vcpu->vm->config->name,
              ipa, esr);
    hv_report_state(vcpu);
}

// A synchronous exception of the guest (ESR_EL2 esr).
static hv_action_t handle_sync(hv_vcpu_t *vcpu, uint64_t esr)
{
    hv_guest_regs_t *regs = &vcpu->arch.regs;
    const char *name = vcpu->vm->config->name;
    uint32_t ec = (uint32_t) ((esr >> ESR_EC_SHIFT) & ESR_EC_MASK);
    hv_action_t action = HV_CONTINUE;

    switch (ec) {
        case EC_HVC64:
            // ELR_EL2 is already past the hvc.
            action = psci_guest_call(vcpu, (uint32_t) (esr & ESR_ISS_IMM16));
            break;
        case EC_SMC64:
            regs->elr += 4U;    // A trapped smc: ELR_EL2 is the smc
            action = psci_guest_call(vcpu, (uint32_t) (esr & ESR_ISS_IMM16));
            break;
        case EC_DABT_LOW:
            action = mmio_abort(vcpu, esr);
            break;
        case EC_IABT_LOW:
            report_fetch(vcpu, esr);
            action = HV_STOP;
            break;
        case EC_WFX:
            regs->elr += 4U;    // Not trapped; skipped if it was
            break;
        default:
            hv_printf("[hv] %s: unexpected exception (ESR 0x%lx)\n", name,
                      esr);
            hv_report_state(vcpu);
            action = HV_STOP;
            break;
    }
    return action;
}

// Handle an exit of the guest of this CPU; returns what the loop does next.
static hv_action_t handle_exit(hv_cpu_t *cpu, uint64_t exit)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    const char *name = vcpu->vm->config->name;
    hv_action_t action = HV_CONTINUE;

    if (exit == HV_EXIT_SYNC) {
        action = handle_sync(vcpu, HV_READ_SYSREG(esr_el2));
    } else if (exit == HV_EXIT_IRQ) {
        gic_handle_irq(cpu);
    } else if (exit == HV_EXIT_FIQ) {
        if (!cpu->arch.fiq_logged) {
            cpu->arch.fiq_logged = true;
            hv_printf("[hv] CPU %u: unexpected FIQ\n", cpu->index);
        }
    } else if (exit == HV_EXIT_SERROR) {
        hv_printf("[hv] %s: SError (ESR 0x%lx)\n", name,
                  HV_READ_SYSREG(esr_el2));
        hv_report_state(vcpu);
        action = HV_STOP;
    } else {
        hv_printf("[hv] %s: exception from AArch32 code, which isn't "
                  "supported\n", name);
        hv_report_state(vcpu);
        action = HV_STOP;
    }
    return action;
}

/*
 * Run loop of a vCPU: run the guest, handle its exit; a reset reloads the
 * VM, a stop halts the CPU. Doesn't return.
 */
void hv_arch_vcpu_run(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    hv_vm_t *vm = vcpu->vm;

    while (true) {
        uint64_t exit = hv_guest_enter(&vcpu->arch.regs);
        hv_action_t action = handle_exit(cpu, exit);

        if (action == HV_RESET) {
            // As at power-on: the image is loaded again and restarts.
            vm->resets++;
            vm_print(vm, "reset");
            vm_load(vm);
            hv_arch_vcpu_init(vcpu);
        } else if (action == HV_STOP) {
            // Only this VM stops; the other CPUs go on.
            vm_print(vm, "stopped");
            hv_halt_forever();
        } else {
            // Run the guest again.
        }
    }
}
