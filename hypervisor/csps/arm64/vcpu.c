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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file and
 * its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * hv_action_e (was hv_action_t); the macros, types and declarations are in
 * arch/internal.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/arm64/vcpu.c (the code of the processor architecture is in
 * hypervisor/csps); system registers are accessed with read_sysreg(),
 * write_sysreg() and isb(), shared with the kernel; memset() of the library
 * shared with the kernel replaces hv_memset().
 */

/**
 * @file
 * The vCPU of a CPU: the EL2 controls for running its guest, the guest's
 * state at power-on (as QEMU starts a kernel at EL1: MMU off, exceptions
 * masked, x0 the device tree), the run loop and the exits.
 *
 * Each CPU runs one vCPU, so the guest's EL1 registers stay in the CPU; they
 * are only set to their reset values when the VM (re)starts. The guest has
 * its CPU's virtual timer and counter, its FP/SIMD registers (the hypervisor
 * never uses them) and its debug registers; its physical interrupts, SError
 * and SMC exit to the hypervisor, WFI and WFE don't.
 */

#include <arch/internal.h>

/**
 * @funcname `hv_arch_cpu_enable`
 *
 * @description
 * Prepares this CPU to run the guest of its vCPU: the EL2 controls, the VM's
 * stage 2, the identity the guest sees and the interrupt controller.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in] The CPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | The CPU's vCPU |
 * | `pmcr` | `uint64_t` | PMCR_EL0: the number of performance counters |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `vcpu` to the address of `cpu->vcpu`.
 * 2. Set `pmcr` to `read_sysreg(pmcr_el0)`.
 * 3. Use the macro `write_sysreg()` with the arguments `tpidr_el2` and
 *    `(uint64_t) (uintptr_t) &vcpu->arch.regs`.
 * 4. Use the macro `write_sysreg()` with the arguments `vtcr_el2` and
 *    `VTCR_VALUE`.
 * 5. Use the macro `write_sysreg()` with the arguments `vttbr_el2` and
 *    `vcpu->vm->arch.vttbr`.
 * 6. Use the macro `write_sysreg()` with the arguments `hcr_el2` and
 *    `HCR_VALUE`.
 * 7. Use the macro `write_sysreg()` with the arguments `cptr_el2` and
 *    `CPTR_EL2_RES1`.
 * 8. Use the macro `write_sysreg()` with the arguments `hstr_el2` and `0U`.
 * 9. Use the macro `write_sysreg()` with the arguments `mdcr_el2` and
 *    `(pmcr >> PMCR_N_SHIFT) & PMCR_N_MASK`.
 * 10. Use the macro `write_sysreg()` with the arguments `cnthctl_el2` and
 *     `CNTHCTL_EL1_ACCESS`.
 * 11. Use the macro `write_sysreg()` with the arguments `vpidr_el2` and
 *     `read_sysreg(midr_el1)`.
 * 12. Use the macro `write_sysreg()` with the arguments `vmpidr_el2` and
 *     `VMPIDR_RES1 | vcpu->index`.
 * 13. Call `isb()`.
 * 14. Call `gic_init_cpu()`.
 *
 * @calls
 * - `isb()`
 * - `gic_init_cpu()`
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (hv_arch_cpu_enable()): TPIDR_EL2
 * holds the address of the vCPU's registers for vectors.S, converted from
 * a pointer.
 */
void hv_arch_cpu_enable(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    uint64_t pmcr = read_sysreg(pmcr_el0);

    write_sysreg(tpidr_el2, (uint64_t) (uintptr_t) &vcpu->arch.regs);
    write_sysreg(vtcr_el2, VTCR_VALUE);
    write_sysreg(vttbr_el2, vcpu->vm->arch.vttbr);
    write_sysreg(hcr_el2, HCR_VALUE);
    write_sysreg(cptr_el2, CPTR_EL2_RES1);
    write_sysreg(hstr_el2, 0U);
    // The guest has all the performance counters; debug stays at EL1.
    write_sysreg(mdcr_el2, (pmcr >> PMCR_N_SHIFT) & PMCR_N_MASK);
    write_sysreg(cnthctl_el2, CNTHCTL_EL1_ACCESS);
    write_sysreg(vpidr_el2, read_sysreg(midr_el1));
    write_sysreg(vmpidr_el2, VMPIDR_RES1 | vcpu->index);
    isb();
    gic_init_cpu();
}

