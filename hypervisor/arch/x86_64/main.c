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
 * src/cpus.c, for both architectures. Doxygen headers for the file and its
 * functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and external
 * declarations are in arch/internal.h, documented there.
 */

/**
 * @file
 * Start-up of the x86_64 hypervisor: the boot CPU checks its image, the CPU
 * and the configuration, measures the timers, sets up the VMs and starts the
 * other CPUs; then every CPU runs its vCPU (or parks it).
 */

#include <arch/internal.h>

// Index + 1 of the CPU with each local APIC ID (0: none).
static uint8_t cpu_of_apic_id[APIC_IDS];

/**
 * @funcname `hv_halt_forever`
 *
 * @description
 * Halts this CPU for good, with interrupts disabled (a stopped or parked
 * vCPU, a fatal error).
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
 * 1. Call `x86_cli()`.
 * 2. While `true` is true:
 *    1. Call `x86_hlt()`.
 *
 * @calls
 * - `x86_cli()`
 * - `x86_hlt()`
 */
void hv_halt_forever(void)
{
    x86_cli();
    while (true) {
        x86_hlt();
    }
}

/**
 * @funcname `hv_arch_cpu_id`
 *
 * @description
 * Returns the hardware number of the calling CPU: its local APIC ID.
 *
 * @return The local APIC ID.
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
 * 1. Call `lapic_id()`, and return its result.
 *
 * @calls
 * - `lapic_id()`
 */
uint32_t hv_arch_cpu_id(void)
{
    return lapic_id();
}

/**
 * @funcname `hv_this_cpu`
 *
 * @description
 * Returns the CPU structure of the calling CPU, found by its local APIC ID
 * (CPU 0 before the CPUs are known).
 *
 * @return The calling CPU's structure.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu_of_apic_id` | `uint8_t[256]` | Index + 1 of the CPU with each local APIC ID (0: none) |
 * | `hv_cpus` | `hv_cpu_t[HV_MAX_CPUS]` | The CPUs |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `uint32_t` | The local APIC ID |
 * | `index` | `uint32_t` | The index of the CPU |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `lapic_id()`, and store the result in `id`.
 * 2. Set `index` to `0U`.
 * 3. If `id` is less than `APIC_IDS` and `cpu_of_apic_id[id]` is not equal to
 *    `0U`:
 *    1. Set `index` to `(uint32_t) cpu_of_apic_id[id] - 1U`.
 * 4. Return the address of `hv_cpus[index]`.
 *
 * @calls
 * - `lapic_id()`
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

/**
 * @funcname `read_mbi32`
 *
 * @description
 * Reads a 32-bit field of the Multiboot information that the boot loader
 * gives (identity-mapped).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mbi` | `uint32_t` | [in] The physical address of the Multiboot information. |
 * | `offset` | `uint32_t` | [in] The field's offset. |
 *
 * @return The field.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const volatile uint32_t *` | The field |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `(const volatile uint32_t *) ((uintptr_t) mbi + offset)`.
 * 2. Return the value at `p`.
 *
 * @calls
 * None.
 */
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

/**
 * @funcname `ram_end_of`
 *
 * @description
 * Returns the end of the RAM above 1 MiB, from the Multiboot information;
 * stops the hypervisor if the boot loader gave no memory size.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mbi` | `uint32_t` | [in] The physical address of the Multiboot information (0: none). |
 *
 * @return The end of the RAM above 1 MiB.
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
 * 1. If `mbi` is equal to `0U` or `(read_mbi32(mbi, MBI_FLAGS) & MBI_FLAG_MEM)`
 *    is equal to `0U`:
 *    1. Call `hv_fatal()` with the argument
 *       `"the boot loader gave no memory size"`.
 * 2. Return
 *    `MBI_UPPER_MEM_START + ((uint64_t) read_mbi32(mbi, MBI_MEM_UPPER) *`
 *    `1024U)`.
 *
 * @calls
 * - `read_mbi32()`
 * - `hv_fatal()`
 */
