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
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/arm64/main.c (the code of the processor architecture is in
 * hypervisor/csps); system registers are accessed with read_sysreg(),
 * write_sysreg() and isb(), shared with the kernel.
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
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
 * - \hlrid{HLR_MAIN_THIS_MPIDR_01}: `this_mpidr()` shall give the affinity of
 *   the calling CPU.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_001}
 *
 * @llr
 * - \llrid{LLR_MAIN_THIS_MPIDR_01}: `this_mpidr()` shall return MPIDR_EL1
 *   masked with `MPIDR_AFFINITY`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_THIS_MPIDR_01}
 *
 * @design
 * 1. Return `(uint32_t) (read_sysreg(mpidr_el1) & MPIDR_AFFINITY)`.
 *    \llrref{LLR_MAIN_THIS_MPIDR_01}
 *
 * @calls
 * None.
 */
static uint32_t this_mpidr(void)
{
    return (uint32_t) (read_sysreg(mpidr_el1) & MPIDR_AFFINITY);
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
 * - \hlrid{HLR_MAIN_ARM64_HV_ARCH_CPU_ID_01}: `hv_arch_cpu_id()` shall give the
 *   hardware number of the calling CPU.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_001}
 *   - \srqref{SRQ_HV_ERR_003}
 *
 * @llr
 * - \llrid{LLR_MAIN_ARM64_HV_ARCH_CPU_ID_01}: `hv_arch_cpu_id()` shall return
 *   the low 16 bits of the result of `this_mpidr()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_ARCH_CPU_ID_01}
 *
 * @design
 * 1. Return `this_mpidr() & 0xFFFFU`. \llrref{LLR_MAIN_ARM64_HV_ARCH_CPU_ID_01}
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
 * - \hlrid{HLR_MAIN_ARM64_HV_THIS_CPU_01}: `hv_this_cpu()` shall give the CPU
 *   structure of the calling CPU, the first one before the CPUs are known.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_001}
 *
 * @llr
 * - \llrid{LLR_MAIN_ARM64_HV_THIS_CPU_01}: If `hw_id` of one of the
 *   `hv_nb_cpus` CPUs of `hv_cpus` is equal to the result of `this_mpidr()`,
 *   `hv_this_cpu()` shall return the address of the last such CPU.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_THIS_CPU_01}
 * - \llrid{LLR_MAIN_ARM64_HV_THIS_CPU_02}: If `hw_id` of none of the
 *   `hv_nb_cpus` CPUs of `hv_cpus` is equal to the result of `this_mpidr()`,
 *   `hv_this_cpu()` shall return the address of `hv_cpus[0]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_THIS_CPU_01}
 *
 * @design
 * 1. Call `this_mpidr()`, and store the result in `mpidr`.
 *    \llrref{LLR_MAIN_ARM64_HV_THIS_CPU_01},
 *    \llrref{LLR_MAIN_ARM64_HV_THIS_CPU_02}
 * 2. Set `cpu` to the address of `hv_cpus[0]`.
 *    \llrref{LLR_MAIN_ARM64_HV_THIS_CPU_02}
 * 3. Set `i` to `0U`; then while `i` is less than `hv_nb_cpus`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_MAIN_ARM64_HV_THIS_CPU_01},
 *    \llrref{LLR_MAIN_ARM64_HV_THIS_CPU_02}
 *    1. If `hv_cpus[i].hw_id` is equal to `mpidr`:
 *       \llrref{LLR_MAIN_ARM64_HV_THIS_CPU_01}
 *       1. Set `cpu` to the address of `hv_cpus[i]`.
 * 4. Return `cpu`. \llrref{LLR_MAIN_ARM64_HV_THIS_CPU_01},
 *    \llrref{LLR_MAIN_ARM64_HV_THIS_CPU_02}
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
 * - \hlrid{HLR_MAIN_ARM64_HV_HALT_FOREVER_01}: `hv_halt_forever()` shall halt
 *   the calling CPU for good, with its exceptions masked, its virtual timer and
 *   its GIC interrupts off.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_007}
 *   - \srqref{SRQ_HV_ERR_003}
 *
 * @llr
 * - \llrid{LLR_MAIN_ARM64_HV_HALT_FOREVER_01}: `hv_halt_forever()` shall, in
 *   this order, call `arm64_mask_all()`, write 0 to CNTV_CTL_EL0 and call
 *   `gic_cpu_off()`, and then call `arm64_wfi()` again and again, without
 *   returning.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_HALT_FOREVER_01}
 *
 * @design
 * 1. Call `arm64_mask_all()`. \llrref{LLR_MAIN_ARM64_HV_HALT_FOREVER_01}
 * 2. Use the macro `write_sysreg()` with the arguments `cntv_ctl_el0` and
 *    `0U`. \llrref{LLR_MAIN_ARM64_HV_HALT_FOREVER_01}
 * 3. Call `gic_cpu_off()`. \llrref{LLR_MAIN_ARM64_HV_HALT_FOREVER_01}
 * 4. While `true` is true: \llrref{LLR_MAIN_ARM64_HV_HALT_FOREVER_01}
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
    write_sysreg(cntv_ctl_el0, 0U);
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
 * - \hlrid{HLR_MAIN_HV_COUNTER_HZ_01}: `hv_counter_hz()` shall give the
 *   frequency of the system counter.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_005}
 *
 * @llr
 * - \llrid{LLR_MAIN_HV_COUNTER_HZ_01}: `hv_counter_hz()` shall return
 *   CNTFRQ_EL0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_HV_COUNTER_HZ_01}
 *
 * @design
 * 1. Return `read_sysreg(cntfrq_el0)`. \llrref{LLR_MAIN_HV_COUNTER_HZ_01}
 *
 * @calls
 * None.
 */
