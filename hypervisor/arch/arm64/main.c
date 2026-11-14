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
 * Start-up of the arm64 hypervisor: the boot CPU turns its MMU on, reads
 * QEMU's device tree, finds the serial ports, checks its image, the GIC, the
 * CPUs and the RAM against the configuration, sets up the VMs and starts the
 * other CPUs; then every CPU runs its vCPU (or parks it).
 */

#include <arch/internal.h>

/**
 * @funcname `this_mpidr`
 *
 * @description
 * Returns this CPU's MPIDR, affinity levels 0-2.
 *
 * @return The affinity bits of MPIDR_EL1.
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
 * 1. Return `(uint32_t) (HV_READ_SYSREG(mpidr_el1) & MPIDR_AFFINITY)`.
 *
 * @calls
 * None.
 */
static uint32_t this_mpidr(void)
{
    return (uint32_t) (HV_READ_SYSREG(mpidr_el1) & MPIDR_AFFINITY);
}

/**
 * @funcname `hv_arch_cpu_id`
 *
 * @description
 * Returns the hardware number of the calling CPU: affinity levels 0 and 1 of
 * its MPIDR.
 *
 * @return The CPU's number.
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
 * 1. Return `this_mpidr() & 0xFFFFU`.
 *
 * @calls
 * - `this_mpidr()`
 */
uint32_t hv_arch_cpu_id(void)
{
    return this_mpidr() & 0xFFFFU;
}

/**
 * @funcname `hv_this_cpu`
 *
 * @description
 * Returns the CPU structure of the calling CPU, found by its MPIDR (CPU 0
 * before the CPUs are known).
 *
 * @return The calling CPU's structure.
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
 * | `mpidr` | `uint32_t` | This CPU's MPIDR |
 * | `cpu` | `hv_cpu_t *` | The CPU found (CPU 0 by default) |
 * | `i` | `uint32_t` | The index of a CPU |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `this_mpidr()`, and store the result in `mpidr`.
 * 2. Set `cpu` to the address of `hv_cpus[0]`.
 * 3. Set `i` to `0U`; then while `i` is less than `hv_nb_cpus`, repeat (after
 *    each pass: increment `i`):
 *    1. If `hv_cpus[i].hw_id` is equal to `mpidr`:
 *       1. Set `cpu` to the address of `hv_cpus[i]`.
 * 4. Return `cpu`.
 *
 * @calls
 * - `this_mpidr()`
 */
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

/**
 * @funcname `hv_halt_forever`
 *
 * @description
 * Halts this CPU for good (a stopped or parked vCPU, a fatal error): exceptions
 * masked, its virtual timer and its GIC interrupts off.
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
 * 1. Call `arm64_mask_all()`.
 * 2. Use the macro `HV_WRITE_SYSREG()` with the arguments `cntv_ctl_el0` and
 *    `0U`.
 * 3. Call `gic_cpu_off()`.
 * 4. While `true` is true:
 *    1. Call `arm64_wfi()`.
 *
 * @calls
 * - `arm64_mask_all()`
 * - `gic_cpu_off()`
 * - `arm64_wfi()`
 */
void hv_halt_forever(void)
{
    arm64_mask_all();
    HV_WRITE_SYSREG(cntv_ctl_el0, 0U);
    gic_cpu_off();
    while (true) {
        arm64_wfi();
    }
}

/**
 * @funcname `hv_counter_hz`
 *
 * @description
 * Returns the frequency of the system counter.
 *
 * @return CNTFRQ_EL0, Hz.
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
 * 1. Return `HV_READ_SYSREG(cntfrq_el0)`.
 *
 * @calls
 * None.
 */
uint64_t hv_counter_hz(void)
{
    return HV_READ_SYSREG(cntfrq_el0);
}