static uint64_t ram_end_of(uint32_t mbi)
{
    if ((mbi == 0U) || ((read_mbi32(mbi, MBI_FLAGS) & MBI_FLAG_MEM) == 0U)) {
        hv_fatal("the boot loader gave no memory size");
    }
    // mem_upper: KiB of RAM from 1 MiB up to the first hole.
    return MBI_UPPER_MEM_START
           + ((uint64_t) read_mbi32(mbi, MBI_MEM_UPPER) * 1024U);
}

/**
 * @funcname `find_cpus`
 *
 * @description
 * Lists the local APIC IDs of the physical CPUs: the boot CPU first, then
 * the other enabled CPUs of the ACPI MADT.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `apic_ids` | `uint32_t[HV_MAX_CPUS]` | [out] Where the APIC IDs are stored. |
 *
 * @return The number of CPUs.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `madt_ids` | `uint32_t[HV_MAX_CPUS]` | The APIC IDs of the MADT |
 * | `found` | `uint32_t` | The number of CPUs in the MADT |
 * | `bsp` | `uint32_t` | The boot CPU's APIC ID |
 * | `count` | `uint32_t` | The number of CPUs listed |
 * | `i` | `uint32_t` | The index in madt_ids |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `acpi_find_cpus()` with the arguments `madt_ids` and `HV_MAX_CPUS`,
 *    and store the result in `found`.
 * 2. Call `lapic_id()`, and store the result in `bsp`.
 * 3. Set `count` to `1U`.
 * 4. Set `apic_ids[0]` to `bsp`.
 * 5. Set `i` to `0U`; then while `i` is less than `found`, repeat (after each
 *    pass: increment `i`):
 *    1. If `madt_ids[i]` is not equal to `bsp` and `madt_ids[i]` is less than
 *       `APIC_IDS`:
 *       1. Set `apic_ids[count]` to `madt_ids[i]`.
 *       2. Increment `count`.
 * 6. Return `count`.
 *
 * @calls
 * - `acpi_find_cpus()`
 * - `lapic_id()`
 */
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

