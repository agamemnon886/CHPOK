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
 * Nested page tables of the VMs: guest-physical [0, size) is mapped to
 * the VM's host RAM with 2 MiB pages (one PML4, PDPT and page directory
 * per VM, so at most 1 GiB). Everything else is unmapped: an access to it
 * is a nested page fault, which stops the VM.
 */

#include <hv/hv.h>

#define NPT_ENTRIES     512U
#define NPT_PRESENT     0x001UL
#define NPT_WRITABLE    0x002UL
#define NPT_USER        0x004UL     // Nested walks are user accesses
#define NPT_LARGE       0x080UL
#define NPT_TABLE       (NPT_PRESENT | NPT_WRITABLE | NPT_USER)
#define NPT_PAGE        (NPT_TABLE | NPT_LARGE)

typedef struct {
    uint64_t pml4[NPT_ENTRIES];
    uint64_t pdpt[NPT_ENTRIES];
    uint64_t pd[NPT_ENTRIES];
} npt_tables_t;

// Physical address of a page table (identity-mapped).
/*
 * MISRA C:2025 Rule 11.4 deviation: the table entries hold the physical
 * addresses (identity-mapped) of the next-level tables.
 */
static uint64_t npt_pa(const uint64_t *table)
{
    return (uint64_t) (uintptr_t) table;
}

// Build the tables of VM vm_index; returns the physical PML4 address.
uint64_t npt_build(uint32_t vm_index, uint64_t ram_base, uint64_t size)
{
    static npt_tables_t npt[HV_MAX_VMS] HV_ALIGNED(4096);
    npt_tables_t *t = &npt[vm_index];
    uint64_t pages = size / HV_LARGE_PAGE_SIZE;

    // One entry each in the PML4 and PDPT cover guest-physical 0-1 GiB.
    hv_memset(t, 0U, sizeof(*t));
    t->pml4[0] = npt_pa(t->pdpt) | NPT_TABLE;
    t->pdpt[0] = npt_pa(t->pd) | NPT_TABLE;
    // Guest page i is host page ram_base + i * 2 MiB; the rest is absent.
    for (uint64_t i = 0U; (i < pages) && (i < NPT_ENTRIES); i++) {
        t->pd[i] = (ram_base + (i * HV_LARGE_PAGE_SIZE)) | NPT_PAGE;
    }
    return npt_pa(t->pml4);
}
