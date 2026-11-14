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
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and declarations
 * are in arch/internal.h, documented there.
 */

/**
 * @file
 * Start-up of the other CPUs: each one is started with PSCI CPU_ON at
 * hv_secondary_entry (entry.S), with its index as the context ID, and must
 * come online within a second.
 */

#include <arch/internal.h>

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
 *
 * @llr
 *
 * @design
 * 1. Set `waited` to `0U`.
 * 2. While `cpu->online` is equal to `0U` and `waited` is less than
 *    `microseconds`:
 *    1. Call `hv_udelay()` with the argument `10U`.
 *    2. Add `10U` to `waited`.
 * 3. Return `cpu->online != 0U`.
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
 * Starts the CPUs 1 to hv_nb_cpus - 1 by their MPIDRs, one after another. Stops
 * the hypervisor if one doesn't start.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_cpus` | `hv_cpu_t[HV_MAX_CPUS]` | The CPUs |
 * | `hv_nb_cpus` | `uint32_t` | The number of CPUs used |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `entry` | `uint64_t` | The physical address of hv_secondary_entry |
 * | `i` | `uint32_t` | The index of the CPU |
 * | `cpu` | `const hv_cpu_t *` | The CPU being started |
 * | `result` | `int64_t` | The PSCI result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `entry` to `(uint64_t) (uintptr_t) &hv_secondary_entry`.
 * 2. Set `hv_cpus[0].online` to `1U`.
 * 3. Set `i` to `1U`; then while `i` is less than `hv_nb_cpus`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `cpu` to the address of `hv_cpus[i]`.
 *    2. Call `psci_host_cpu_on()` with the arguments `cpu->hw_id`, `entry` and
 *       `i`, and store the result in `result`.
 *    3. If `result` is not equal to `0`:
 *       1. Call `hv_printf()` with the arguments
 *          `"[hv] CPU %%u (MPIDR 0x%%x): PSCI CPU_ON failed (%%ld)\n"`, `i`,
 *          `cpu->hw_id` and `result`.
 *       2. Call `hv_fatal()` with the argument `"CPU start-up failed"`.
 *    4. If `wait_online(cpu, START_TIMEOUT_US)` is false:
 *       1. Call `hv_printf()` with the arguments
 *          `"[hv] CPU %%u (MPIDR 0x%%x) doesn't start\n"`, `i` and
 *          `cpu->hw_id`.
 *       2. Call `hv_fatal()` with the argument `"CPU start-up failed"`.
 *
 * @calls
 * - `psci_host_cpu_on()`
 * - `hv_printf()`
 * - `hv_fatal()`
 * - `wait_online()`
 */
/*
 * MISRA C:2025 Rule 11.1 and Rule 11.4 deviation (hv_smp_start()): PSCI
 * CPU_ON takes the physical address of the entry point (identity-mapped),
 * converted from the function pointer.
 */
void hv_smp_start(void)
{
    uint64_t entry = (uint64_t) (uintptr_t) &hv_secondary_entry;

    hv_cpus[0].online = 1U;
    for (uint32_t i = 1U; i < hv_nb_cpus; i++) {
        const hv_cpu_t *cpu = &hv_cpus[i];
        int64_t result = psci_host_cpu_on(cpu->hw_id, entry, i);

        if (result != 0) {
            hv_printf("[hv] CPU %u (MPIDR 0x%x): PSCI CPU_ON failed (%ld)\n",
                      i, cpu->hw_id, result);
            hv_fatal("CPU start-up failed");
        }
        if (!wait_online(cpu, START_TIMEOUT_US)) {
            hv_printf("[hv] CPU %u (MPIDR 0x%x) doesn't start\n", i,
                      cpu->hw_id);
            hv_fatal("CPU start-up failed");
        }
    }
}
