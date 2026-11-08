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
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from hypervisor/src/vm.c
 * (the x86_64 part: nested paging and I/O permission map of a VM,
 * Multiboot loading); hv_arch_vm_init() is the per-VM set-up.
 */

/*
 * Virtual machines on x86_64: their nested page tables and I/O permission
 * map, loading the image as QEMU's Multiboot loader does (a.out kludge
 * addresses from the Multiboot header, checked by hvconfig.py), and the
 * Multiboot information the guest gets in EBX.
 */

#include <arch/internal.h>

// Multiboot information (offsets) and the data it points to.
#define MBI_FLAGS           0U
#define MBI_MEM_LOWER       4U
#define MBI_MEM_UPPER       8U
#define MBI_MMAP_LENGTH     44U
#define MBI_MMAP_ADDR       48U
#define MBI_LOADER_NAME     64U
#define MBI_FLAG_MEM        ((uint32_t) 1U << 0)
#define MBI_FLAG_MMAP       ((uint32_t) 1U << 6)
#define MBI_FLAG_LOADER     ((uint32_t) 1U << 9)
#define MBI_MMAP_OFFSET     0x100U
#define MBI_NAME_OFFSET     0x200U
#define MMAP_ENTRY_SIZE     24U
#define MMAP_RAM            1U
#define LOW_MEMORY_END      0x9FC00U
#define HIGH_MEMORY_START   0x100000U

/*
 * MISRA C:2025 Rule 11.4 deviation (this file): the guest RAM and the
 * IOPMs are used by host-physical address (identity-mapped), converted
 * to and from pointers.
 */

// Nested page tables and I/O permission map of a VM (RAM already placed).
void hv_arch_vm_init(hv_vm_t *vm)
{
    static uint8_t iopms[HV_MAX_VMS][SVM_IOPM_SIZE] HV_ALIGNED(4096);

    vm->arch.npt_root = npt_build(vm->index, vm->ram_base,
                                  vm->config->memory_size);
    vm->arch.iopm = iopms[vm->index];
    vm->arch.iopm_pa = (uint64_t) (uintptr_t) iopms[vm->index];
    io_init_iopm(vm);
}

// Clear size bytes (a multiple of 8) of guest RAM at a host-physical address.
static void zero_ram(uint64_t pa, uint64_t size)
{
    volatile uint64_t *p = (volatile uint64_t *) (uintptr_t) pa;

    for (uint64_t i = 0U; i < (size / 8U); i++) {
        p[i] = 0U;
    }
}

// Write a 32-bit value to guest RAM at a host-physical address.
static void put32(uint64_t pa, uint32_t value)
{
    volatile uint32_t *p = (volatile uint32_t *) (uintptr_t) pa;

    *p = value;
}

// Write one RAM entry of the Multiboot memory map.
static void put_mmap_entry(uint64_t pa, uint64_t base, uint64_t length)
{
    put32(pa, MMAP_ENTRY_SIZE - 4U);    // Size of the rest of the entry
    put32(pa + 4U, (uint32_t) base);
    put32(pa + 8U, (uint32_t) (base >> 32));
    put32(pa + 12U, (uint32_t) length);
    put32(pa + 16U, (uint32_t) (length >> 32));
    put32(pa + 20U, MMAP_RAM);
}

// Multiboot information at HV_GUEST_MBI: memory sizes, map, loader.
static void write_mbi(const hv_vm_t *vm)
{
    static const char loader_name[] = "JetOS hypervisor";
    uint64_t mbi = vm->ram_base + HV_GUEST_MBI;
    uint64_t size = vm->config->memory_size;
    uint8_t *name = (uint8_t *) (uintptr_t) (mbi + MBI_NAME_OFFSET);

    // Fields given: memory sizes (KiB), memory map, boot loader name.
    put32(mbi + MBI_FLAGS, MBI_FLAG_MEM | MBI_FLAG_MMAP | MBI_FLAG_LOADER);
    put32(mbi + MBI_MEM_LOWER, LOW_MEMORY_END / 1024U);
    put32(mbi + MBI_MEM_UPPER, (uint32_t) ((size - HIGH_MEMORY_START)
                                           / 1024U));
    put32(mbi + MBI_MMAP_LENGTH, 2U * MMAP_ENTRY_SIZE);
    put32(mbi + MBI_MMAP_ADDR, HV_GUEST_MBI + MBI_MMAP_OFFSET);
    put32(mbi + MBI_LOADER_NAME, HV_GUEST_MBI + MBI_NAME_OFFSET);
    // RAM below 640 KiB and from 1 MiB to the end of the VM's RAM.
    put_mmap_entry(mbi + MBI_MMAP_OFFSET, 0U, LOW_MEMORY_END);
    put_mmap_entry(mbi + MBI_MMAP_OFFSET + MMAP_ENTRY_SIZE,
                   HIGH_MEMORY_START, size - HIGH_MEMORY_START);
    hv_memcpy(name, loader_name, sizeof(loader_name));
}

/*
 * Load a VM (first start or reset): clear its RAM, copy its image, write the
 * Multiboot information and reset its devices.
 */
void vm_load(hv_vm_t *vm)
{
    const hv_vm_config_t *cfg = vm->config;
    uint64_t image_size = *cfg->image_size;
    uint64_t load = cfg->load_addr;
    uint64_t load_end = load + image_size;

    // hvconfig.py checks this too; the RAM is never written outside.
    if ((load < HIGH_MEMORY_START) || (load_end > cfg->bss_end)
        || (cfg->bss_end > cfg->memory_size) || (cfg->entry < load)
        || (cfg->entry >= load_end)) {
        vm_print(vm, "image doesn't fit into the VM's RAM");
        hv_fatal("bad VM image");
    }

    // Cleared RAM, then the loaded bytes (the BSS after them stays 0).
    zero_ram(vm->ram_base, cfg->memory_size);
    hv_memcpy((uint8_t *) (uintptr_t) (vm->ram_base + load), cfg->image,
              (size_t) image_size);
    write_mbi(vm);

    // Devices as after power-on; unhandled ports are reported again.
    vpic_reset(&vm->arch.pic);
    vpit_reset(&vm->arch.pit);
    hv_memset(vm->arch.io_logged, 0U, sizeof(vm->arch.io_logged));
}