/**
 * @funcname `reset_el1`
 *
 * @description
 * Sets the guest's EL1 registers (those a kernel may rely on, and the timers)
 * to their reset values.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Use the macro `write_sysreg()` with the arguments `sctlr_el1` and
 *    `SCTLR_EL1_RESET`.
 * 2. Use the macro `write_sysreg()` with the arguments `tcr_el1` and `0U`.
 * 3. Use the macro `write_sysreg()` with the arguments `ttbr0_el1` and `0U`.
 * 4. Use the macro `write_sysreg()` with the arguments `ttbr1_el1` and `0U`.
 * 5. Use the macro `write_sysreg()` with the arguments `mair_el1` and `0U`.
 * 6. Use the macro `write_sysreg()` with the arguments `amair_el1` and `0U`.
 * 7. Use the macro `write_sysreg()` with the arguments `vbar_el1` and `0U`.
 * 8. Use the macro `write_sysreg()` with the arguments `cpacr_el1` and `0U`.
 * 9. Use the macro `write_sysreg()` with the arguments `contextidr_el1` and
 *    `0U`.
 * 10. Use the macro `write_sysreg()` with the arguments `tpidr_el1` and
 *     `0U`.
 * 11. Use the macro `write_sysreg()` with the arguments `tpidr_el0` and
 *     `0U`.
 * 12. Use the macro `write_sysreg()` with the arguments `tpidrro_el0` and
 *     `0U`.
 * 13. Use the macro `write_sysreg()` with the arguments `sp_el0` and `0U`.
 * 14. Use the macro `write_sysreg()` with the arguments `sp_el1` and `0U`.
 * 15. Use the macro `write_sysreg()` with the arguments `elr_el1` and `0U`.
 * 16. Use the macro `write_sysreg()` with the arguments `spsr_el1` and `0U`.
 * 17. Use the macro `write_sysreg()` with the arguments `esr_el1` and `0U`.
 * 18. Use the macro `write_sysreg()` with the arguments `far_el1` and `0U`.
 * 19. Use the macro `write_sysreg()` with the arguments `afsr0_el1` and
 *     `0U`.
 * 20. Use the macro `write_sysreg()` with the arguments `afsr1_el1` and
 *     `0U`.
 * 21. Use the macro `write_sysreg()` with the arguments `par_el1` and `0U`.
 * 22. Use the macro `write_sysreg()` with the arguments `csselr_el1` and
 *     `0U`.
 * 23. Use the macro `write_sysreg()` with the arguments `mdscr_el1` and
 *     `0U`.
 * 24. Use the macro `write_sysreg()` with the arguments `cntkctl_el1` and
 *     `0U`.
 * 25. Use the macro `write_sysreg()` with the arguments `cntv_ctl_el0` and
 *     `0U`.
 * 26. Use the macro `write_sysreg()` with the arguments `cntv_cval_el0` and
 *     `0U`.
 * 27. Use the macro `write_sysreg()` with the arguments `cntp_ctl_el0` and
 *     `0U`.
 * 28. Use the macro `write_sysreg()` with the arguments `cntp_cval_el0` and
 *     `0U`.
 *
 * @calls
 * None.
 */
static void reset_el1(void)
{
    write_sysreg(sctlr_el1, SCTLR_EL1_RESET);
    write_sysreg(tcr_el1, 0U);
    write_sysreg(ttbr0_el1, 0U);
    write_sysreg(ttbr1_el1, 0U);
    write_sysreg(mair_el1, 0U);
    write_sysreg(amair_el1, 0U);
    write_sysreg(vbar_el1, 0U);
    write_sysreg(cpacr_el1, 0U);
    write_sysreg(contextidr_el1, 0U);
    write_sysreg(tpidr_el1, 0U);
    write_sysreg(tpidr_el0, 0U);
    write_sysreg(tpidrro_el0, 0U);
    write_sysreg(sp_el0, 0U);
    write_sysreg(sp_el1, 0U);
    write_sysreg(elr_el1, 0U);
    write_sysreg(spsr_el1, 0U);
    write_sysreg(esr_el1, 0U);
    write_sysreg(far_el1, 0U);
    write_sysreg(afsr0_el1, 0U);
    write_sysreg(afsr1_el1, 0U);
    write_sysreg(par_el1, 0U);
    write_sysreg(csselr_el1, 0U);
    write_sysreg(mdscr_el1, 0U);
    write_sysreg(cntkctl_el1, 0U);
    write_sysreg(cntv_ctl_el0, 0U);
    write_sysreg(cntv_cval_el0, 0U);
    write_sysreg(cntp_ctl_el0, 0U);
    write_sysreg(cntp_cval_el0, 0U);
}

