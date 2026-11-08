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
 * Start-up of the arm64 hypervisor: the boot CPU turns its MMU on, reads
 * QEMU's device tree, finds the serial ports, checks its image, the GIC,
 * the CPUs and the RAM against the configuration, sets up the VMs and
 * starts the other CPUs; then every CPU runs its vCPU (or parks it).
 */

#include <arch/internal.h>

#define MPIDR_AFFINITY      0xFFFFFFUL      // Affinity levels 0-2
#define GIB_SHIFT           30U
#define GIB_DEVICES         0U              // Mapped by mmu.c
#define GIB_HIGH_ECAM       256U

void hv_main(void);
void hv_ap_main(uint64_t index);
void hv_el2_exception(uint64_t kind);

// This CPU's MPIDR, affinity levels 0-2.
static uint32_t this_mpidr(void)
{
    return (uint32_t) (HV_READ_SYSREG(mpidr_el1) & MPIDR_AFFINITY);
}

// Hardware number of the calling CPU: affinity levels 0 and 1.
uint32_t hv_arch_cpu_id(void)
{
    return this_mpidr() & 0xFFFFU;
}

// The CPU structure of the calling CPU (CPU 0 before the CPUs are known).
hv_cpu_t *hv_this_cpu(void)
{
    uint32_t mpidr = this_mpidr();
    hv_cpu_t *cpu = &hv_cpus[0];

    for (uint32_t i = 0U; i < hv_nb_cpus; i++) {
        if (hv_cpus[i].hw_id == mpidr) {
            cpu = &hv_cpus[i];
        }
    }
    return cpu;
}

// Halt this CPU for good: exceptions masked, its interrupts off.
void hv_halt_forever(void)
{
    arm64_mask_all();
    HV_WRITE_SYSREG(cntv_ctl_el0, 0U);
    gic_cpu_off();
    while (true) {
        arm64_wfi();
    }
}

// Frequency of the system counter, Hz.
uint64_t hv_counter_hz(void)
{
    return HV_READ_SYSREG(cntfrq_el0);
}

// Wait for the given number of microseconds (system counter).
void hv_udelay(uint64_t microseconds)
{
    uint64_t ticks = (microseconds * hv_counter_hz()) / 1000000U;
    uint64_t start;

    arm64_isb();
    start = HV_READ_SYSREG(cntpct_el0);
    while ((HV_READ_SYSREG(cntpct_el0) - start) < ticks) {
        arm64_yield();
    }
}

// An exception of the hypervisor itself (vectors.S): a fatal error.
void hv_el2_exception(uint64_t kind)
{
    static const char *const kinds[4] = {
        "synchronous exception", "IRQ", "FIQ", "SError"
    };

    hv_printf("[hv] %s at EL2: ESR 0x%lx ELR 0x%lx FAR 0x%lx\n",
              kinds[kind & 3U], HV_READ_SYSREG(esr_el2),
              HV_READ_SYSREG(elr_el2), HV_READ_SYSREG(far_el2));
    hv_fatal("exception in the hypervisor");
}

// Physical CPUs: this (boot) CPU first, then the others of the tree.
static uint32_t find_cpus(const hv_fdt_info_t *info,
                          uint32_t mpidrs[HV_MAX_CPUS])
{
    uint32_t boot = this_mpidr();
    uint32_t found = (info->nb_cpus < HV_MAX_CPUS) ? info->nb_cpus
                                                   : HV_MAX_CPUS;
    uint32_t count = 1U;

    mpidrs[0] = boot;
    for (uint32_t i = 0U; (i < found) && (count < HV_MAX_CPUS); i++) {
        if (info->mpidr[i] != boot) {
            mpidrs[count] = info->mpidr[i];
            count++;
        }
    }
    return count;
}

// Whether the PCI configuration space is in a GiB that mmu.c maps.
static bool ecam_mapped(const hv_fdt_info_t *info)
{
    uint64_t first = info->ecam_base >> GIB_SHIFT;
    uint64_t last = ((info->ecam_base + info->ecam_size) - 1U) >> GIB_SHIFT;

    return (info->ecam_size != 0U) && (first == last)
           && ((first == GIB_DEVICES) || (first == GIB_HIGH_ECAM));
}