/**
 * @funcname `hv_udelay`
 *
 * @description
 * Busy-waits for a number of microseconds, by the system counter.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `microseconds` | `uint64_t` | [in] The time to wait. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ticks` | `uint64_t` | The counter ticks to wait |
 * | `start` | `uint64_t` | The counter at the start |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ticks` to `(microseconds * hv_counter_hz()) / 1000000U`.
 * 2. Call `arm64_isb()`.
 * 3. Set `start` to `HV_READ_SYSREG(cntpct_el0)`.
 * 4. While `(HV_READ_SYSREG(cntpct_el0) - start)` is less than `ticks`:
 *    1. Call `arm64_yield()`.
 *
 * @calls
 * - `hv_counter_hz()`
 * - `arm64_isb()`
 * - `arm64_yield()`
 */
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

/**
 * @funcname `hv_el2_exception`
 *
 * @description
 * Handles an exception of the hypervisor itself (vectors.S): a fatal error.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kind` | `uint64_t` | [in] The kind of exception: 0 synchronous, 1 IRQ, 2 FIQ, 3 SError. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kinds` | `static const char *const[4]` | The names of the kinds |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `hv_printf()` with the arguments
 *    `"[hv] %%s at EL2: ESR 0x%%lx ELR 0x%%lx FAR 0x%%lx\n"`,
 *    `kinds[kind & 3U]`, `HV_READ_SYSREG(esr_el2)`, `HV_READ_SYSREG(elr_el2)`
 *    and `HV_READ_SYSREG(far_el2)`.
 * 2. Call `hv_fatal()` with the argument `"exception in the hypervisor"`.
 *
 * @calls
 * - `hv_printf()`
 * - `hv_fatal()`
 */
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

/**
 * @funcname `find_cpus`
 *
 * @description
 * Lists the MPIDRs of the physical CPUs: this (boot) CPU first, then the others
 * of the device tree.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `info` | `const hv_fdt_info_t *` | [in] The device tree information. |
 * | `mpidrs` | `uint32_t[HV_MAX_CPUS]` | [out] Where the MPIDRs are stored. |
 *
 * @return The number of CPUs.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `boot` | `uint32_t` | This CPU's MPIDR |
 * | `found` | `uint32_t` | The CPUs of the tree (at most HV_MAX_CPUS) |
 * | `count` | `uint32_t` | The number of CPUs listed |
 * | `i` | `uint32_t` | The index of a CPU of the tree |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `this_mpidr()`, and store the result in `boot`.
 * 2. Set `found` to `info->nb_cpus` if `info->nb_cpus` is less than
 *    `HV_MAX_CPUS`, otherwise `HV_MAX_CPUS`.
 * 3. Set `count` to `1U`.
 * 4. Set `mpidrs[0]` to `boot`.
 * 5. Set `i` to `0U`; then while `i` is less than `found` and `count` is less
 *    than `HV_MAX_CPUS`, repeat (after each pass: increment `i`):
 *    1. If `info->mpidr[i]` is not equal to `boot`:
 *       1. Set `mpidrs[count]` to `info->mpidr[i]`.
 *       2. Increment `count`.
 * 6. Return `count`.
 *
 * @calls
 * - `this_mpidr()`
 */
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

/**
 * @funcname `ecam_mapped`
 *
 * @description
 * Tells whether the PCI configuration space is in one GiB that the EL2 map
 * covers (the devices' or the high ECAM's).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `info` | `const hv_fdt_info_t *` | [in] The device tree information. |
 *
 * @return true if the ECAM can be accessed.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `first` | `uint64_t` | The GiB of its start |
 * | `last` | `uint64_t` | The GiB of its end |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `first` to `info->ecam_base >> GIB_SHIFT`.
 * 2. Set `last` to `((info->ecam_base + info->ecam_size) - 1U) >> GIB_SHIFT`.
 * 3. Return
 *    `(info->ecam_size != 0U) && (first == last) && ((first == GIB_DEVICES) ||`
 *    `(first == GIB_HIGH_ECAM))`.
 *
 * @calls
 * None.
 */
