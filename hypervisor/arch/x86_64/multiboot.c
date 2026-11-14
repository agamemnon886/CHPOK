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
 * Multiboot loading); hv_arch_vm_init() is the per-VM set-up. Doxygen headers
 * for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and external
 * declarations are in arch/internal.h, documented there.
 */

/**
 * @file
 * Virtual machines on x86_64: their nested page tables and I/O permission
 * map, loading the image as QEMU's Multiboot loader does (a.out kludge
 * addresses from the Multiboot header, checked by hvconfig.py), and the
 * Multiboot information the guest gets in EBX.
 */

#include <arch/internal.h>

/*
 * MISRA C:2025 Rule 11.4 deviation (this file): the guest RAM and the
 * IOPMs are used by host-physical address (identity-mapped), converted
 * to and from pointers.
 */

/**
 * @funcname `hv_arch_vm_init`
 *
 * @description
 * Sets up the x86_64 part of a VM, once its RAM is placed: its nested page
 * tables and its I/O permission map.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `iopms` | `static uint8_t[HV_MAX_VMS][SVM_IOPM_SIZE]` | The I/O permission maps of the VMs |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `npt_build()` with the arguments `vm->index`, `vm->ram_base` and
 *    `vm->config->memory_size`, and store the result in `vm->arch.npt_root`.
 * 2. Set `vm->arch.iopm` to `iopms[vm->index]`.
 * 3. Set `vm->arch.iopm_pa` to `(uint64_t) (uintptr_t) iopms[vm->index]`.
 * 4. Call `io_init_iopm()` with the argument `vm`.
 *
 * @calls
 * - `npt_build()`
 * - `io_init_iopm()`
 */
void hv_arch_vm_init(hv_vm_t *vm)
{
    static uint8_t iopms[HV_MAX_VMS][SVM_IOPM_SIZE] HV_ALIGNED(4096);

    vm->arch.npt_root = npt_build(vm->index, vm->ram_base,
                                  vm->config->memory_size);
    vm->arch.iopm = iopms[vm->index];
    vm->arch.iopm_pa = (uint64_t) (uintptr_t) iopms[vm->index];
    io_init_iopm(vm);
}

/**
 * @funcname `zero_ram`
 *
 * @description
 * Clears guest RAM at a host-physical address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The host-physical address. |
 * | `size` | `uint64_t` | [in] The size, a multiple of 8. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `volatile uint64_t *` | The RAM |
 * | `i` | `uint64_t` | The index of the 64-bit word |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `(volatile uint64_t *) (uintptr_t) pa`.
 * 2. Set `i` to `0U`; then while `i` is less than `(size / 8U)`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `p[i]` to `0U`.
 *
 * @calls
 * None.
 */
static void zero_ram(uint64_t pa, uint64_t size)
{
    volatile uint64_t *p = (volatile uint64_t *) (uintptr_t) pa;

    for (uint64_t i = 0U; i < (size / 8U); i++) {
        p[i] = 0U;
    }
}

/**
 * @funcname `put32`
 *
 * @description
 * Writes a 32-bit value to guest RAM at a host-physical address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The host-physical address. |
 * | `value` | `uint32_t` | [in] The value. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `volatile uint32_t *` | The word |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `(volatile uint32_t *) (uintptr_t) pa`.
 * 2. Set `*p` to `value`.
 *
 * @calls
 * None.
 */
static void put32(uint64_t pa, uint32_t value)
{
    volatile uint32_t *p = (volatile uint32_t *) (uintptr_t) pa;

    *p = value;
}

/**
 * @funcname `put_mmap_entry`
 *
 * @description
 * Writes one RAM entry of the Multiboot memory map.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pa` | `uint64_t` | [in] The host-physical address of the entry. |
 * | `base` | `uint64_t` | [in] The guest-physical start of the RAM. |
 * | `length` | `uint64_t` | [in] The length of the RAM. |
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
 * 1. Call `put32()` with the arguments `pa` and `MMAP_ENTRY_SIZE - 4U`.
 * 2. Call `put32()` with the arguments `pa + 4U` and `(uint32_t) base`.
 * 3. Call `put32()` with the arguments `pa + 8U` and `(uint32_t) (base >> 32)`.
 * 4. Call `put32()` with the arguments `pa + 12U` and `(uint32_t) length`.
 * 5. Call `put32()` with the arguments `pa + 16U` and
 *    `(uint32_t) (length >> 32)`.
 * 6. Call `put32()` with the arguments `pa + 20U` and `MMAP_RAM`.
 *
 * @calls
 * - `put32()`
 */
static void put_mmap_entry(uint64_t pa, uint64_t base, uint64_t length)
{
    put32(pa, MMAP_ENTRY_SIZE - 4U);    // Size of the rest of the entry
    put32(pa + 4U, (uint32_t) base);
    put32(pa + 8U, (uint32_t) (base >> 32));
    put32(pa + 12U, (uint32_t) length);
    put32(pa + 16U, (uint32_t) (length >> 32));
    put32(pa + 20U, MMAP_RAM);
}