uint64_t hv_counter_hz(void)
{
    return read_sysreg(cntfrq_el0);
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
 * - \hlrid{HLR_MAIN_HV_UDELAY_01}: `hv_udelay()` shall wait for a number of
 *   microseconds, for the start of the other CPUs.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_002}
 *
 * @llr
 * - \llrid{LLR_MAIN_HV_UDELAY_01}: `hv_udelay()` shall call `isb()`, read
 *   CNTPCT_EL0, and then call `arm64_yield()` while CNTPCT_EL0 minus that first
 *   value is less than `microseconds` times the result of `hv_counter_hz()`
 *   divided by 1000000.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_HV_UDELAY_01}
 *
 * @design
 * 1. Set `ticks` to `(microseconds * hv_counter_hz()) / 1000000U`.
 *    \llrref{LLR_MAIN_HV_UDELAY_01}
 * 2. Call `isb()`. \llrref{LLR_MAIN_HV_UDELAY_01}
 * 3. Set `start` to `read_sysreg(cntpct_el0)`. \llrref{LLR_MAIN_HV_UDELAY_01}
 * 4. While `(read_sysreg(cntpct_el0) - start)` is less than `ticks`:
 *    \llrref{LLR_MAIN_HV_UDELAY_01}
 *    1. Call `arm64_yield()`.
 *
 * @calls
 * - `hv_counter_hz()`
 * - `isb()`
 * - `arm64_yield()`
 */