static bool ecam_mapped(const hv_fdt_info_t *info)
{
    uint64_t first = info->ecam_base >> GIB_SHIFT;
    uint64_t last = ((info->ecam_base + info->ecam_size) - 1U) >> GIB_SHIFT;

    return (info->ecam_size != 0U) && (first == last)
           && ((first == GIB_DEVICES) || (first == GIB_HIGH_ECAM));
}

/**
 * @funcname `find_serial_ports`
 *
 * @description
 * Records the host's serial ports, then starts the console: COM2 if QEMU made
 * the second PL011, the PCI 16550s. Stops (on COM1) if the console's port is
 * missing.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `info` | `const hv_fdt_info_t *` | [in] The device tree information. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_console_port` | `const uint16_t` | The hypervisor console's port (generated configuration) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `console` | `uint16_t` | The console's port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `console` to `hv_console_port`.
 * 2. If `info->uart1` is true:
 *    1. Call `hv_uart_add()` with the arguments `HV_PORT_COM2`, `HV_HOST_UART1`
 *       and `true`.
 * 3. If `ecam_mapped(info)` is true:
 *    1. Call `pci_find_serial_ports()` with the arguments `info->ecam_base` and
 *       `info->ecam_size`.
 * 4. If `console` is not equal to `HV_PORT_NONE` and `hv_uart_present(console)`
 *    is false:
 *    1. Call `hv_console_init()` with the argument `HV_PORT_COM1`.
 *    2. Call `hv_printf()` with the arguments
 *       `"\n[hv] %%s, the hypervisor's console, doesn't exist\n"` and
 *       `hv_port_name(console)`.
 *    3. Call `hv_fatal()` with the argument `"the console port is missing"`.
 * 5. Call `hv_console_init()` with the argument `console`.
 *
 * @calls
 * - `hv_uart_add()`
 * - `ecam_mapped()`
 * - `pci_find_serial_ports()`
 * - `hv_uart_present()`
 * - `hv_console_init()`
 * - `hv_printf()`
 * - `hv_port_name()`
 * - `hv_fatal()`
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

/**
 * @funcname `check_gic`
 *
 * @description
 * Checks that the GIC is a GICv2 with the virtualization extensions; stops with
 * the QEMU option otherwise.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `info` | `const hv_fdt_info_t *` | [in] The device tree information. |
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
 * 1. If `info->gic_v2` is false:
 *    1. Call `hv_fatal()` with the argument
 *       `"a GICv2 is needed (QEMU: -machine virt,gic-version=2)"`.
 * 2. If `info->gic_regs` is less than `4U` or `info->gicv_size` is less than
 *    `HV_GUEST_GICC_SIZE`:
 *    1. Call `hv_fatal()` with the argument
 *       `"the GIC has no virtualization extensions "`
 *       `"(QEMU: -machine virt,virtualization=on)"`.
 *
 * @calls
 * - `hv_fatal()`
 */
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