/*
 * The host's serial ports: COM1 (the first PL011, always there), COM2 (if
 * QEMU made the second PL011) and the PCI 16550s. Then the console.
 */
static void find_serial_ports(const hv_fdt_info_t *info)
{
    uint16_t console = hv_console_port;

    if (info->uart1) {
        hv_uart_add(HV_PORT_COM2, HV_HOST_UART1, true);
    }
    if (ecam_mapped(info)) {
        pci_find_serial_ports(info->ecam_base, info->ecam_size);
    }
    if ((console != HV_PORT_NONE) && !hv_uart_present(console)) {
        // Tell it on COM1, the only port surely there.
        hv_console_init(HV_PORT_COM1);
        hv_printf("\n[hv] %s, the hypervisor's console, doesn't exist\n",
                  hv_port_name(console));
        hv_fatal("the console port is missing");
    }
    hv_console_init(console);
}

// The GIC must be a GICv2 with the virtualization extensions.
static void check_gic(const hv_fdt_info_t *info)
{
    if (!info->gic_v2) {
        hv_fatal("a GICv2 is needed (QEMU: -machine virt,gic-version=2)");
    }
    if ((info->gic_regs < 4U) || (info->gicv_size < HV_GUEST_GICC_SIZE)) {
        hv_fatal("the GIC has no virtualization extensions "
                 "(QEMU: -machine virt,virtualization=on)");
    }
}

/*
 * Start-up on the boot CPU (called by entry.S at EL2, MMU off): see the
 * comment at the top of the file. Runs its own vCPU at the end.
 */
void hv_main(void)
{
    hv_fdt_info_t info;
    uint32_t mpidrs[HV_MAX_CPUS];
    uint32_t needed = hv_cpus_needed();
    uint32_t available;

    hv_mmu_init();
    hv_uart_add(HV_PORT_COM1, HV_HOST_UART0, true);
    if (!fdt_read(HV_HOST_RAM_BASE, &info)) {
        hv_console_init(HV_PORT_COM1);
        hv_fatal("no device tree from QEMU at 0x40000000");
    }
    find_serial_ports(&info);
    hv_printf("\n[hv] JetOS hypervisor (arm64): %u VM(s), %u CPU(s)\n",
              hv_nb_vms, needed);

    // The image must be the one the build made (stops if not).
    hv_image_check();

    check_gic(&info);
    gic_init(&info);
    hv_printf("[hv] GICv2 with virtualization, %u list registers; "
              "counter %lu kHz\n", gic_list_registers(),
              hv_counter_hz() / 1000U);

    // The machine must have the CPUs and the RAM the VMs need.
    available = find_cpus(&info, mpidrs);
    hv_printf("[hv] %lu MiB of RAM, %u CPU(s)\n", info.ram_size >> 20,
              available);
    if ((needed == 0U) || (needed > HV_MAX_CPUS)) {
        hv_fatal("bad number of CPUs in the configuration");
    }
    if (available < needed) {
        hv_printf("[hv] the VMs need %u CPUs (QEMU: -smp %u)\n",
                  needed, needed);
        hv_fatal("not enough CPUs");
    }
    hv_assign_cpus(mpidrs);
    hv_vms_init(info.ram_base, info.ram_base + info.ram_size);

    // Start the other CPUs, then run the vCPU of this one.
    hv_smp_start();
    hv_cpu_main(&hv_cpus[0]);
}

/*
 * Start-up of the other CPUs (called by entry.S, MMU on): the CPU runs its
 * vCPU.
 */
void hv_ap_main(uint64_t index)
{
    hv_cpu_t *cpu = &hv_cpus[index];

    // Tell the boot CPU (waiting in hv_smp_start()) that this one runs.
    arm64_dmb_ish();
    cpu->online = 1U;
    hv_cpu_main(cpu);
}
