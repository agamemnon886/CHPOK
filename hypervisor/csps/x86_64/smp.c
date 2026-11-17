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
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from hypervisor/src/ to
 * arch/x86_64/. Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and external
 * declarations are in arch/internal.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/x86_64/smp.c (the code of the processor architecture is in
 * hypervisor/csps); memcpy() of the library shared with the kernel replaces
 * hv_memcpy().
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 */

/**
 * @file
 * Start-up of the other physical CPUs (APs): INIT, then start-up IPIs which
 * make them run the trampoline of entry.S in real mode. They are started one
 * after another, as they share the trampoline's stack and the
 * hv_ap_stack_top/hv_ap_cpu_index variables.
 */

#include <arch/internal.h>

/**
 * @funcname `copy_trampoline`
 *
 * @description
 * Copies the AP start-up code of entry.S to its fixed physical address
 * (0x8000), where the start-up IPI makes the CPUs start.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_ap_trampoline` | `const uint8_t[]` | The start of the trampoline (entry.S) |
 * | `hv_ap_trampoline_end` | `const uint8_t[]` | The end of the trampoline |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `start` | `uintptr_t` | The address of the trampoline |
 * | `size` | `size_t` | The size of the trampoline |
 *
 * @hlr
 * - \hlrid{HLR_SMP_COPY_TRAMPOLINE_01}: `copy_trampoline()` shall put the
 *   start-up code of the other CPUs where the start-up interrupt makes them
 *   start.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_002}
 *
 * @llr
 * - \llrid{LLR_SMP_COPY_TRAMPOLINE_01}: `copy_trampoline()` shall copy with
 *   `memcpy()` the bytes from `hv_ap_trampoline` up to `hv_ap_trampoline_end`
 *   to the address `AP_TRAMPOLINE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SMP_COPY_TRAMPOLINE_01}
 *
 * @design
 * 1. Set `start` to `(uintptr_t) hv_ap_trampoline`.
 *    \llrref{LLR_SMP_COPY_TRAMPOLINE_01}
 * 2. Set `size` to `(size_t) ((uintptr_t) hv_ap_trampoline_end - start)`.
 *    \llrref{LLR_SMP_COPY_TRAMPOLINE_01}
 * 3. Call `memcpy()` with the arguments `(void *) AP_TRAMPOLINE`,
 *    `hv_ap_trampoline` and `size`. \llrref{LLR_SMP_COPY_TRAMPOLINE_01}
 *
 * @calls
 * - `memcpy()`
 */
/*
 * MISRA C:2025 Rule 11.4 and Rule 11.6 deviation (copy_trampoline(),
 * hv_smp_start()): the trampoline is copied to a fixed physical address
 * (identity-mapped), its size is the difference of two linker addresses,
 * and the stack top is passed to entry.S as an integer.
 */
static void copy_trampoline(void)
{
    uintptr_t start = (uintptr_t) hv_ap_trampoline;
    size_t size = (size_t) ((uintptr_t) hv_ap_trampoline_end - start);

    (void) memcpy((void *) AP_TRAMPOLINE, hv_ap_trampoline, size);
}

/**
 * @funcname `wait_online`
 *
 * @description
 * Waits up to a time for a CPU to come online.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `const hv_cpu_t *` | [in] The CPU. |
 * | `microseconds` | `uint64_t` | [in] The longest wait. |
 *
 * @return true if the CPU came online.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `waited` | `uint64_t` | The time waited so far, in microseconds |
 *
 * @hlr
 * - \hlrid{HLR_SMP_X86_64_WAIT_ONLINE_01}: `wait_online()` shall wait, up to a
 *   time, for a CPU to come online, and tell whether it did.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_002}
 *   - \srqref{SRQ_HV_BOOT_004}
 *
 * @llr
 * - \llrid{LLR_SMP_X86_64_WAIT_ONLINE_01}: `wait_online()` shall call
 *   `hv_udelay()` with 10 while `online` of the CPU is 0 and the microseconds
 *   waited, 10 for each call, are fewer than `microseconds`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SMP_X86_64_WAIT_ONLINE_01}
 * - \llrid{LLR_SMP_X86_64_WAIT_ONLINE_02}: `wait_online()` shall then return
 *   true if `online` of the CPU is not 0, and false if it is 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_SMP_X86_64_WAIT_ONLINE_01}
 *
 * @design
 * 1. Set `waited` to `0U`. \llrref{LLR_SMP_X86_64_WAIT_ONLINE_01}
 * 2. While `cpu->online` is equal to `0U` and `waited` is less than
 *    `microseconds`: \llrref{LLR_SMP_X86_64_WAIT_ONLINE_01}
 *    1. Call `hv_udelay()` with the argument `10U`.
 *    2. Add `10U` to `waited`.
 * 3. Return `cpu->online != 0U`. \llrref{LLR_SMP_X86_64_WAIT_ONLINE_02}
 *
 * @calls
 * - `hv_udelay()`
 */
