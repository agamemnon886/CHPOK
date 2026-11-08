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
 * Modified by Kirill Brazhnikov, 2026-11-07: the boot CPU checks the
 * CRC-32 of the hypervisor image first.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from hypervisor/src/ to
 * arch/x86_64/; the vCPU assignment and hv_cpu_main() moved to
 * src/cpus.c, for both architectures.
 */

/*
 * Start-up of the hypervisor: the boot CPU checks its image, the CPU and
 * the configuration, measures the timers, sets up the VMs and starts the
 * other CPUs; then every CPU runs its vCPU (or parks it).
 */

#include <arch/internal.h>

// Multiboot information block (fields used here).
#define MBI_FLAGS           0U
#define MBI_MEM_UPPER       8U
#define MBI_FLAG_MEM        ((uint32_t) 1U << 0)
#define MBI_UPPER_MEM_START 0x100000UL

#define APIC_IDS            256U

// Index + 1 of the CPU with each local APIC ID (0: none).
static uint8_t cpu_of_apic_id[APIC_IDS];

void hv_main(uint32_t mbi);
void hv_ap_main(uint32_t index);

// Halt this CPU for good, with interrupts disabled.
void hv_halt_forever(void)
{
    x86_cli();
    while (true) {
        x86_hlt();
    }
}

// Hardware number of the calling CPU: its local APIC ID.
uint32_t hv_arch_cpu_id(void)
{
    return lapic_id();
}

/*
 * The CPU structure of the calling CPU (found by its local APIC ID; CPU 0
 * before the CPUs are known).
 */
hv_cpu_t *hv_this_cpu(void)
{
    uint32_t id = lapic_id();
    uint32_t index = 0U;

    if ((id < APIC_IDS) && (cpu_of_apic_id[id] != 0U)) {
        index = (uint32_t) cpu_of_apic_id[id] - 1U;
    }
    return &hv_cpus[index];
}

// Read a 32-bit field of the Multiboot information.
/*
 * MISRA C:2025 Rule 11.4 deviation (read_mbi32()): the boot loader passes
 * the physical address of the Multiboot information, identity-mapped, as
 * an integer.
 */
static uint32_t read_mbi32(uint32_t mbi, uint32_t offset)
{
    const volatile uint32_t *p =
        (const volatile uint32_t *) ((uintptr_t) mbi + offset);

    return *p;
}

// End of the RAM above 1 MiB, from the Multiboot information.
static uint64_t ram_end_of(uint32_t mbi)
{
    if ((mbi == 0U) || ((read_mbi32(mbi, MBI_FLAGS) & MBI_FLAG_MEM) == 0U)) {
        hv_fatal("the boot loader gave no memory size");
    }
    // mem_upper: KiB of RAM from 1 MiB up to the first hole.
    return MBI_UPPER_MEM_START
           + ((uint64_t) read_mbi32(mbi, MBI_MEM_UPPER) * 1024U);
}

// Physical CPUs: the boot CPU first, then the others of the MADT.
static uint32_t find_cpus(uint32_t apic_ids[HV_MAX_CPUS])
{
    uint32_t madt_ids[HV_MAX_CPUS];
    uint32_t found = acpi_find_cpus(madt_ids, HV_MAX_CPUS);
    uint32_t bsp = lapic_id();
    uint32_t count = 1U;

    apic_ids[0] = bsp;
    for (uint32_t i = 0U; i < found; i++) {
        // Skip the boot CPU (already first) and IDs beyond the xAPIC range.
        if ((madt_ids[i] != bsp) && (madt_ids[i] < APIC_IDS)) {
            apic_ids[count] = madt_ids[i];
            count++;
        }
    }
    return count;
}

/*
 * Start-up on the boot CPU (called by entry.S with the address of the
 * Multiboot information): checks the image, the CPU and the configuration,
 * measures the timers, sets up the VMs, starts the other CPUs, then runs
 * its own vCPU.
 */
void hv_main(uint32_t mbi)
{
    uint32_t apic_ids[HV_MAX_CPUS];
    uint32_t needed = hv_cpus_needed();
    uint64_t ram_end;
    uint32_t available;

    hv_console_init(hv_console_port);
    hv_printf("\n[hv] JetOS hypervisor: %u VM(s), %u CPU(s)\n",
              hv_nb_vms, needed);

    // The image must be the one the build made (stops if not).
    hv_image_check();

    // Interrupts of this CPU: IDT, legacy PIC masked, local APIC.
    lapic_init_base();
    hv_idt_init();
    hv_idt_load();
    hv_legacy_pic_disable();
    lapic_init_cpu();

    // The CPU must have SVM with nested paging.
    if (!svm_supported()) {
        hv_fatal("AMD SVM with nested paging is needed "
                 "(QEMU: -cpu qemu64,+svm,+npt)");
    }
    svm_init();

    hv_timing_init();
    hv_printf("[hv] TSC %lu kHz, local APIC timer %lu kHz\n",
              hv_tsc_hz() / 1000U, hv_lapic_hz() / 1000U);

    // The machine must have the CPUs and the RAM the VMs need.
    ram_end = ram_end_of(mbi);
    available = find_cpus(apic_ids);
    hv_printf("[hv] %lu MiB of RAM, %u CPU(s)\n", ram_end >> 20,
              available);
    if ((needed == 0U) || (needed > HV_MAX_CPUS)) {
        hv_fatal("bad number of CPUs in the configuration");
    }
    if (available < needed) {
        hv_printf("[hv] the VMs need %u CPUs (QEMU: -smp %u)\n",
                  needed, needed);
        hv_fatal("not enough CPUs");
    }
    hv_assign_cpus(apic_ids);
    for (uint32_t i = 0U; i < hv_nb_cpus; i++) {
        cpu_of_apic_id[apic_ids[i]] = (uint8_t) (i + 1U);
    }
    hv_vms_init(0U, ram_end);

    // Start the other CPUs, then run the vCPU of this one.
    hv_smp_start(hv_nb_cpus, apic_ids);
    hv_cpu_main(&hv_cpus[0]);
}

/*
 * Start-up of the other CPUs (called by entry.S): IDT and local APIC, then the
 * CPU runs its vCPU.
 */
void hv_ap_main(uint32_t index)
{
    hv_cpu_t *cpu = &hv_cpus[index];

    hv_idt_load();
    lapic_init_cpu();
    // Tell the boot CPU (waiting in hv_smp_start()) that this one runs.
    x86_barrier();
    cpu->online = 1U;
    hv_cpu_main(cpu);
}
