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
 * EL2 address translation: the hypervisor sees the physical address space
 * through an identity map of 1 GiB blocks (one level 1 table, 39-bit
 * addresses, 4 KiB granule). The RAM of QEMU 'virt' (1 GiB to 4 GiB) is
 * normal write-back memory, the devices (first GiB) and the PCI
 * configuration space (256 GiB, QEMU's high ECAM) are device memory; nothing
 * else is mapped. The MMU and the caches are needed for the exclusive
 * accesses of the console lock and for coherency with the guests, which run
 * with their caches on.
 */

#include <arch/internal.h>

hv_mmu_regs_t hv_el2_mmu;

/**
 * @funcname `hv_mmu_init`
 *
 * @description
 * Builds the EL2 identity map and turns the MMU on, on the boot CPU. It is
 * called first, with the MMU off: the table and hv_el2_mmu are written to
 * memory, where the other CPUs read them with their MMU still off.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_el2_mmu` | `hv_mmu_regs_t` | The EL2 translation registers (MAIR, TCR, TTBR0, SCTLR), also for the other CPUs |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `el2_l1` | `static uint64_t[L1_ENTRIES]` | The level 1 table |
 * | `i` | `uint32_t` | The index of an entry |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `L1_ENTRIES`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `el2_l1[i]` to `0U`.
 * 2. Set `el2_l1[GIB_DEVICES]` to
 *    `((uint64_t) GIB_DEVICES << L1_SHIFT) | DESC_DEVICE`.
 * 3. Set `i` to `GIB_RAM_FIRST`; then while `i` is less than or equal to
 *    `GIB_RAM_LAST`, repeat (after each pass: increment `i`):
 *    1. Set `el2_l1[i]` to `((uint64_t) i << L1_SHIFT) | DESC_NORMAL`.
 * 4. Set `el2_l1[GIB_HIGH_ECAM]` to
 *    `((uint64_t) GIB_HIGH_ECAM << L1_SHIFT) | DESC_DEVICE`.
 * 5. Set `hv_el2_mmu.mair` to `MAIR_VALUE`.
 * 6. Set `hv_el2_mmu.tcr` to `TCR_VALUE`.
 * 7. Set `hv_el2_mmu.ttbr0` to `(uint64_t) (uintptr_t) el2_l1`.
 * 8. Set `hv_el2_mmu.sctlr` to `SCTLR_VALUE`.
 * 9. Call `arm64_dsb_ish()`.
 * 10. Call `hv_mmu_enable()` with the address of `hv_el2_mmu`.
 *
 * @calls
 * - `arm64_dsb_ish()`
 * - `hv_mmu_enable()`
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (hv_mmu_init()): the CPU is given the
 * physical address of the table (identity-mapped), converted from a
 * pointer.
 */
void hv_mmu_init(void)
{
    static uint64_t el2_l1[L1_ENTRIES] HV_ALIGNED(4096);

    for (uint32_t i = 0U; i < L1_ENTRIES; i++) {
        el2_l1[i] = 0U;
    }
    el2_l1[GIB_DEVICES] = ((uint64_t) GIB_DEVICES << L1_SHIFT)
                          | DESC_DEVICE;
    for (uint32_t i = GIB_RAM_FIRST; i <= GIB_RAM_LAST; i++) {
        el2_l1[i] = ((uint64_t) i << L1_SHIFT) | DESC_NORMAL;
    }
    el2_l1[GIB_HIGH_ECAM] = ((uint64_t) GIB_HIGH_ECAM << L1_SHIFT)
                            | DESC_DEVICE;

    hv_el2_mmu.mair = MAIR_VALUE;
    hv_el2_mmu.tcr = TCR_VALUE;
    hv_el2_mmu.ttbr0 = (uint64_t) (uintptr_t) el2_l1;
    hv_el2_mmu.sctlr = SCTLR_VALUE;
    arm64_dsb_ish();
    hv_mmu_enable(&hv_el2_mmu);
}