static bool wait_online(const hv_cpu_t *cpu, uint64_t microseconds)
{
    uint64_t waited = 0U;

    while ((cpu->online == 0U) && (waited < microseconds)) {
        hv_udelay(10U);
        waited += 10U;
    }
    return cpu->online != 0U;
}

/**
 * @funcname `hv_smp_start`
 *
 * @description
 * Starts the CPUs 1 to nb_cpus - 1, one after another, each on its own
 * stack. Stops the hypervisor if one doesn't start.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `nb_cpus` | `uint32_t` | [in] The number of CPUs, the boot CPU included. |
 * | `apic_ids` | `const uint32_t[]` | [in] The local APIC ID of each CPU. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_cpus` | `hv_cpu_t[HV_MAX_CPUS]` | The CPUs |
 * | `hv_ap_stack_top` | `volatile uint64_t` | The stack of the starting CPU (for entry.S) |
 * | `hv_ap_cpu_index` | `volatile uint32_t` | The index of the starting CPU (for entry.S) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ap_stacks` | `static uint8_t[HV_MAX_CPUS][AP_STACK_SIZE]` | The stacks of the CPUs |
 * | `i` | `uint32_t` | The index of the CPU |
 * | `cpu` | `const hv_cpu_t *` | The CPU being started |
 *
 * @hlr
 * - \hlrid{HLR_SMP_X86_64_HV_SMP_START_01}: `hv_smp_start()` shall start the
 *   other physical CPUs that the VMs need, one after another, each on its own
 *   stack, by INIT and start-up interrupts.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_002}
 * - \hlrid{HLR_SMP_X86_64_HV_SMP_START_02}: `hv_smp_start()` shall stop with a
 *   message if a CPU does not start.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *
 * @llr
 * - \llrid{LLR_SMP_X86_64_HV_SMP_START_01}: `hv_smp_start()` shall call
 *   `copy_trampoline()` once and set `online` of `hv_cpus[0]` to 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_SMP_X86_64_HV_SMP_START_01}
 * - \llrid{LLR_SMP_X86_64_HV_SMP_START_02}: For each CPU `i` from 1 to
 *   `nb_cpus` minus 1, in order, `hv_smp_start()` shall set `hv_ap_stack_top`
 *   to the end of the stack of `AP_STACK_SIZE` bytes of that CPU and
 *   `hv_ap_cpu_index` to `i`, and then call, in this order, `x86_barrier()`,
 *   `lapic_send_init()` with `apic_ids[i]`, `hv_udelay()` with 10000 and
 *   `lapic_send_sipi()` with `apic_ids[i]` and `AP_SIPI_VECTOR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SMP_X86_64_HV_SMP_START_01}
 * - \llrid{LLR_SMP_X86_64_HV_SMP_START_03}: If and only if `wait_online()`
 *   returns false for the CPU and 1000, `hv_smp_start()` shall call
 *   `lapic_send_sipi()` with `apic_ids[i]` and `AP_SIPI_VECTOR` a second time.
 *   <br>Traces to:
 *   - \hlrref{HLR_SMP_X86_64_HV_SMP_START_01}
 * - \llrid{LLR_SMP_X86_64_HV_SMP_START_04}: If `wait_online()` then returns
 *   false for the CPU and `AP_START_TIMEOUT_US`, `hv_smp_start()` shall call
 *   `hv_printf()` with `"[hv] CPU %%u (APIC ID %%u) doesn't start\n"`, `i` and
 *   `apic_ids[i]`, and then `hv_fatal()` with `"CPU start-up failed"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SMP_X86_64_HV_SMP_START_02}
 * - \llrid{LLR_SMP_X86_64_HV_SMP_START_05}: If `wait_online()` then returns
 *   true for the CPU and `AP_START_TIMEOUT_US`, `hv_smp_start()` shall go on
 *   with the next CPU, without calling `hv_fatal()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SMP_X86_64_HV_SMP_START_01}
 *
 * @design
 * 1. Call `copy_trampoline()`. \llrref{LLR_SMP_X86_64_HV_SMP_START_01}
 * 2. Set `hv_cpus[0].online` to `1U`. \llrref{LLR_SMP_X86_64_HV_SMP_START_01}
 * 3. Set `i` to `1U`; then while `i` is less than `nb_cpus`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_SMP_X86_64_HV_SMP_START_02}
 *    1. Set `cpu` to the address of `hv_cpus[i]`.
 *    2. Set `hv_ap_stack_top` to
 *       `(uint64_t) ((uintptr_t) &ap_stacks[i][0] + AP_STACK_SIZE)`.
 *    3. Set `hv_ap_cpu_index` to `i`.
 *    4. Call `x86_barrier()`.
 *    5. Call `lapic_send_init()` with the argument `apic_ids[i]`.
 *    6. Call `hv_udelay()` with the argument `10000U`.
 *    7. Call `lapic_send_sipi()` with the arguments `apic_ids[i]` and
 *       `AP_SIPI_VECTOR`.
 *    8. If `wait_online(cpu, 1000U)` is false:
 *       \llrref{LLR_SMP_X86_64_HV_SMP_START_03}
 *       1. Call `lapic_send_sipi()` with the arguments `apic_ids[i]` and
 *          `AP_SIPI_VECTOR`.
 *    9. If `wait_online(cpu, AP_START_TIMEOUT_US)` is false:
 *       \llrref{LLR_SMP_X86_64_HV_SMP_START_04},
 *       \llrref{LLR_SMP_X86_64_HV_SMP_START_05}
 *       1. Call `hv_printf()` with the arguments
 *          `"[hv] CPU %%u (APIC ID %%u) doesn't start\n"`, `i` and
 *          `apic_ids[i]`.
 *       2. Call `hv_fatal()` with the argument `"CPU start-up failed"`.
 *
 * @calls
 * - `copy_trampoline()`
 * - `x86_barrier()`
 * - `lapic_send_init()`
 * - `hv_udelay()`
 * - `lapic_send_sipi()`
 * - `wait_online()`
 * - `hv_printf()`
 * - `hv_fatal()`
 */
