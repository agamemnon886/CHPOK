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
 * hypervisor/arch/arm64/psci.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps).
 */

/**
 * @file
 * PSCI (Arm Power State Coordination Interface), both ways:
 *
 * - The hypervisor starts the other CPUs with CPU_ON, through smc: with EL2
 *   and no EL3, QEMU 'virt' answers PSCI calls made with smc.
 * - A guest's calls (hvc, or smc, which HCR_EL2.TSC traps) are answered
 *   here, for a VM with one running vCPU: VERSION (1.0), FEATURES,
 *   AFFINITY_INFO, CPU_ON (the only CPU is on already), CPU_OFF and
 *   SYSTEM_OFF (the VM stops) and SYSTEM_RESET (the VM restarts). Anything
 *   else is NOT_SUPPORTED.
 */

#include <arch/internal.h>

/**
 * @funcname `psci_host_cpu_on`
 *
 * @description
 * Starts a physical CPU with PSCI CPU_ON (64-bit), through smc.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mpidr` | `uint64_t` | [in] The CPU's MPIDR. |
 * | `entry` | `uint64_t` | [in] The physical address where it starts. |
 * | `context` | `uint64_t` | [in] The context ID it gets in x0 (its index). |
 *
 * @return The PSCI result: 0 on success.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x0` | `uint64_t` | Register x0: the function ID, then the result |
 * | `x1` | `uint64_t` | Register x1: the MPIDR |
 * | `x2` | `uint64_t` | Register x2: the entry point |
 * | `x3` | `uint64_t` | Register x3: the context ID |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `x0` to `PSCI_CPU_ON_64`.
 * 2. Set `x1` to `mpidr`.
 * 3. Set `x2` to `entry`.
 * 4. Set `x3` to `context`.
 * 5. Execute the assembly code `smc #0`.
 * 6. Return `(int64_t) x0`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (psci_host_cpu_on()): the
 * smc instruction is GNU inline assembly, with GNU register variables
 * for the arguments of the SMC calling convention.
 */
int64_t psci_host_cpu_on(uint64_t mpidr, uint64_t entry, uint64_t context)
{
    register uint64_t x0 asm ("x0") = PSCI_CPU_ON_64;
    register uint64_t x1 asm ("x1") = mpidr;
    register uint64_t x2 asm ("x2") = entry;
    register uint64_t x3 asm ("x3") = context;

    asm volatile ("smc #0"
                  : "+r" (x0), "+r" (x1), "+r" (x2), "+r" (x3)
                  :
                  : "memory");
    return (int64_t) x0;
}

/**
 * @funcname `supported`
 *
 * @description
 * Tells whether a guest PSCI function is answered (for PSCI_FEATURES).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `function` | `uint32_t` | [in] The function ID. |
 *
 * @return true for the functions of this file.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `yes` | `bool` | Whether it is answered |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Depending on `function`:
 *    1. Case `PSCI_VERSION` or `PSCI_CPU_OFF` or `PSCI_CPU_ON_32` or
 *       `PSCI_CPU_ON_64` or `PSCI_AFFINITY_INFO_32` or `PSCI_AFFINITY_INFO_64`
 *       or `PSCI_SYSTEM_OFF` or `PSCI_SYSTEM_RESET` or `PSCI_FEATURES`:
 *       1. Set `yes` to `true`.
 *       2. Leave the switch.
 *    2. Otherwise (default):
 *       1. Set `yes` to `false`.
 *       2. Leave the switch.
 * 2. Return `yes`.
 *
 * @calls
 * None.
 */
static bool supported(uint32_t function)
{
    bool yes;

    switch (function) {
        case PSCI_VERSION:
        case PSCI_CPU_OFF:
        case PSCI_CPU_ON_32:
        case PSCI_CPU_ON_64:
        case PSCI_AFFINITY_INFO_32:
        case PSCI_AFFINITY_INFO_64:
        case PSCI_SYSTEM_OFF:
        case PSCI_SYSTEM_RESET:
        case PSCI_FEATURES:
            yes = true;
            break;
        default:
            yes = false;
            break;
    }
    return yes;
}

