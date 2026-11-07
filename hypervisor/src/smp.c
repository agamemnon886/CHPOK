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
 * Start-up of the other physical CPUs (APs): INIT, then start-up IPIs
 * which make them run the trampoline of entry.S in real mode. They are
 * started one after another, as they share the trampoline's stack and the
 * hv_ap_stack_top/hv_ap_cpu_index variables.
 */

#include <hv/hv.h>
#include <hv/x86.h>

#define AP_TRAMPOLINE       0x8000UL
#define AP_SIPI_VECTOR      ((uint8_t) (AP_TRAMPOLINE >> 12))
#define AP_STACK_SIZE       0x4000U
#define AP_START_TIMEOUT_US 1000000U

extern const uint8_t hv_ap_trampoline[];
extern const uint8_t hv_ap_trampoline_end[];
extern volatile uint64_t hv_ap_stack_top;
extern volatile uint32_t hv_ap_cpu_index;

// Copy the AP start-up code (entry.S) to AP_TRAMPOLINE.
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

    hv_memcpy((void *) AP_TRAMPOLINE, hv_ap_trampoline, size);
}

/*
 * Wait up to the given number of microseconds for a CPU to come online; returns
 * whether it did.
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
 * Start the CPUs 1 to nb_cpus - 1 (by local APIC ID), one after another, each
 * on its own stack. Stops the hypervisor if one doesn't start.
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