/**
 * @funcname `hv_arch_vcpu_init`
 *
 * @description
 * Sets the guest's state at power-on, at the VM's first start and each reset:
 * at the entry point at EL1 with the MMU off and exceptions masked, x0 the
 * device tree; EL1 registers and the interrupt state reset, its virtual counter
 * from 0, its old translations gone.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in,out] The vCPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `regs` | `hv_guest_regs_t *` | The guest's saved registers |
 * | `host_sp` | `uint64_t` | The hypervisor's stack pointer, kept |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `regs` to the address of `vcpu->arch.regs`.
 * 2. Set `host_sp` to `regs->host_sp`.
 * 3. Call `memset()` with the arguments `regs`, `0` and `sizeof(*regs)`.
 * 4. Set `regs->host_sp` to `host_sp`.
 * 5. Set `regs->x[0]` to `HV_GUEST_RAM`.
 * 6. Set `regs->elr` to `vcpu->vm->config->entry`.
 * 7. Set `regs->spsr` to `SPSR_EL1H_MASKED`.
 * 8. Call `reset_el1()`.
 * 9. Call `gic_vcpu_reset()`.
 * 10. Call `isb()`.
 * 11. Use the macro `write_sysreg()` with the arguments `cntvoff_el2` and
 *     `read_sysreg(cntpct_el0)`.
 * 12. Call `arm64_tlbi_vmalls12e1()`.
 * 13. Call `arm64_dsb_ish()`.
 * 14. Call `isb()`.
 *
 * @calls
 * - `memset()`
 * - `reset_el1()`
 * - `gic_vcpu_reset()`
 * - `isb()`
 * - `arm64_tlbi_vmalls12e1()`
 * - `arm64_dsb_ish()`
 */
void hv_arch_vcpu_init(hv_vcpu_t *vcpu)
{
    hv_guest_regs_t *regs = &vcpu->arch.regs;
    uint64_t host_sp = regs->host_sp;

    (void) memset(regs, 0, sizeof(*regs));
    regs->host_sp = host_sp;
    regs->x[0] = HV_GUEST_RAM;
    regs->elr = vcpu->vm->config->entry;
    regs->spsr = SPSR_EL1H_MASKED;

    reset_el1();
    gic_vcpu_reset();
    isb();
    write_sysreg(cntvoff_el2, read_sysreg(cntpct_el0));
    arm64_tlbi_vmalls12e1();
    arm64_dsb_ish();
    isb();
}

/**
 * @funcname `hv_report_state`
 *
 * @description
 * Prints the main registers of a vCPU when its VM stops.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `const hv_vcpu_t *` | [in] The vCPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `regs` | `const hv_guest_regs_t *` | The guest's saved registers |
 * | `name` | `const char *` | The VM's name |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `regs` to the address of `vcpu->arch.regs`.
 * 2. Set `name` to `vcpu->vm->config->name`.
 * 3. Call `hv_printf()` with the arguments
 *    `"[hv] %%s: PC 0x%%lx PSTATE 0x%%lx ESR 0x%%lx FAR 0x%%lx\n"`
 *    `"[hv] %%s: SCTLR_EL1 0x%%lx TTBR0_EL1 0x%%lx VBAR_EL1 0x%%lx\n"`, `name`,
 *    `regs->elr`, `regs->spsr`, `read_sysreg(esr_el2)`,
 *    `read_sysreg(far_el2)`, `name`, `read_sysreg(sctlr_el1)`,
 *    `read_sysreg(ttbr0_el1)` and `read_sysreg(vbar_el1)`.
 *
 * @calls
 * - `hv_printf()`
 */
void hv_report_state(const hv_vcpu_t *vcpu)
{
    const hv_guest_regs_t *regs = &vcpu->arch.regs;
    const char *name = vcpu->vm->config->name;

    hv_printf("[hv] %s: PC 0x%lx PSTATE 0x%lx ESR 0x%lx FAR 0x%lx\n"
              "[hv] %s: SCTLR_EL1 0x%lx TTBR0_EL1 0x%lx VBAR_EL1 0x%lx\n",
              name, regs->elr, regs->spsr, read_sysreg(esr_el2),
              read_sysreg(far_el2), name, read_sysreg(sctlr_el1),
              read_sysreg(ttbr0_el1), read_sysreg(vbar_el1));
}