/**
 * @funcname `psci_guest_call`
 *
 * @description
 * Answers a guest's PSCI call (hvc or smc, the function in x0); the result goes
 * to x0.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in,out] The vCPU. |
 * | `immediate` | `uint32_t` | [in] The immediate of the hvc or smc (0 for PSCI). |
 *
 * @return What the run loop does next: HV_CONTINUE, HV_STOP or HV_RESET.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `regs` | `hv_guest_regs_t *` | The guest's registers |
 * | `function` | `uint32_t` | The function ID |
 * | `target` | `uint64_t` | The target MPIDR (x1) |
 * | `result` | `int64_t` | The result |
 * | `action` | `hv_action_e` | What the run loop does next |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `regs` to the address of `vcpu->arch.regs`.
 * 2. Set `function` to `(uint32_t) regs->x[0]`.
 * 3. Set `target` to `regs->x[1] & MPIDR_AFFINITY`.
 * 4. Set `result` to `PSCI_NOT_SUPPORTED`.
 * 5. Set `action` to `HV_CONTINUE`.
 * 6. If `immediate` is not equal to `0U`:
 *    1. Set `function` to `0U`.
 * 7. Depending on `function`:
 *    1. Case `PSCI_VERSION`:
 *       1. Set `result` to `PSCI_VERSION_1_0`.
 *       2. Leave the switch.
 *    2. Case `PSCI_FEATURES`:
 *       1. Set `result` to `PSCI_SUCCESS` if `supported((uint32_t) regs->x[1])`
 *          is true, otherwise `PSCI_NOT_SUPPORTED`.
 *       2. Leave the switch.
 *    3. Case `PSCI_AFFINITY_INFO_32` or `PSCI_AFFINITY_INFO_64`:
 *       1. Set `result` to `PSCI_AFFINITY_ON` if `target` is equal to
 *          `vcpu->index`, otherwise `PSCI_INVALID_PARAMETERS`.
 *       2. Leave the switch.
 *    4. Case `PSCI_CPU_ON_32` or `PSCI_CPU_ON_64`:
 *       1. Set `result` to `PSCI_ALREADY_ON` if `target` is equal to
 *          `vcpu->index`, otherwise `PSCI_INVALID_PARAMETERS`.
 *       2. Leave the switch.
 *    5. Case `PSCI_CPU_OFF`:
 *       1. Call `vm_print()` with the arguments `vcpu->vm` and
 *          `"turned its CPU off (PSCI CPU_OFF)"`.
 *       2. Set `action` to `HV_STOP`.
 *       3. Leave the switch.
 *    6. Case `PSCI_SYSTEM_OFF`:
 *       1. Call `vm_print()` with the arguments `vcpu->vm` and
 *          `"powered off (PSCI SYSTEM_OFF)"`.
 *       2. Set `action` to `HV_STOP`.
 *       3. Leave the switch.
 *    7. Case `PSCI_SYSTEM_RESET`:
 *       1. Set `action` to `HV_RESET`.
 *       2. Leave the switch.
 *    8. Otherwise (default):
 *       1. Leave the switch.
 * 8. Set `regs->x[0]` to `(uint64_t) result`.
 * 9. Return `action`.
 *
 * @calls
 * - `supported()`
 * - `vm_print()`
 */
hv_action_e psci_guest_call(hv_vcpu_t *vcpu, uint32_t immediate)
{
    hv_guest_regs_t *regs = &vcpu->arch.regs;
    uint32_t function = (uint32_t) regs->x[0];
    uint64_t target = regs->x[1] & MPIDR_AFFINITY;
    int64_t result = PSCI_NOT_SUPPORTED;
    hv_action_e action = HV_CONTINUE;

    if (immediate != 0U) {
        function = 0U;          // Not a PSCI call: NOT_SUPPORTED
    }
    switch (function) {
        case PSCI_VERSION:
            result = PSCI_VERSION_1_0;
            break;
        case PSCI_FEATURES:
            result = supported((uint32_t) regs->x[1]) ? PSCI_SUCCESS
                                                       : PSCI_NOT_SUPPORTED;
            break;
        case PSCI_AFFINITY_INFO_32:
        case PSCI_AFFINITY_INFO_64:
            result = (target == vcpu->index) ? PSCI_AFFINITY_ON
                                             : PSCI_INVALID_PARAMETERS;
            break;
        case PSCI_CPU_ON_32:
        case PSCI_CPU_ON_64:
            // The VM runs on one vCPU, which is on.
            result = (target == vcpu->index) ? PSCI_ALREADY_ON
                                             : PSCI_INVALID_PARAMETERS;
            break;
        case PSCI_CPU_OFF:
            vm_print(vcpu->vm, "turned its CPU off (PSCI CPU_OFF)");
            action = HV_STOP;
            break;
        case PSCI_SYSTEM_OFF:
            vm_print(vcpu->vm, "powered off (PSCI SYSTEM_OFF)");
            action = HV_STOP;
            break;
        case PSCI_SYSTEM_RESET:
            action = HV_RESET;
            break;
        default:
            // NOT_SUPPORTED.
            break;
    }
    regs->x[0] = (uint64_t) result;
    return action;
}