/**
 * @funcname `hv_main`
 *
 * @description
 * Start-up on the boot CPU, called by entry.S at EL2 with the MMU off: turns
 * the MMU on, reads the device tree, finds the serial ports, checks the image,
 * GIC, CPUs and RAM, sets up the VMs, starts the other CPUs and runs its own
 * vCPU. Doesn't return.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_nb_vms` | `const uint32_t` | The number of VMs |
 * | `hv_cpus` | `hv_cpu_t[HV_MAX_CPUS]` | The CPUs |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `info` | `hv_fdt_info_t` | The device tree information |
 * | `mpidrs` | `uint32_t[HV_MAX_CPUS]` | The MPIDRs of the CPUs |
 * | `needed` | `uint32_t` | The number of CPUs the VMs need |
 * | `available` | `uint32_t` | The number of CPUs of the machine |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `hv_cpus_needed()`, and store the result in `needed`.
 * 2. Call `hv_mmu_init()`.
 * 3. Call `hv_uart_add()` with the arguments `HV_PORT_COM1`, `HV_HOST_UART0`
 *    and `true`.
 * 4. If `fdt_read(HV_HOST_RAM_BASE, &info)` is false:
 *    1. Call `hv_console_init()` with the argument `HV_PORT_COM1`.
 *    2. Call `hv_fatal()` with the argument
 *       `"no device tree from QEMU at 0x40000000"`.
 * 5. Call `find_serial_ports()` with the address of `info`.
 * 6. Call `hv_printf()` with the arguments
 *    `"\n[hv] JetOS hypervisor (arm64): %%u VM(s), %%u CPU(s)\n"`, `hv_nb_vms`
 *    and `needed`.
 * 7. Call `hv_image_check()`.
 * 8. Call `check_gic()` with the address of `info`.
 * 9. Call `gic_init()` with the address of `info`.
 * 10. Call `hv_printf()` with the arguments
 *     `"[hv] GICv2 with virtualization, %%u list registers; "`
 *     `"counter %%lu kHz\n"`, `gic_list_registers()` and
 *     `hv_counter_hz() / 1000U`.
 * 11. Call `find_cpus()` with the arguments the address of `info` and `mpidrs`,
 *     and store the result in `available`.
 * 12. Call `hv_printf()` with the arguments
 *     `"[hv] %%lu MiB of RAM, %%u CPU(s)\n"`, `info.ram_size >> 20` and
 *     `available`.
 * 13. If `needed` is equal to `0U` or `needed` is greater than `HV_MAX_CPUS`:
 *     1. Call `hv_fatal()` with the argument
 *        `"bad number of CPUs in the configuration"`.
 * 14. If `available` is less than `needed`:
 *     1. Call `hv_printf()` with the arguments
 *        `"[hv] the VMs need %%u CPUs (QEMU: -smp %%u)\n"`, `needed` and
 *        `needed`.
 *     2. Call `hv_fatal()` with the argument `"not enough CPUs"`.
 * 15. Call `hv_assign_cpus()` with the argument `mpidrs`.
 * 16. Call `hv_vms_init()` with the arguments `info.ram_base` and
 *     `info.ram_base + info.ram_size`.
 * 17. Call `hv_smp_start()`.
 * 18. Call `hv_cpu_main()` with the address of `hv_cpus[0]`.
 *
 * @calls
 * - `hv_cpus_needed()`
 * - `hv_mmu_init()`
 * - `hv_uart_add()`
 * - `fdt_read()`
 * - `hv_console_init()`
 * - `hv_fatal()`
 * - `find_serial_ports()`
 * - `hv_printf()`
 * - `hv_image_check()`
 * - `check_gic()`
 * - `gic_init()`
 * - `gic_list_registers()`
 * - `hv_counter_hz()`
 * - `find_cpus()`
 * - `hv_assign_cpus()`
 * - `hv_vms_init()`
 * - `hv_smp_start()`
 * - `hv_cpu_main()`
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

/**
 * @funcname `hv_ap_main`
 *
 * @description
 * Start-up of the other CPUs, called by entry.S with the MMU on: tells the boot
 * CPU that it runs, then runs its vCPU. Doesn't return.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `index` | `uint64_t` | [in] The index of the CPU (the CPU_ON context ID). |
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
 * 2. Call `arm64_dmb_ish()`.
 * 3. Set `cpu->online` to `1U`.
 * 4. Call `hv_cpu_main()` with the argument `cpu`.
 *
 * @calls
 * - `arm64_dmb_ish()`
 * - `hv_cpu_main()`
 */
void hv_ap_main(uint64_t index)
{
    hv_cpu_t *cpu = &hv_cpus[index];

    // Tell the boot CPU (waiting in hv_smp_start()) that this one runs.
    arm64_dmb_ish();
    cpu->online = 1U;
    hv_cpu_main(cpu);
}
