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
 * hypervisor/arch/x86_64/npt.c (the code of the processor architecture is in
 * hypervisor/csps); memset() of the library shared with the kernel replaces
 * hv_memset().
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
 * Nested page tables of the VMs: guest-physical [0, size) is mapped to the
 * VM's host RAM with 2 MiB pages (one PML4, PDPT and page directory per VM,
 * so at most 1 GiB). Everything else is unmapped: an access to it is a
 * nested page fault, which stops the VM.
 */

#include <arch/internal.h>

/**
 * @funcname `npt_pa`
 *
 * @description
 * Returns the physical address of a page table (identity-mapped).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `table` | `const uint64_t *` | [in] The page table. |
 *
 * @return Its physical address.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_NPT_NPT_PA_01}: `npt_pa()` shall give the physical address of a
 *   page table, which the hypervisor maps at the same address.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_MEM_001}
 *
 * @llr
 * - \llrid{LLR_NPT_NPT_PA_01}: `npt_pa()` shall return the address of the
 *   table.
 *   <br>Traces to:
 *   - \hlrref{HLR_NPT_NPT_PA_01}
 *
 * @design
 * 1. Return `(uint64_t) (uintptr_t) table`. \llrref{LLR_NPT_NPT_PA_01}
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation: the table entries hold the physical
 * addresses (identity-mapped) of the next-level tables.
 */
static uint64_t npt_pa(const uint64_t *table)
{
    return (uint64_t) (uintptr_t) table;
}

/**
 * @funcname `npt_build`
 *
 * @description
 * Builds the nested page tables of a VM: guest-physical [0, size) maps to
 * the VM's host RAM with 2 MiB pages, the rest is absent.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm_index` | `uint32_t` | [in] The index of the VM. |
 * | `ram_base` | `uint64_t` | [in] The host-physical start of the VM's RAM. |
 * | `size` | `uint64_t` | [in] The VM's RAM size. |
 *
 * @return The physical address of the PML4.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `npt` | `static npt_tables_t[HV_MAX_VMS]` | The tables of the VMs |
 * | `t` | `npt_tables_t *` | The tables of this VM |
 * | `pages` | `uint64_t` | The number of 2 MiB pages |
 * | `i` | `uint64_t` | The index of the page |
 *
 * @hlr
 * - \hlrid{HLR_NPT_NPT_BUILD_01}: `npt_build()` shall build the nested page
 *   tables of a VM: guest physical addresses from 0 up to the size of its RAM
 *   map to its host RAM with 2 MiB pages, and nothing else is mapped.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_MEM_001}
 *   - \srqref{SRQ_HV_MEM_006}
 *   - \srqref{SRQ_HV_MEM_002}
 *   - \srqref{SRQ_HV_VM_002}
 *   - \srqref{SRQ_HV_DEV_002}
 *   - \srqref{SRQ_HV_VM_009}
 *
 * @llr
 * - \llrid{LLR_NPT_NPT_BUILD_01}: `npt_build()` shall set every byte of the
 *   tables of the VM `vm_index` to 0 with `memset()`, then set entry 0 of their
 *   PML4 to the address of their PDPT (`npt_pa()`) with `NPT_TABLE`, and entry
 *   0 of their PDPT to the address of their page directory with `NPT_TABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_NPT_NPT_BUILD_01}
 * - \llrid{LLR_NPT_NPT_BUILD_02}: For each `i` that is less than `size` divided
 *   by `HV_LARGE_PAGE_SIZE` and less than `NPT_ENTRIES`, `npt_build()` shall
 *   set entry `i` of the page directory to `ram_base` plus `i` times
 *   `HV_LARGE_PAGE_SIZE`, with `NPT_PAGE`, and leave the other entries 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_NPT_NPT_BUILD_01}
 * - \llrid{LLR_NPT_NPT_BUILD_03}: `npt_build()` shall return the address of the
 *   PML4 of the VM (`npt_pa()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_NPT_NPT_BUILD_01}
 *
 * @design
 * 1. Set `t` to the address of `npt[vm_index]`. \llrref{LLR_NPT_NPT_BUILD_01},
 *    \llrref{LLR_NPT_NPT_BUILD_02}, \llrref{LLR_NPT_NPT_BUILD_03}
 * 2. Set `pages` to `size / HV_LARGE_PAGE_SIZE`. \llrref{LLR_NPT_NPT_BUILD_02}
 * 3. Call `memset()` with the arguments `t`, `0` and `sizeof(*t)`.
 *    \llrref{LLR_NPT_NPT_BUILD_01}
 * 4. Set `t->pml4[0]` to `npt_pa(t->pdpt) | NPT_TABLE`.
 *    \llrref{LLR_NPT_NPT_BUILD_01}
 * 5. Set `t->pdpt[0]` to `npt_pa(t->pd) | NPT_TABLE`.
 *    \llrref{LLR_NPT_NPT_BUILD_01}
 * 6. Set `i` to `0U`; then while `i` is less than `pages` and `i` is less than
 *    `NPT_ENTRIES`, repeat (after each pass: increment `i`):
 *    \llrref{LLR_NPT_NPT_BUILD_02}
 *    1. Set `t->pd[i]` to `(ram_base + (i * HV_LARGE_PAGE_SIZE)) | NPT_PAGE`.
 * 7. Call `npt_pa()` with the argument `t->pml4`, and return its result.
 *    \llrref{LLR_NPT_NPT_BUILD_03}
 *
 * @calls
 * - `memset()`
 * - `npt_pa()`
 */
uint64_t npt_build(uint32_t vm_index, uint64_t ram_base, uint64_t size)
{
    static npt_tables_t npt[HV_MAX_VMS] HV_ALIGNED(4096);
    npt_tables_t *t = &npt[vm_index];
    uint64_t pages = size / HV_LARGE_PAGE_SIZE;

    // One entry each in the PML4 and PDPT cover guest-physical 0-1 GiB.
    (void) memset(t, 0, sizeof(*t));
    t->pml4[0] = npt_pa(t->pdpt) | NPT_TABLE;
    t->pdpt[0] = npt_pa(t->pd) | NPT_TABLE;
    // Guest page i is host page ram_base + i * 2 MiB; the rest is absent.
    for (uint64_t i = 0U; (i < pages) && (i < NPT_ENTRIES); i++) {
        t->pd[i] = (ram_base + (i * HV_LARGE_PAGE_SIZE)) | NPT_PAGE;
    }
    return npt_pa(t->pml4);
}