/**
 * @funcname `report_fetch`
 *
 * @description
 * Reports an instruction fetch of the guest outside its RAM.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `const hv_vcpu_t *` | [in] The vCPU. |
 * | `esr` | `uint64_t` | [in] ESR_EL2 of the abort. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ipa` | `uint64_t` | The guest-physical address (HPFAR_EL2 and FAR_EL2) |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ipa` to `((read_sysreg(hpfar_el2) >> HPFAR_FIPA_SHIFT) << 12) |`
 *    `(read_sysreg(far_el2) & PAGE_OFFSET_MASK)`.
 * 2. Call `hv_printf()` with the arguments
 *    `"[hv] %%s: instruction fetch from guest-physical address "`
 *    `"0x%%lx outside its RAM (ESR 0x%%lx)\n"`, `vcpu->vm->config->name`, `ipa`
 *    and `esr`.
 * 3. Call `hv_report_state()` with the argument `vcpu`.
 *
 * @calls
 * - `hv_printf()`
 * - `hv_report_state()`
 */
static void report_fetch(const hv_vcpu_t *vcpu, uint64_t esr)
{
    uint64_t ipa = ((read_sysreg(hpfar_el2) >> HPFAR_FIPA_SHIFT) << 12)
                   | (read_sysreg(far_el2) & PAGE_OFFSET_MASK);

    hv_printf("[hv] %s: instruction fetch from guest-physical address "
              "0x%lx outside its RAM (ESR 0x%lx)\n", vcpu->vm->config->name,
              ipa, esr);
    hv_report_state(vcpu);
}

/**
 * @funcname `handle_sync`
 *
 * @description
 * Handles a synchronous exception of the guest.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in,out] The vCPU. |
 * | `esr` | `uint64_t` | [in] ESR_EL2. |
 *
 * @return What the run loop does next.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `regs` | `hv_guest_regs_t *` | The guest's saved registers |
 * | `name` | `const char *` | The VM's name |
 * | `ec` | `uint32_t` | The exception class |
 * | `action` | `hv_action_e` | What the run loop does next |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `regs` to the address of `vcpu->arch.regs`.
 * 2. Set `name` to `vcpu->vm->config->name`.
 * 3. Set `ec` to `(uint32_t) ((esr >> ESR_EC_SHIFT) & ESR_EC_MASK)`.
 * 4. Set `action` to `HV_CONTINUE`.
 * 5. Depending on `ec`:
 *    1. Case `EC_HVC64`:
 *       1. Call `psci_guest_call()` with the arguments `vcpu` and
 *          `(uint32_t) (esr & ESR_ISS_IMM16)`, and store the result in
 *          `action`.
 *       2. Leave the switch.
 *    2. Case `EC_SMC64`:
 *       1. Add `4U` to `regs->elr`.
 *       2. Call `psci_guest_call()` with the arguments `vcpu` and
 *          `(uint32_t) (esr & ESR_ISS_IMM16)`, and store the result in
 *          `action`.
 *       3. Leave the switch.
 *    3. Case `EC_DABT_LOW`:
 *       1. Call `mmio_abort()` with the arguments `vcpu` and `esr`, and store
 *          the result in `action`.
 *       2. Leave the switch.
 *    4. Case `EC_IABT_LOW`:
 *       1. Call `report_fetch()` with the arguments `vcpu` and `esr`.
 *       2. Set `action` to `HV_STOP`.
 *       3. Leave the switch.
 *    5. Case `EC_WFX`:
 *       1. Add `4U` to `regs->elr`.
 *       2. Leave the switch.
 *    6. Otherwise (default):
 *       1. Call `hv_printf()` with the arguments
 *          `"[hv] %%s: unexpected exception (ESR 0x%%lx)\n"`, `name` and `esr`.
 *       2. Call `hv_report_state()` with the argument `vcpu`.
 *       3. Set `action` to `HV_STOP`.
 *       4. Leave the switch.
 * 6. Return `action`.
 *
 * @calls
 * - `psci_guest_call()`
 * - `mmio_abort()`
 * - `report_fetch()`
 * - `hv_printf()`
 * - `hv_report_state()`
 */
