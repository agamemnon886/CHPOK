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
 * PSCI (Arm Power State Coordination Interface), both ways:
 *
 * - The hypervisor starts the other CPUs with CPU_ON, through smc: with
 *   EL2 and no EL3, QEMU 'virt' answers PSCI calls made with smc.
 * - A guest's calls (hvc, or smc, which HCR_EL2.TSC traps) are answered
 *   here, for a VM with one running vCPU: VERSION (1.0), FEATURES,
 *   AFFINITY_INFO, CPU_ON (the only CPU is on already), CPU_OFF and
 *   SYSTEM_OFF (the VM stops) and SYSTEM_RESET (the VM restarts).
 *   Anything else is NOT_SUPPORTED.
 */

#include <arch/internal.h>

#define PSCI_VERSION            0x84000000U
#define PSCI_CPU_OFF            0x84000002U
#define PSCI_CPU_ON_32          0x84000003U
#define PSCI_CPU_ON_64          0xC4000003U
#define PSCI_AFFINITY_INFO_32   0x84000004U
#define PSCI_AFFINITY_INFO_64   0xC4000004U
#define PSCI_SYSTEM_OFF         0x84000008U
#define PSCI_SYSTEM_RESET       0x84000009U
#define PSCI_FEATURES           0x8400000AU

#define PSCI_VERSION_1_0        0x00010000
#define PSCI_SUCCESS            0
#define PSCI_NOT_SUPPORTED      (-1)
#define PSCI_INVALID_PARAMETERS (-2)
#define PSCI_ALREADY_ON         (-4)
#define PSCI_AFFINITY_ON        0

#define MPIDR_AFFINITY          0xFFFFFFUL  // Affinity levels 0-2

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

// Whether a guest PSCI function is answered (PSCI_FEATURES).
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

/*
 * A guest's PSCI call (hvc or smc #immediate, function in x0): the result
 * goes to x0; returns what the run loop does next.
 */
hv_action_t psci_guest_call(hv_vcpu_t *vcpu, uint32_t immediate)
{
    hv_guest_regs_t *regs = &vcpu->arch.regs;
    uint32_t function = (uint32_t) regs->x[0];
    uint64_t target = regs->x[1] & MPIDR_AFFINITY;
    int64_t result = PSCI_NOT_SUPPORTED;
    hv_action_t action = HV_CONTINUE;

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