/**
 * @funcname `hv_main`
 *
 * @description
 * Start-up on the boot CPU, called by entry.S with the address of the
 * Multiboot information: checks the image, the CPU and the configuration,
 * measures the timers, sets up the VMs, starts the other CPUs, then runs its
 * own vCPU. Doesn't return.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mbi` | `uint32_t` | [in] The physical address of the Multiboot information. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_console_port` | `const uint16_t` | The hypervisor console's port (generated configuration) |
 * | `hv_nb_vms` | `const uint32_t` | The number of VMs |
 * | `cpu_of_apic_id` | `uint8_t[256]` | Index + 1 of the CPU with each local APIC ID |
 * | `hv_nb_cpus` | `uint32_t` | The number of CPUs used |
 * | `hv_cpus` | `hv_cpu_t[HV_MAX_CPUS]` | The CPUs |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `apic_ids` | `uint32_t[HV_MAX_CPUS]` | The APIC IDs of the CPUs |
 * | `needed` | `uint32_t` | The number of CPUs the VMs need |
 * | `ram_end` | `uint64_t` | The end of the RAM |
 * | `available` | `uint32_t` | The number of CPUs of the machine |
 * | `i` | `uint32_t` | The index of a CPU |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `hv_cpus_needed()`, and store the result in `needed`.
 * 2. Call `hv_console_init()` with the argument `hv_console_port`.
 * 3. Call `hv_printf()` with the arguments
 *    `"\n[hv] JetOS hypervisor: %%u VM(s), %%u CPU(s)\n"`, `hv_nb_vms` and
 *    `needed`.
 * 4. Call `hv_image_check()`.
 * 5. Call `lapic_init_base()`.
 * 6. Call `hv_idt_init()`.
 * 7. Call `hv_idt_load()`.
 * 8. Call `hv_legacy_pic_disable()`.
 * 9. Call `lapic_init_cpu()`.
 * 10. If `svm_supported()` is false:
 *     1. Call `hv_fatal()` with the argument
 *        `"AMD SVM with nested paging is needed "`
 *        `"(QEMU: -cpu qemu64,+svm,+npt)"`.
 * 11. Call `svm_init()`.
 * 12. Call `hv_timing_init()`.
 * 13. Call `hv_printf()` with the arguments
 *     `"[hv] TSC %%lu kHz, local APIC timer %%lu kHz\n"`, `hv_tsc_hz() / 1000U`
 *     and `hv_lapic_hz() / 1000U`.
 * 14. Call `ram_end_of()` with the argument `mbi`, and store the result in
 *     `ram_end`.
 * 15. Call `find_cpus()` with the argument `apic_ids`, and store the result in
 *     `available`.
 * 16. Call `hv_printf()` with the arguments
 *     `"[hv] %%lu MiB of RAM, %%u CPU(s)\n"`, `ram_end >> 20` and `available`.
 * 17. If `needed` is equal to `0U` or `needed` is greater than `HV_MAX_CPUS`:
 *     1. Call `hv_fatal()` with the argument
 *        `"bad number of CPUs in the configuration"`.
 * 18. If `available` is less than `needed`:
 *     1. Call `hv_printf()` with the arguments
 *        `"[hv] the VMs need %%u CPUs (QEMU: -smp %%u)\n"`, `needed` and
 *        `needed`.
 *     2. Call `hv_fatal()` with the argument `"not enough CPUs"`.
 * 19. Call `hv_assign_cpus()` with the argument `apic_ids`.
 * 20. Set `i` to `0U`; then while `i` is less than `hv_nb_cpus`, repeat (after
 *     each pass: increment `i`):
 *     1. Set `cpu_of_apic_id[apic_ids[i]]` to `(uint8_t) (i + 1U)`.
 * 21. Call `hv_vms_init()` with the arguments `0U` and `ram_end`.
 * 22. Call `hv_smp_start()` with the arguments `hv_nb_cpus` and `apic_ids`.
 * 23. Call `hv_cpu_main()` with the address of `hv_cpus[0]`.
 *
 * @calls
 * - `hv_cpus_needed()`
 * - `hv_console_init()`
 * - `hv_printf()`
 * - `hv_image_check()`
 * - `lapic_init_base()`
 * - `hv_idt_init()`
 * - `hv_idt_load()`
 * - `hv_legacy_pic_disable()`
 * - `lapic_init_cpu()`
 * - `svm_supported()`
 * - `hv_fatal()`
 * - `svm_init()`
 * - `hv_timing_init()`
 * - `hv_tsc_hz()`
 * - `hv_lapic_hz()`
 * - `ram_end_of()`
 * - `find_cpus()`
 * - `hv_assign_cpus()`
 * - `hv_vms_init()`
 * - `hv_smp_start()`
 * - `hv_cpu_main()`
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

/**
 * @funcname `hv_ap_main`
 *
 * @description
 * Start-up of the other CPUs, called by entry.S: loads the IDT, sets up the
 * local APIC, tells the boot CPU that it runs, then runs its vCPU. Doesn't
 * return.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `index` | `uint32_t` | [in] The index of the CPU. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_cpus` | `hv_cpu_t[HV_MAX_CPUS]` | The CPUs |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | This CPU |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `cpu` to the address of `hv_cpus[index]`.
 * 2. Call `hv_idt_load()`.
 * 3. Call `lapic_init_cpu()`.
 * 4. Call `x86_barrier()`.
 * 5. Set `cpu->online` to `1U`.
 * 6. Call `hv_cpu_main()` with the argument `cpu`.
 *
 * @calls
 * - `hv_idt_load()`
 * - `lapic_init_cpu()`
 * - `x86_barrier()`
 * - `hv_cpu_main()`
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