static hv_action_e handle_sync(hv_vcpu_t *vcpu, uint64_t esr)
{
    hv_guest_regs_t *regs = &vcpu->arch.regs;
    const char *name = vcpu->vm->config->name;
    uint32_t ec = (uint32_t) ((esr >> ESR_EC_SHIFT) & ESR_EC_MASK);
    hv_action_e action = HV_CONTINUE;

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

/**
 * @funcname `handle_exit`
 *
 * @description
 * Handles an exit of the guest of this CPU.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in,out] The CPU. |
 * | `exit` | `uint64_t` | [in] The kind of exit (vectors.S). |
 *
 * @return What the run loop does next.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | The CPU's vCPU |
 * | `name` | `const char *` | The VM's name |
 * | `action` | `hv_action_e` | What the run loop does next |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `vcpu` to the address of `cpu->vcpu`.
 * 2. Set `name` to `vcpu->vm->config->name`.
 * 3. Set `action` to `HV_CONTINUE`.
 * 4. If `exit` is equal to `HV_EXIT_SYNC`:
 *    1. Call `handle_sync()` with the arguments `vcpu` and
 *       `read_sysreg(esr_el2)`, and store the result in `action`.
 * 5. Otherwise, if `exit` is equal to `HV_EXIT_IRQ`:
 *    1. Call `gic_handle_irq()` with the argument `cpu`.
 * 6. Otherwise, if `exit` is equal to `HV_EXIT_FIQ`:
 *    1. If `cpu->arch.fiq_logged` is false:
 *       1. Set `cpu->arch.fiq_logged` to `true`.
 *       2. Call `hv_printf()` with the arguments
 *          `"[hv] CPU %%u: unexpected FIQ\n"` and `cpu->index`.
 * 7. Otherwise, if `exit` is equal to `HV_EXIT_SERROR`:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: SError (ESR 0x%%lx)\n"`, `name` and
 *       `read_sysreg(esr_el2)`.
 *    2. Call `hv_report_state()` with the argument `vcpu`.
 *    3. Set `action` to `HV_STOP`.
 * 8. Otherwise:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: exception from AArch32 code, which isn't " "supported\n"`
 *       and `name`.
 *    2. Call `hv_report_state()` with the argument `vcpu`.
 *    3. Set `action` to `HV_STOP`.
 * 9. Return `action`.
 *
 * @calls
 * - `handle_sync()`
 * - `gic_handle_irq()`
 * - `hv_printf()`
 * - `hv_report_state()`
 */
static hv_action_e handle_exit(hv_cpu_t *cpu, uint64_t exit)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    const char *name = vcpu->vm->config->name;
    hv_action_e action = HV_CONTINUE;

    if (exit == HV_EXIT_SYNC) {
        action = handle_sync(vcpu, read_sysreg(esr_el2));
    } else if (exit == HV_EXIT_IRQ) {
        gic_handle_irq(cpu);
    } else if (exit == HV_EXIT_FIQ) {
        if (!cpu->arch.fiq_logged) {
            cpu->arch.fiq_logged = true;
            hv_printf("[hv] CPU %u: unexpected FIQ\n", cpu->index);
        }
    } else if (exit == HV_EXIT_SERROR) {
        hv_printf("[hv] %s: SError (ESR 0x%lx)\n", name,
                  read_sysreg(esr_el2));
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

/**
 * @funcname `hv_arch_vcpu_run`
 *
 * @description
 * The run loop of a vCPU: runs the guest and handles its exit; a reset reloads
 * the VM, a stop halts the CPU. Doesn't return.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in,out] The CPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | The CPU's vCPU |
 * | `vm` | `hv_vm_t *` | Its VM |
 * | `exit` | `uint64_t` | The kind of exit |
 * | `action` | `hv_action_e` | What to do after the exit |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `vcpu` to the address of `cpu->vcpu`.
 * 2. Set `vm` to `vcpu->vm`.
 * 3. While `true` is true:
 *    1. Call `hv_guest_enter()` with the address of `vcpu->arch.regs`, and
 *       store the result in `exit`.
 *    2. Call `handle_exit()` with the arguments `cpu` and `exit`, and store the
 *       result in `action`.
 *    3. If `action` is equal to `HV_RESET`:
 *       1. Increment `vm->resets`.
 *       2. Call `vm_print()` with the arguments `vm` and `"reset"`.
 *       3. Call `vm_load()` with the argument `vm`.
 *       4. Call `hv_arch_vcpu_init()` with the argument `vcpu`.
 *    4. Otherwise, if `action` is equal to `HV_STOP`:
 *       1. Call `vm_print()` with the arguments `vm` and `"stopped"`.
 *       2. Call `hv_halt_forever()`.
 *    5. Otherwise:
 *       1. Do nothing.
 *
 * @calls
 * - `hv_guest_enter()`
 * - `handle_exit()`
 * - `vm_print()`
 * - `vm_load()`
 * - `hv_arch_vcpu_init()`
 * - `hv_halt_forever()`
 */
void hv_arch_vcpu_run(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    hv_vm_t *vm = vcpu->vm;

    while (true) {
        uint64_t exit = hv_guest_enter(&vcpu->arch.regs);
        hv_action_e action = handle_exit(cpu, exit);

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
