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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
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
 * - \hlrid{HLR_PSCI_PSCI_HOST_CPU_ON_01}: `psci_host_cpu_on()` shall start a
 *   physical CPU at an entry point with a context, by the PSCI call CPU_ON of
 *   the firmware, and give the result.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_002}
 *
 * @llr
 * - \llrid{LLR_PSCI_PSCI_HOST_CPU_ON_01}: `psci_host_cpu_on()` shall execute
 *   `smc #0` once with `PSCI_CPU_ON_64` in x0, `mpidr` in x1, `entry` in x2 and
 *   `context` in x3, and return the value that the call leaves in x0.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_HOST_CPU_ON_01}
 *
 * @design
 * 1. Set `x0` to `PSCI_CPU_ON_64`. \llrref{LLR_PSCI_PSCI_HOST_CPU_ON_01}
 * 2. Set `x1` to `mpidr`. \llrref{LLR_PSCI_PSCI_HOST_CPU_ON_01}
 * 3. Set `x2` to `entry`. \llrref{LLR_PSCI_PSCI_HOST_CPU_ON_01}
 * 4. Set `x3` to `context`. \llrref{LLR_PSCI_PSCI_HOST_CPU_ON_01}
 * 5. Execute the assembly code `smc #0`. \llrref{LLR_PSCI_PSCI_HOST_CPU_ON_01}
 * 6. Return `(int64_t) x0`. \llrref{LLR_PSCI_PSCI_HOST_CPU_ON_01}
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
 * - \hlrid{HLR_PSCI_SUPPORTED_01}: `supported()` shall tell whether a PSCI
 *   function of a guest is answered.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_005}
 *
 * @llr
 * - \llrid{LLR_PSCI_SUPPORTED_01}: If `function` is `PSCI_VERSION`,
 *   `PSCI_CPU_OFF`, `PSCI_CPU_ON_32`, `PSCI_CPU_ON_64`,
 *   `PSCI_AFFINITY_INFO_32`, `PSCI_AFFINITY_INFO_64`, `PSCI_SYSTEM_OFF`,
 *   `PSCI_SYSTEM_RESET` or `PSCI_FEATURES`, `supported()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_SUPPORTED_01}
 * - \llrid{LLR_PSCI_SUPPORTED_02}: If `function` is none of `PSCI_VERSION`,
 *   `PSCI_CPU_OFF`, `PSCI_CPU_ON_32`, `PSCI_CPU_ON_64`,
 *   `PSCI_AFFINITY_INFO_32`, `PSCI_AFFINITY_INFO_64`, `PSCI_SYSTEM_OFF`,
 *   `PSCI_SYSTEM_RESET` and `PSCI_FEATURES`, `supported()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_SUPPORTED_01}
 *
 * @design
 * 1. Depending on `function`: \llrref{LLR_PSCI_SUPPORTED_01},
 *    \llrref{LLR_PSCI_SUPPORTED_02}
 *    1. Case `PSCI_VERSION` or `PSCI_CPU_OFF` or `PSCI_CPU_ON_32` or
 *       `PSCI_CPU_ON_64` or `PSCI_AFFINITY_INFO_32` or `PSCI_AFFINITY_INFO_64`
 *       or `PSCI_SYSTEM_OFF` or `PSCI_SYSTEM_RESET` or `PSCI_FEATURES`:
 *       \llrref{LLR_PSCI_SUPPORTED_01}
 *       1. Set `yes` to `true`.
 *       2. Leave the switch.
 *    2. Otherwise (default): \llrref{LLR_PSCI_SUPPORTED_02}
 *       1. Set `yes` to `false`.
 *       2. Leave the switch.
 * 2. Return `yes`. \llrref{LLR_PSCI_SUPPORTED_01},
 *    \llrref{LLR_PSCI_SUPPORTED_02}
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
 * - \hlrid{HLR_PSCI_PSCI_GUEST_CALL_01}: `psci_guest_call()` shall answer the
 *   PSCI calls of a guest of one CPU: the version, the features, the affinity
 *   information, the start of a CPU (it is already on), and refuse other calls.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_005}
 * - \hlrid{HLR_PSCI_PSCI_GUEST_CALL_02}: `psci_guest_call()` shall stop the VM,
 *   with a message, when the guest switches its CPU or the system off.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_008}
 * - \hlrid{HLR_PSCI_PSCI_GUEST_CALL_03}: `psci_guest_call()` shall reset the VM
 *   when the guest asks for a system reset.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_007}
 *
 * @llr
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_01}: If `immediate` is 0 and the function
 *   (the low 32 bits of x0 of the guest) is `PSCI_VERSION`, `psci_guest_call()`
 *   shall store `PSCI_VERSION_1_0` in x0 of the guest.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_01}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_02}: If `immediate` is 0, the function is
 *   `PSCI_FEATURES` and `supported()` returns true for the low 32 bits of x1 of
 *   the guest, `psci_guest_call()` shall store `PSCI_SUCCESS` in x0 of the
 *   guest.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_01}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_03}: If `immediate` is 0, the function is
 *   `PSCI_FEATURES` and `supported()` returns false for the low 32 bits of x1
 *   of the guest, `psci_guest_call()` shall store `PSCI_NOT_SUPPORTED` in x0 of
 *   the guest.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_01}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_04}: If `immediate` is 0, the function is
 *   `PSCI_AFFINITY_INFO_32` or `PSCI_AFFINITY_INFO_64` and the bits
 *   `MPIDR_AFFINITY` of x1 of the guest are equal to `vcpu->index`,
 *   `psci_guest_call()` shall store `PSCI_AFFINITY_ON` in x0 of the guest.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_01}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_05}: If `immediate` is 0, the function is
 *   `PSCI_CPU_ON_32` or `PSCI_CPU_ON_64` and the bits `MPIDR_AFFINITY` of x1 of
 *   the guest are equal to `vcpu->index`, `psci_guest_call()` shall store
 *   `PSCI_ALREADY_ON` in x0 of the guest.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_01}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_06}: If `immediate` is 0, the function is
 *   `PSCI_AFFINITY_INFO_32`, `PSCI_AFFINITY_INFO_64`, `PSCI_CPU_ON_32` or
 *   `PSCI_CPU_ON_64` and the bits `MPIDR_AFFINITY` of x1 of the guest are not
 *   equal to `vcpu->index`, `psci_guest_call()` shall store
 *   `PSCI_INVALID_PARAMETERS` in x0 of the guest.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_01}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_07}: If `immediate` is not 0, or the
 *   function is none of `PSCI_VERSION`, `PSCI_FEATURES`,
 *   `PSCI_AFFINITY_INFO_32`, `PSCI_AFFINITY_INFO_64`, `PSCI_CPU_ON_32` and
 *   `PSCI_CPU_ON_64`, `psci_guest_call()` shall store `PSCI_NOT_SUPPORTED` in
 *   x0 of the guest.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_01}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_08}: If `immediate` is 0 and the function
 *   is `PSCI_CPU_OFF`, `psci_guest_call()` shall call `vm_print()` once with
 *   the VM and `"turned its CPU off (PSCI CPU_OFF)"`, and return `HV_STOP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_02}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_09}: If `immediate` is 0 and the function
 *   is `PSCI_SYSTEM_OFF`, `psci_guest_call()` shall call `vm_print()` once with
 *   the VM and `"powered off (PSCI SYSTEM_OFF)"`, and return `HV_STOP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_02}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_10}: If `immediate` is 0 and the function
 *   is `PSCI_SYSTEM_RESET`, `psci_guest_call()` shall return `HV_RESET`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_03}
 * - \llrid{LLR_PSCI_PSCI_GUEST_CALL_11}: If `immediate` is not 0, or the
 *   function is none of `PSCI_CPU_OFF`, `PSCI_SYSTEM_OFF` and
 *   `PSCI_SYSTEM_RESET`, `psci_guest_call()` shall return `HV_CONTINUE`,
 *   without calling `vm_print()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PSCI_PSCI_GUEST_CALL_01}
 *
 * @design
 * 1. Set `regs` to the address of `vcpu->arch.regs`.
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_01},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_02},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_03},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_04},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_05},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_06},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_07},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_08},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_09},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_10}, \llrref{LLR_PSCI_PSCI_GUEST_CALL_11}
 * 2. Set `function` to `(uint32_t) regs->x[0]`.
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_01},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_02},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_03},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_04},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_05},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_06},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_07},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_08},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_09},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_10}, \llrref{LLR_PSCI_PSCI_GUEST_CALL_11}
 * 3. Set `target` to `regs->x[1] & MPIDR_AFFINITY`.
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_04},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_05}, \llrref{LLR_PSCI_PSCI_GUEST_CALL_06}
 * 4. Set `result` to `PSCI_NOT_SUPPORTED`. \llrref{LLR_PSCI_PSCI_GUEST_CALL_07}
 * 5. Set `action` to `HV_CONTINUE`. \llrref{LLR_PSCI_PSCI_GUEST_CALL_11}
 * 6. If `immediate` is not equal to `0U`: \llrref{LLR_PSCI_PSCI_GUEST_CALL_07},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_11}
 *    1. Set `function` to `0U`.
 * 7. Depending on `function`: \llrref{LLR_PSCI_PSCI_GUEST_CALL_01},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_02},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_03},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_04},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_05},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_06},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_07},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_08},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_09},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_10}, \llrref{LLR_PSCI_PSCI_GUEST_CALL_11}
 *    1. Case `PSCI_VERSION`: \llrref{LLR_PSCI_PSCI_GUEST_CALL_01}
 *       1. Set `result` to `PSCI_VERSION_1_0`.
 *       2. Leave the switch.
 *    2. Case `PSCI_FEATURES`: \llrref{LLR_PSCI_PSCI_GUEST_CALL_02},
 *       \llrref{LLR_PSCI_PSCI_GUEST_CALL_03}
 *       1. Set `result` to `PSCI_SUCCESS` if `supported((uint32_t) regs->x[1])`
 *          is true, otherwise `PSCI_NOT_SUPPORTED`.
 *       2. Leave the switch.
 *    3. Case `PSCI_AFFINITY_INFO_32` or `PSCI_AFFINITY_INFO_64`:
 *       \llrref{LLR_PSCI_PSCI_GUEST_CALL_04},
 *       \llrref{LLR_PSCI_PSCI_GUEST_CALL_06}
 *       1. Set `result` to `PSCI_AFFINITY_ON` if `target` is equal to
 *          `vcpu->index`, otherwise `PSCI_INVALID_PARAMETERS`.
 *       2. Leave the switch.
 *    4. Case `PSCI_CPU_ON_32` or `PSCI_CPU_ON_64`:
 *       \llrref{LLR_PSCI_PSCI_GUEST_CALL_05},
 *       \llrref{LLR_PSCI_PSCI_GUEST_CALL_06}
 *       1. Set `result` to `PSCI_ALREADY_ON` if `target` is equal to
 *          `vcpu->index`, otherwise `PSCI_INVALID_PARAMETERS`.
 *       2. Leave the switch.
 *    5. Case `PSCI_CPU_OFF`: \llrref{LLR_PSCI_PSCI_GUEST_CALL_08}
 *       1. Call `vm_print()` with the arguments `vcpu->vm` and
 *          `"turned its CPU off (PSCI CPU_OFF)"`.
 *       2. Set `action` to `HV_STOP`.
 *       3. Leave the switch.
 *    6. Case `PSCI_SYSTEM_OFF`: \llrref{LLR_PSCI_PSCI_GUEST_CALL_09}
 *       1. Call `vm_print()` with the arguments `vcpu->vm` and
 *          `"powered off (PSCI SYSTEM_OFF)"`.
 *       2. Set `action` to `HV_STOP`.
 *       3. Leave the switch.
 *    7. Case `PSCI_SYSTEM_RESET`: \llrref{LLR_PSCI_PSCI_GUEST_CALL_10}
 *       1. Set `action` to `HV_RESET`.
 *       2. Leave the switch.
 *    8. Otherwise (default): \llrref{LLR_PSCI_PSCI_GUEST_CALL_07},
 *       \llrref{LLR_PSCI_PSCI_GUEST_CALL_11}
 *       1. Leave the switch.
 * 8. Set `regs->x[0]` to `(uint64_t) result`.
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_01},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_02},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_03},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_04},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_05},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_06}, \llrref{LLR_PSCI_PSCI_GUEST_CALL_07}
 * 9. Return `action`. \llrref{LLR_PSCI_PSCI_GUEST_CALL_08},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_09},
 *    \llrref{LLR_PSCI_PSCI_GUEST_CALL_10}, \llrref{LLR_PSCI_PSCI_GUEST_CALL_11}
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
