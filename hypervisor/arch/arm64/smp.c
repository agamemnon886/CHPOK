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
 * Start-up of the other CPUs: each one is started with PSCI CPU_ON at
 * hv_secondary_entry (entry.S), with its index as the context ID, and
 * must come online within a second.
 */

#include <arch/internal.h>

#define START_TIMEOUT_US    1000000U

/*
 * Wait up to the given number of microseconds for a CPU to come online;
 * returns whether it did.
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

/*
 * Start CPUs 1 to hv_nb_cpus - 1 (by MPIDR), one after another. Stops the
 * hypervisor if one doesn't start.
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
