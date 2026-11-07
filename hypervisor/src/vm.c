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
 * Virtual machines: their RAM (a contiguous host region after the
 * hypervisor, mapped from guest-physical 0 by nested paging), loading the
 * image as QEMU's Multiboot loader does (a.out kludge addresses from the
 * Multiboot header, checked by hvconfig.py), and the Multiboot
 * information the guest gets in EBX.
 */

#include <hv/hv.h>

// Guest RAM size limit: one NPT page directory (see npt.c).
#define VM_MAX_MEMORY       0x40000000UL

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

hv_vm_t hv_vms[HV_MAX_VMS];

// End of the hypervisor image (hv.lds).
extern uint8_t __hv_end[];

// Print a message about a VM on the hypervisor console.
void vm_print(const hv_vm_t *vm, const char *message)
{
    hv_printf("[hv] %s: %s\n", vm->config->name, message);
}

// Name of a serial port (COM1-COM4, or "none").
static const char *port_name(uint16_t port)
{
    const char *name;

    switch (port) {
        case HV_PORT_COM1:
            name = "COM1";
            break;
        case HV_PORT_COM2:
            name = "COM2";
            break;
        case HV_PORT_COM3:
            name = "COM3";
            break;
        case HV_PORT_COM4:
            name = "COM4";
            break;
        default:
            name = "none";
            break;
    }
    return name;
}

// Round value up to a multiple of alignment (a power of 2).
static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1U) & ~(alignment - 1U);
}

/*
 * Set up the VMs of the configuration: RAM (after the hypervisor, checked
 * against ram_end), nested page tables and I/O permission maps. Stops the
 * hypervisor if the configuration doesn't fit.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (this file): the guest RAM and the
 * IOPMs are used by host-physical address (identity-mapped), converted
 * to and from pointers, and the end of the hypervisor is a linker
 * address.
 */
void hv_vms_init(uint64_t ram_end)
{
    static uint8_t iopms[HV_MAX_VMS][SVM_IOPM_SIZE] HV_ALIGNED(4096);
    uint64_t base = align_up((uint64_t) (uintptr_t) __hv_end,
                             HV_LARGE_PAGE_SIZE);
    uint64_t next = base;

    // First check that the configuration fits, then set the VMs up.
    if (hv_nb_vms > HV_MAX_VMS) {
        hv_fatal("too many VMs in the configuration");
    }
    for (uint32_t i = 0U; i < hv_nb_vms; i++) {
        uint64_t size = hv_vm_configs[i].memory_size;

        if ((size == 0U) || (size > VM_MAX_MEMORY)
            || ((size % HV_LARGE_PAGE_SIZE) != 0U)) {
            hv_fatal("bad VM memory size in the configuration");
        }
        next += size;
    }
    if (next > ram_end) {
        hv_printf("[hv] the VMs need %lu MiB of RAM (QEMU: -m %lu)\n",
                  (next >> 20), (next >> 20) + 16U);
        hv_fatal("not enough RAM");
    }

    // The VMs' RAM follows the hypervisor, in configuration order.
    next = base;
    for (uint32_t i = 0U; i < hv_nb_vms; i++) {
        const hv_vm_config_t *cfg = &hv_vm_configs[i];
        hv_vm_t *vm = &hv_vms[i];

        vm->config = cfg;
        vm->index = i;
        vm->ram_base = next;
        next += cfg->memory_size;
        vm->npt_root = npt_build(i, vm->ram_base, cfg->memory_size);
        vm->iopm = iopms[i];
        vm->iopm_pa = (uint64_t) (uintptr_t) iopms[i];
        io_init_iopm(vm);

        hv_printf("[hv] %s: %lu MiB at 0x%lx, %u CPU(s) from CPU %u, "
                  "console %s, GDB %s\n", cfg->name,
                  cfg->memory_size >> 20, vm->ram_base, cfg->nb_cpus,
                  vm->first_cpu, port_name(cfg->console_port),
                  port_name(cfg->gdb_port));
    }
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
    vpic_reset(&vm->pic);
    vpit_reset(&vm->pit);
    hv_memset(vm->io_logged, 0U, sizeof(vm->io_logged));
}