void hv_smp_start(uint32_t nb_cpus, const uint32_t apic_ids[])
{
    static uint8_t ap_stacks[HV_MAX_CPUS][AP_STACK_SIZE] HV_ALIGNED(16);

    copy_trampoline();
    hv_cpus[0].online = 1U;

    for (uint32_t i = 1U; i < nb_cpus; i++) {
        const hv_cpu_t *cpu = &hv_cpus[i];

        // Stack and index for entry.S, which takes them before C runs.
        hv_ap_stack_top = (uint64_t) ((uintptr_t) &ap_stacks[i][0]
                                      + AP_STACK_SIZE);
        hv_ap_cpu_index = i;
        x86_barrier();

        // INIT, 10 ms, start-up IPI; a second one if the first is missed.
        lapic_send_init(apic_ids[i]);
        hv_udelay(10000U);
        lapic_send_sipi(apic_ids[i], AP_SIPI_VECTOR);
        if (!wait_online(cpu, 1000U)) {
            lapic_send_sipi(apic_ids[i], AP_SIPI_VECTOR);
        }
        if (!wait_online(cpu, AP_START_TIMEOUT_US)) {
            hv_printf("[hv] CPU %u (APIC ID %u) doesn't start\n", i,
                      apic_ids[i]);
            hv_fatal("CPU start-up failed");
        }
    }
}