/**
 * @funcname `write_mbi`
 *
 * @description
 * Writes the Multiboot information of a VM at HV_GUEST_MBI in its RAM: the
 * memory sizes, the memory map and the boot loader's name.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `const hv_vm_t *` | [in] The VM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `loader_name` | `static const char[17]` | The boot loader's name |
 * | `mbi` | `uint64_t` | The host-physical address of the information |
 * | `size` | `uint64_t` | The VM's RAM size |
 * | `name` | `uint8_t *` | Where the boot loader's name goes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `mbi` to `vm->ram_base + HV_GUEST_MBI`.
 * 2. Set `size` to `vm->config->memory_size`.
 * 3. Set `name` to `(uint8_t *) (uintptr_t) (mbi + MBI_NAME_OFFSET)`.
 * 4. Call `put32()` with the arguments `mbi + MBI_FLAGS` and
 *    `MBI_FLAG_MEM | MBI_FLAG_MMAP | MBI_FLAG_LOADER`.
 * 5. Call `put32()` with the arguments `mbi + MBI_MEM_LOWER` and
 *    `LOW_MEMORY_END / 1024U`.
 * 6. Call `put32()` with the arguments `mbi + MBI_MEM_UPPER` and
 *    `(uint32_t) ((size - HIGH_MEMORY_START) / 1024U)`.
 * 7. Call `put32()` with the arguments `mbi + MBI_MMAP_LENGTH` and
 *    `2U * MMAP_ENTRY_SIZE`.
 * 8. Call `put32()` with the arguments `mbi + MBI_MMAP_ADDR` and
 *    `HV_GUEST_MBI + MBI_MMAP_OFFSET`.
 * 9. Call `put32()` with the arguments `mbi + MBI_LOADER_NAME` and
 *    `HV_GUEST_MBI + MBI_NAME_OFFSET`.
 * 10. Call `put_mmap_entry()` with the arguments `mbi + MBI_MMAP_OFFSET`, `0U`
 *     and `LOW_MEMORY_END`.
 * 11. Call `put_mmap_entry()` with the arguments
 *     `mbi + MBI_MMAP_OFFSET + MMAP_ENTRY_SIZE`, `HIGH_MEMORY_START` and
 *     `size - HIGH_MEMORY_START`.
 * 12. Call `hv_memcpy()` with the arguments `name`, `loader_name` and
 *     `sizeof(loader_name)`.
 *
 * @calls
 * - `put32()`
 * - `put_mmap_entry()`
 * - `hv_memcpy()`
 */
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

/**
 * @funcname `vm_load`
 *
 * @description
 * Loads a VM, at its first start and at each reset: clears its RAM, copies
 * its image, writes the Multiboot information and resets its devices.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cfg` | `const hv_vm_config_t *` | The VM's configuration |
 * | `image_size` | `uint64_t` | The size of the image |
 * | `load` | `uint64_t` | The guest-physical load address |
 * | `load_end` | `uint64_t` | The end of the loaded bytes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `cfg` to `vm->config`.
 * 2. Set `image_size` to the value at `cfg->image_size`.
 * 3. Set `load` to `cfg->load_addr`.
 * 4. Set `load_end` to `load + image_size`.
 * 5. If `load` is less than `HIGH_MEMORY_START` or `load_end` is greater than
 *    `cfg->bss_end` or `cfg->bss_end` is greater than `cfg->memory_size` or
 *    `cfg->entry` is less than `load` or `cfg->entry` is greater than or equal
 *    to `load_end`:
 *    1. Call `vm_print()` with the arguments `vm` and
 *       `"image doesn't fit into the VM's RAM"`.
 *    2. Call `hv_fatal()` with the argument `"bad VM image"`.
 * 6. Call `zero_ram()` with the arguments `vm->ram_base` and
 *    `cfg->memory_size`.
 * 7. Call `hv_memcpy()` with the arguments
 *    `(uint8_t *) (uintptr_t) (vm->ram_base + load)`, `cfg->image` and
 *    `(size_t) image_size`.
 * 8. Call `write_mbi()` with the argument `vm`.
 * 9. Call `vpic_reset()` with the address of `vm->arch.pic`.
 * 10. Call `vpit_reset()` with the address of `vm->arch.pit`.
 * 11. Call `hv_memset()` with the arguments `vm->arch.io_logged`, `0U` and
 *     `sizeof(vm->arch.io_logged)`.
 *
 * @calls
 * - `vm_print()`
 * - `hv_fatal()`
 * - `zero_ram()`
 * - `hv_memcpy()`
 * - `write_mbi()`
 * - `vpic_reset()`
 * - `vpit_reset()`
 * - `hv_memset()`
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