void hv_udelay(uint64_t microseconds)
{
    uint64_t ticks = (microseconds * hv_counter_hz()) / 1000000U;
    uint64_t start;

    isb();
    start = read_sysreg(cntpct_el0);
    while ((read_sysreg(cntpct_el0) - start) < ticks) {
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
 * - \hlrid{HLR_MAIN_HV_EL2_EXCEPTION_01}: `hv_el2_exception()` shall report an
 *   exception of the hypervisor itself as a fatal error.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_003}
 *   - \srqref{SRQ_HV_INT_012}
 *
 * @llr
 * - \llrid{LLR_MAIN_HV_EL2_EXCEPTION_01}: `hv_el2_exception()` shall call
 *   `hv_printf()` once with `"[hv] %%s at EL2: ESR 0x%%lx ELR 0x%%lx FAR
 *   0x%%lx\n"`, the name of the kind (`"synchronous exception"`, `"IRQ"`,
 *   `"FIQ"` or `"SError"` for the low 2 bits of `kind` from 0 to 3) and
 *   ESR_EL2, ELR_EL2 and FAR_EL2, and then `hv_fatal()` with `"exception in the
 *   hypervisor"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_HV_EL2_EXCEPTION_01}
 *
 * @design
 * 1. Call `hv_printf()` with the arguments
 *    `"[hv] %%s at EL2: ESR 0x%%lx ELR 0x%%lx FAR 0x%%lx\n"`,
 *    `kinds[kind & 3U]`, `read_sysreg(esr_el2)`, `read_sysreg(elr_el2)`
 *    and `read_sysreg(far_el2)`. \llrref{LLR_MAIN_HV_EL2_EXCEPTION_01}
 * 2. Call `hv_fatal()` with the argument `"exception in the hypervisor"`.
 *    \llrref{LLR_MAIN_HV_EL2_EXCEPTION_01}
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
              kinds[kind & 3U], read_sysreg(esr_el2),
              read_sysreg(elr_el2), read_sysreg(far_el2));
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
 * - \hlrid{HLR_MAIN_ARM64_FIND_CPUS_01}: `find_cpus()` shall list the
 *   affinities of the physical CPUs: the boot CPU first, then the others of the
 *   device tree.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_002}
 *
 * @llr
 * - \llrid{LLR_MAIN_ARM64_FIND_CPUS_01}: `find_cpus()` shall store the result
 *   of `this_mpidr()` in `mpidrs[0]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_FIND_CPUS_01}
 * - \llrid{LLR_MAIN_ARM64_FIND_CPUS_02}: If one of the first `nb_cpus`
 *   affinities of `info`, at most `HV_MAX_CPUS` of them, is not the result of
 *   `this_mpidr()` and fewer than `HV_MAX_CPUS` elements of `mpidrs` are
 *   stored, `find_cpus()` shall store it in the next free element of `mpidrs`,
 *   in the order of `info`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_FIND_CPUS_01}
 * - \llrid{LLR_MAIN_ARM64_FIND_CPUS_03}: If one of the first `nb_cpus`
 *   affinities of `info` is the result of `this_mpidr()`, `find_cpus()` shall
 *   not store it again.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_FIND_CPUS_01}
 * - \llrid{LLR_MAIN_ARM64_FIND_CPUS_04}: `find_cpus()` shall return the number
 *   of elements of `mpidrs` that it stored.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_FIND_CPUS_01}
 *
 * @design
 * 1. Call `this_mpidr()`, and store the result in `boot`.
 *    \llrref{LLR_MAIN_ARM64_FIND_CPUS_01},
 *    \llrref{LLR_MAIN_ARM64_FIND_CPUS_02}, \llrref{LLR_MAIN_ARM64_FIND_CPUS_03}
 * 2. Set `found` to `info->nb_cpus` if `info->nb_cpus` is less than
 *    `HV_MAX_CPUS`, otherwise `HV_MAX_CPUS`.
 *    \llrref{LLR_MAIN_ARM64_FIND_CPUS_02}
 * 3. Set `count` to `1U`. \llrref{LLR_MAIN_ARM64_FIND_CPUS_04}
 * 4. Set `mpidrs[0]` to `boot`. \llrref{LLR_MAIN_ARM64_FIND_CPUS_01}
 * 5. Set `i` to `0U`; then while `i` is less than `found` and `count` is less
 *    than `HV_MAX_CPUS`, repeat (after each pass: increment `i`):
 *    \llrref{LLR_MAIN_ARM64_FIND_CPUS_02}, \llrref{LLR_MAIN_ARM64_FIND_CPUS_03}
 *    1. If `info->mpidr[i]` is not equal to `boot`:
 *       1. Set `mpidrs[count]` to `info->mpidr[i]`.
 *       2. Increment `count`.
 * 6. Return `count`. \llrref{LLR_MAIN_ARM64_FIND_CPUS_04}
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
 * - \hlrid{HLR_MAIN_ECAM_MAPPED_01}: `ecam_mapped()` shall tell whether the PCI
 *   configuration space lies in a GiB that the hypervisor maps.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_006}
 *
 * @llr
 * - \llrid{LLR_MAIN_ECAM_MAPPED_01}: If `ecam_size` of `info` is not 0, the
 *   first and the last byte of the PCI configuration space are in the same GiB,
 *   and that GiB is `GIB_DEVICES` or `GIB_HIGH_ECAM`, `ecam_mapped()` shall
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ECAM_MAPPED_01}
 * - \llrid{LLR_MAIN_ECAM_MAPPED_02}: If `ecam_size` of `info` is 0, or the
 *   first and the last byte of the PCI configuration space are in different
 *   GiBs, or their GiB is neither `GIB_DEVICES` nor `GIB_HIGH_ECAM`,
 *   `ecam_mapped()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ECAM_MAPPED_01}
 *
 * @design
 * 1. Set `first` to `info->ecam_base >> GIB_SHIFT`.
 *    \llrref{LLR_MAIN_ECAM_MAPPED_01}, \llrref{LLR_MAIN_ECAM_MAPPED_02}
 * 2. Set `last` to `((info->ecam_base + info->ecam_size) - 1U) >> GIB_SHIFT`.
 *    \llrref{LLR_MAIN_ECAM_MAPPED_01}, \llrref{LLR_MAIN_ECAM_MAPPED_02}
 * 3. Return
 *    `(info->ecam_size != 0U) && (first == last) && ((first == GIB_DEVICES) ||`
 *    `(first == GIB_HIGH_ECAM))`. \llrref{LLR_MAIN_ECAM_MAPPED_01},
 *    \llrref{LLR_MAIN_ECAM_MAPPED_02}
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
 * - \hlrid{HLR_MAIN_FIND_SERIAL_PORTS_01}: `find_serial_ports()` shall record
 *   the serial ports of the host (the second PL011 if QEMU made it, the PCI
 *   16550s) and start the console of the hypervisor.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_006}
 *   - \srqref{SRQ_HV_CON_001}
 * - \hlrid{HLR_MAIN_FIND_SERIAL_PORTS_02}: `find_serial_ports()` shall stop the
 *   hypervisor, with a message on COM1, if the port of its console does not
 *   exist.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_004}
 *
 * @llr
 * - \llrid{LLR_MAIN_FIND_SERIAL_PORTS_01}: If and only if `uart1` of `info` is
 *   true, `find_serial_ports()` shall call `hv_uart_add()` once with
 *   `HV_PORT_COM2`, `HV_HOST_UART1` and true.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_FIND_SERIAL_PORTS_01}
 * - \llrid{LLR_MAIN_FIND_SERIAL_PORTS_02}: If and only if `ecam_mapped()`
 *   returns true for `info`, `find_serial_ports()` shall call
 *   `pci_find_serial_ports()` once with `ecam_base` and `ecam_size` of `info`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_FIND_SERIAL_PORTS_01}
 * - \llrid{LLR_MAIN_FIND_SERIAL_PORTS_03}: If `hv_console_port` is not
 *   `HV_PORT_NONE` and `hv_uart_present()` returns false for it,
 *   `find_serial_ports()` shall call, in this order, `hv_console_init()` with
 *   `HV_PORT_COM1`, `hv_printf()` with `"\n[hv] %%s, the hypervisor's console,
 *   doesn't exist\n"` and the result of `hv_port_name()` for the port, and
 *   `hv_fatal()` with `"the console port is missing"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_FIND_SERIAL_PORTS_02}
 * - \llrid{LLR_MAIN_FIND_SERIAL_PORTS_04}: If `hv_console_port` is
 *   `HV_PORT_NONE`, or `hv_uart_present()` returns true for it,
 *   `find_serial_ports()` shall call `hv_console_init()` once with
 *   `hv_console_port`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_FIND_SERIAL_PORTS_01}
 *
 * @design
 * 1. Set `console` to `hv_console_port`.
 *    \llrref{LLR_MAIN_FIND_SERIAL_PORTS_03},
 *    \llrref{LLR_MAIN_FIND_SERIAL_PORTS_04}
 * 2. If `info->uart1` is true: \llrref{LLR_MAIN_FIND_SERIAL_PORTS_01}
 *    1. Call `hv_uart_add()` with the arguments `HV_PORT_COM2`, `HV_HOST_UART1`
 *       and `true`.
 * 3. If `ecam_mapped(info)` is true: \llrref{LLR_MAIN_FIND_SERIAL_PORTS_02}
 *    1. Call `pci_find_serial_ports()` with the arguments `info->ecam_base` and
 *       `info->ecam_size`.
 * 4. If `console` is not equal to `HV_PORT_NONE` and `hv_uart_present(console)`
 *    is false: \llrref{LLR_MAIN_FIND_SERIAL_PORTS_03}
 *    1. Call `hv_console_init()` with the argument `HV_PORT_COM1`.
 *    2. Call `hv_printf()` with the arguments
 *       `"\n[hv] %%s, the hypervisor's console, doesn't exist\n"` and
 *       `hv_port_name(console)`.
 *    3. Call `hv_fatal()` with the argument `"the console port is missing"`.
 * 5. Call `hv_console_init()` with the argument `console`.
 *    \llrref{LLR_MAIN_FIND_SERIAL_PORTS_04}
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
 * - \hlrid{HLR_MAIN_CHECK_GIC_01}: `check_gic()` shall stop the hypervisor,
 *   with the QEMU option, if the interrupt controller is not a GICv2 with the
 *   virtualization extensions.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_003}
 *   - \srqref{SRQ_HV_BOOT_008}
 *   - \srqref{SRQ_HV_GEN_002}
 *
 * @llr
 * - \llrid{LLR_MAIN_CHECK_GIC_01}: If `gic_v2` of `info` is false,
 *   `check_gic()` shall call `hv_fatal()` with `"a GICv2 is needed (QEMU:
 *   -machine virt,gic-version=2)"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_CHECK_GIC_01}
 * - \llrid{LLR_MAIN_CHECK_GIC_02}: If `gic_v2` of `info` is true, and
 *   `gic_regs` is less than 4 or `gicv_size` is less than `HV_GUEST_GICC_SIZE`,
 *   `check_gic()` shall call `hv_fatal()` with `"the GIC has no virtualization
 *   extensions (QEMU: -machine virt,virtualization=on)"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_CHECK_GIC_01}
 * - \llrid{LLR_MAIN_CHECK_GIC_03}: If `gic_v2` of `info` is true, `gic_regs` is
 *   4 or more and `gicv_size` is not less than `HV_GUEST_GICC_SIZE`,
 *   `check_gic()` shall return without calling `hv_fatal()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_CHECK_GIC_01}
 *
 * @design
 * 1. If `info->gic_v2` is false: \llrref{LLR_MAIN_CHECK_GIC_01},
 *    \llrref{LLR_MAIN_CHECK_GIC_03}
 *    1. Call `hv_fatal()` with the argument
 *       `"a GICv2 is needed (QEMU: -machine virt,gic-version=2)"`.
 * 2. If `info->gic_regs` is less than `4U` or `info->gicv_size` is less than
 *    `HV_GUEST_GICC_SIZE`: \llrref{LLR_MAIN_CHECK_GIC_02},
 *    \llrref{LLR_MAIN_CHECK_GIC_03}
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
 * - \hlrid{HLR_MAIN_ARM64_HV_MAIN_01}: `hv_main()` shall start the hypervisor
 *   on the boot CPU: turn the MMU on, read the device tree, find the serial
 *   ports and start the console, report, check the image and the GIC, find the
 *   CPUs, set up the VMs, start the other CPUs and run its own vCPU, in this
 *   order.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_002}
 *   - \srqref{SRQ_HV_BOOT_005}
 *   - \srqref{SRQ_HV_BOOT_009}
 *   - \srqref{SRQ_HV_BOOT_006}
 *   - \srqref{SRQ_HV_BOOT_001}
 * - \hlrid{HLR_MAIN_ARM64_HV_MAIN_02}: `hv_main()` shall stop with a message if
 *   there is no device tree, or the configuration needs no or too many CPUs, or
 *   more than the computer has.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_BOOT_004}
 *   - \srqref{SRQ_HV_CPU_008}
 *   - \srqref{SRQ_HV_CFG_003}
 *
 * @llr
 * - \llrid{LLR_MAIN_ARM64_HV_MAIN_01}: `hv_main()` shall call, in this order,
 *   `hv_mmu_init()`, `hv_uart_add()` with `HV_PORT_COM1`, `HV_HOST_UART0` and
 *   true, and `fdt_read()` with `HV_HOST_RAM_BASE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_MAIN_01}
 * - \llrid{LLR_MAIN_ARM64_HV_MAIN_02}: If `fdt_read()` returns false,
 *   `hv_main()` shall call `hv_console_init()` with `HV_PORT_COM1` and then
 *   `hv_fatal()` with `"no device tree from QEMU at 0x40000000"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_MAIN_02}
 * - \llrid{LLR_MAIN_ARM64_HV_MAIN_03}: If `fdt_read()` returns true,
 *   `hv_main()` shall, in this order, call `find_serial_ports()`, call
 *   `hv_printf()` with `"\n[hv] JetOS hypervisor (arm64): %%u VM(s), %%u
 *   CPU(s)\n"`, `hv_nb_vms` and the result of `hv_cpus_needed()`, call
 *   `hv_image_check()`, `check_gic()` and `gic_init()`, call `hv_printf()` with
 *   `"[hv] GICv2 with virtualization, %%u list registers; counter %%lu kHz\n"`,
 *   the result of `gic_list_registers()` and the result of `hv_counter_hz()`
 *   divided by 1000, and call `hv_printf()` with `"[hv] %%lu MiB of RAM, %%u
 *   CPU(s)\n"`, the size of the RAM of the device tree shifted right by 20 and
 *   the result of `find_cpus()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_MAIN_01}
 * - \llrid{LLR_MAIN_ARM64_HV_MAIN_04}: If the result of `hv_cpus_needed()` is 0
 *   or more than `HV_MAX_CPUS`, `hv_main()` shall call `hv_fatal()` with `"bad
 *   number of CPUs in the configuration"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_MAIN_02}
 * - \llrid{LLR_MAIN_ARM64_HV_MAIN_05}: If the result of `hv_cpus_needed()` is
 *   from 1 to `HV_MAX_CPUS` and the result of `find_cpus()` is less than it,
 *   `hv_main()` shall call `hv_printf()` with `"[hv] the VMs need %%u CPUs
 *   (QEMU: -smp %%u)\n"` and the number needed twice, and then `hv_fatal()`
 *   with `"not enough CPUs"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_MAIN_02}
 * - \llrid{LLR_MAIN_ARM64_HV_MAIN_06}: If the result of `hv_cpus_needed()` is
 *   from 1 to `HV_MAX_CPUS` and is not more than the result of `find_cpus()`,
 *   `hv_main()` shall call, in this order, `hv_assign_cpus()` with the
 *   affinities found, `hv_vms_init()` with the base and the end of the RAM of
 *   the device tree, `hv_smp_start()` and `hv_cpu_main()` with `hv_cpus[0]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_MAIN_01}
 *
 * @design
 * 1. Call `hv_cpus_needed()`, and store the result in `needed`.
 *    \llrref{LLR_MAIN_ARM64_HV_MAIN_03}, \llrref{LLR_MAIN_ARM64_HV_MAIN_04},
 *    \llrref{LLR_MAIN_ARM64_HV_MAIN_05}, \llrref{LLR_MAIN_ARM64_HV_MAIN_06}
 * 2. Call `hv_mmu_init()`. \llrref{LLR_MAIN_ARM64_HV_MAIN_01}
 * 3. Call `hv_uart_add()` with the arguments `HV_PORT_COM1`, `HV_HOST_UART0`
 *    and `true`. \llrref{LLR_MAIN_ARM64_HV_MAIN_01}
 * 4. If `fdt_read(HV_HOST_RAM_BASE, &info)` is false:
 *    \llrref{LLR_MAIN_ARM64_HV_MAIN_01}, \llrref{LLR_MAIN_ARM64_HV_MAIN_02}
 *    1. Call `hv_console_init()` with the argument `HV_PORT_COM1`.
 *    2. Call `hv_fatal()` with the argument
 *       `"no device tree from QEMU at 0x40000000"`.
 * 5. Call `find_serial_ports()` with the address of `info`.
 *    \llrref{LLR_MAIN_ARM64_HV_MAIN_03}
 * 6. Call `hv_printf()` with the arguments
 *    `"\n[hv] JetOS hypervisor (arm64): %%u VM(s), %%u CPU(s)\n"`, `hv_nb_vms`
 *    and `needed`. \llrref{LLR_MAIN_ARM64_HV_MAIN_03}
 * 7. Call `hv_image_check()`. \llrref{LLR_MAIN_ARM64_HV_MAIN_03}
 * 8. Call `check_gic()` with the address of `info`.
 *    \llrref{LLR_MAIN_ARM64_HV_MAIN_03}
 * 9. Call `gic_init()` with the address of `info`.
 *    \llrref{LLR_MAIN_ARM64_HV_MAIN_03}
 * 10. Call `hv_printf()` with the arguments
 *     `"[hv] GICv2 with virtualization, %%u list registers; "`
 *     `"counter %%lu kHz\n"`, `gic_list_registers()` and
 *     `hv_counter_hz() / 1000U`. \llrref{LLR_MAIN_ARM64_HV_MAIN_03}
 * 11. Call `find_cpus()` with the arguments the address of `info` and `mpidrs`,
 *     and store the result in `available`. \llrref{LLR_MAIN_ARM64_HV_MAIN_03}
 * 12. Call `hv_printf()` with the arguments
 *     `"[hv] %%lu MiB of RAM, %%u CPU(s)\n"`, `info.ram_size >> 20` and
 *     `available`. \llrref{LLR_MAIN_ARM64_HV_MAIN_03}
 * 13. If `needed` is equal to `0U` or `needed` is greater than `HV_MAX_CPUS`:
 *     \llrref{LLR_MAIN_ARM64_HV_MAIN_04}
 *     1. Call `hv_fatal()` with the argument
 *        `"bad number of CPUs in the configuration"`.
 * 14. If `available` is less than `needed`: \llrref{LLR_MAIN_ARM64_HV_MAIN_05}
 *     1. Call `hv_printf()` with the arguments
 *        `"[hv] the VMs need %%u CPUs (QEMU: -smp %%u)\n"`, `needed` and
 *        `needed`.
 *     2. Call `hv_fatal()` with the argument `"not enough CPUs"`.
 * 15. Call `hv_assign_cpus()` with the argument `mpidrs`.
 *     \llrref{LLR_MAIN_ARM64_HV_MAIN_06}
 * 16. Call `hv_vms_init()` with the arguments `info.ram_base` and
 *     `info.ram_base + info.ram_size`. \llrref{LLR_MAIN_ARM64_HV_MAIN_06}
 * 17. Call `hv_smp_start()`. \llrref{LLR_MAIN_ARM64_HV_MAIN_06}
 * 18. Call `hv_cpu_main()` with the address of `hv_cpus[0]`.
 *     \llrref{LLR_MAIN_ARM64_HV_MAIN_06}
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
 * - \hlrid{HLR_MAIN_ARM64_HV_AP_MAIN_01}: `hv_ap_main()` shall start another
 *   CPU: tell the boot CPU that it runs, and run its vCPU.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_002}
 *   - \srqref{SRQ_HV_CPU_003}
 *   - \srqref{SRQ_HV_CPU_009}
 *
 * @llr
 * - \llrid{LLR_MAIN_ARM64_HV_AP_MAIN_01}: `hv_ap_main()` shall, in this order,
 *   call `arm64_dmb_ish()`, set `online` of `hv_cpus[index]` to 1 and call
 *   `hv_cpu_main()` with `hv_cpus[index]`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MAIN_ARM64_HV_AP_MAIN_01}
 *
 * @design
 * 1. Set `cpu` to the address of `hv_cpus[index]`.
 *    \llrref{LLR_MAIN_ARM64_HV_AP_MAIN_01}
 * 2. Call `arm64_dmb_ish()`. \llrref{LLR_MAIN_ARM64_HV_AP_MAIN_01}
 * 3. Set `cpu->online` to `1U`. \llrref{LLR_MAIN_ARM64_HV_AP_MAIN_01}
 * 4. Call `hv_cpu_main()` with the argument `cpu`.
 *    \llrref{LLR_MAIN_ARM64_HV_AP_MAIN_01}
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
