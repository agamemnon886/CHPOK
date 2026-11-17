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
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/x86_64/multiboot.c (the code of the processor architecture
 * is in hypervisor/csps); memcpy() and memset() of the library shared with the
 * kernel replace hv_memcpy() and hv_memset().
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
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
 * - \hlrid{HLR_MULTIBOOT_HV_ARCH_VM_INIT_01}: `hv_arch_vm_init()` shall set up
 *   the x86_64 part of a VM once its RAM is placed: its nested page tables and
 *   its I/O permission map.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_MEM_001}
 *   - \srqref{SRQ_HV_DEV_001}
 *
 * @llr
 * - \llrid{LLR_MULTIBOOT_HV_ARCH_VM_INIT_01}: `hv_arch_vm_init()` shall set
 *   `npt_root` of the VM to the result of `npt_build()` for its index, its RAM
 *   base and the memory size of its configuration, set `iopm` to the I/O
 *   permission map of its index and `iopm_pa` to the address of that map, and
 *   then call `io_init_iopm()` once with the VM.
 *   <br>Traces to:
 *   - \hlrref{HLR_MULTIBOOT_HV_ARCH_VM_INIT_01}
 *
 * @design
 * 1. Call `npt_build()` with the arguments `vm->index`, `vm->ram_base` and
 *    `vm->config->memory_size`, and store the result in `vm->arch.npt_root`.
 *    \llrref{LLR_MULTIBOOT_HV_ARCH_VM_INIT_01}
 * 2. Set `vm->arch.iopm` to `iopms[vm->index]`.
 *    \llrref{LLR_MULTIBOOT_HV_ARCH_VM_INIT_01}
 * 3. Set `vm->arch.iopm_pa` to `(uint64_t) (uintptr_t) iopms[vm->index]`.
 *    \llrref{LLR_MULTIBOOT_HV_ARCH_VM_INIT_01}
 * 4. Call `io_init_iopm()` with the argument `vm`.
 *    \llrref{LLR_MULTIBOOT_HV_ARCH_VM_INIT_01}
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
 * - \hlrid{HLR_MULTIBOOT_ZERO_RAM_01}: `zero_ram()` shall clear guest RAM at a
 *   host physical address.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_004}
 *
 * @llr
 * - \llrid{LLR_MULTIBOOT_ZERO_RAM_01}: `zero_ram()` shall write 0 to each of
 *   the `size` divided by 8 64-bit words from the address `pa`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MULTIBOOT_ZERO_RAM_01}
 *
 * @design
 * 1. Set `p` to `(volatile uint64_t *) (uintptr_t) pa`.
 *    \llrref{LLR_MULTIBOOT_ZERO_RAM_01}
 * 2. Set `i` to `0U`; then while `i` is less than `(size / 8U)`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_MULTIBOOT_ZERO_RAM_01}
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
 * - \hlrid{HLR_MULTIBOOT_PUT32_01}: `put32()` shall write a 32-bit value to
 *   guest RAM at a host physical address.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_005}
 *
 * @llr
 * - \llrid{LLR_MULTIBOOT_PUT32_01}: `put32()` shall write the 32-bit value
 *   `value` once at the address `pa`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MULTIBOOT_PUT32_01}
 *
 * @design
 * 1. Set `p` to `(volatile uint32_t *) (uintptr_t) pa`.
 *    \llrref{LLR_MULTIBOOT_PUT32_01}
 * 2. Set `*p` to `value`. \llrref{LLR_MULTIBOOT_PUT32_01}
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
 * - \hlrid{HLR_MULTIBOOT_PUT_MMAP_ENTRY_01}: `put_mmap_entry()` shall write a
 *   RAM entry of the Multiboot memory map.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_005}
 *
 * @llr
 * - \llrid{LLR_MULTIBOOT_PUT_MMAP_ENTRY_01}: `put_mmap_entry()` shall write
 *   with `put32()`, at `pa` and every 4 bytes after it, in this order:
 *   `MMAP_ENTRY_SIZE` minus 4, the low and the high 32 bits of `base`, the low
 *   and the high 32 bits of `length`, and `MMAP_RAM`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MULTIBOOT_PUT_MMAP_ENTRY_01}
 *
 * @design
 * 1. Call `put32()` with the arguments `pa` and `MMAP_ENTRY_SIZE - 4U`.
 *    \llrref{LLR_MULTIBOOT_PUT_MMAP_ENTRY_01}
 * 2. Call `put32()` with the arguments `pa + 4U` and `(uint32_t) base`.
 *    \llrref{LLR_MULTIBOOT_PUT_MMAP_ENTRY_01}
 * 3. Call `put32()` with the arguments `pa + 8U` and `(uint32_t) (base >> 32)`.
 *    \llrref{LLR_MULTIBOOT_PUT_MMAP_ENTRY_01}
 * 4. Call `put32()` with the arguments `pa + 12U` and `(uint32_t) length`.
 *    \llrref{LLR_MULTIBOOT_PUT_MMAP_ENTRY_01}
 * 5. Call `put32()` with the arguments `pa + 16U` and
 *    `(uint32_t) (length >> 32)`. \llrref{LLR_MULTIBOOT_PUT_MMAP_ENTRY_01}
 * 6. Call `put32()` with the arguments `pa + 20U` and `MMAP_RAM`.
 *    \llrref{LLR_MULTIBOOT_PUT_MMAP_ENTRY_01}
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
 * - \hlrid{HLR_MULTIBOOT_WRITE_MBI_01}: `write_mbi()` shall write the Multiboot
 *   information of a VM at its address in the VM's RAM: the memory sizes, a
 *   memory map and the name of the boot loader.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_005}
 *
 * @llr
 * - \llrid{LLR_MULTIBOOT_WRITE_MBI_01}: `write_mbi()` shall write with
 *   `put32()`, in the Multiboot information at `HV_GUEST_MBI` in the RAM of the
 *   VM: `MBI_FLAG_MEM | MBI_FLAG_MMAP | MBI_FLAG_LOADER` in `MBI_FLAGS`,
 *   `LOW_MEMORY_END` divided by 1024 in `MBI_MEM_LOWER`, the memory size of the
 *   configuration minus `HIGH_MEMORY_START`, divided by 1024, in
 *   `MBI_MEM_UPPER`, 2 times `MMAP_ENTRY_SIZE` in `MBI_MMAP_LENGTH`,
 *   `HV_GUEST_MBI` plus `MBI_MMAP_OFFSET` in `MBI_MMAP_ADDR`, and
 *   `HV_GUEST_MBI` plus `MBI_NAME_OFFSET` in `MBI_LOADER_NAME`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MULTIBOOT_WRITE_MBI_01}
 * - \llrid{LLR_MULTIBOOT_WRITE_MBI_02}: `write_mbi()` shall write with
 *   `put_mmap_entry()`, at `MBI_MMAP_OFFSET` of the information, an entry of
 *   `LOW_MEMORY_END` bytes from 0, and after it an entry from
 *   `HIGH_MEMORY_START` of the memory size minus `HIGH_MEMORY_START` bytes, and
 *   copy `"JetOS hypervisor"` with its 0 byte to `MBI_NAME_OFFSET` of the
 *   information with `memcpy()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MULTIBOOT_WRITE_MBI_01}
 *
 * @design
 * 1. Set `mbi` to `vm->ram_base + HV_GUEST_MBI`.
 *    \llrref{LLR_MULTIBOOT_WRITE_MBI_01}, \llrref{LLR_MULTIBOOT_WRITE_MBI_02}
 * 2. Set `size` to `vm->config->memory_size`.
 *    \llrref{LLR_MULTIBOOT_WRITE_MBI_01}, \llrref{LLR_MULTIBOOT_WRITE_MBI_02}
 * 3. Set `name` to `(uint8_t *) (uintptr_t) (mbi + MBI_NAME_OFFSET)`.
 *    \llrref{LLR_MULTIBOOT_WRITE_MBI_02}
 * 4. Call `put32()` with the arguments `mbi + MBI_FLAGS` and
 *    `MBI_FLAG_MEM | MBI_FLAG_MMAP | MBI_FLAG_LOADER`.
 *    \llrref{LLR_MULTIBOOT_WRITE_MBI_01}
 * 5. Call `put32()` with the arguments `mbi + MBI_MEM_LOWER` and
 *    `LOW_MEMORY_END / 1024U`. \llrref{LLR_MULTIBOOT_WRITE_MBI_01}
 * 6. Call `put32()` with the arguments `mbi + MBI_MEM_UPPER` and
 *    `(uint32_t) ((size - HIGH_MEMORY_START) / 1024U)`.
 *    \llrref{LLR_MULTIBOOT_WRITE_MBI_01}
 * 7. Call `put32()` with the arguments `mbi + MBI_MMAP_LENGTH` and
 *    `2U * MMAP_ENTRY_SIZE`. \llrref{LLR_MULTIBOOT_WRITE_MBI_01}
 * 8. Call `put32()` with the arguments `mbi + MBI_MMAP_ADDR` and
 *    `HV_GUEST_MBI + MBI_MMAP_OFFSET`. \llrref{LLR_MULTIBOOT_WRITE_MBI_01}
 * 9. Call `put32()` with the arguments `mbi + MBI_LOADER_NAME` and
 *    `HV_GUEST_MBI + MBI_NAME_OFFSET`. \llrref{LLR_MULTIBOOT_WRITE_MBI_01}
 * 10. Call `put_mmap_entry()` with the arguments `mbi + MBI_MMAP_OFFSET`, `0U`
 *     and `LOW_MEMORY_END`. \llrref{LLR_MULTIBOOT_WRITE_MBI_02}
 * 11. Call `put_mmap_entry()` with the arguments
 *     `mbi + MBI_MMAP_OFFSET + MMAP_ENTRY_SIZE`, `HIGH_MEMORY_START` and
 *     `size - HIGH_MEMORY_START`. \llrref{LLR_MULTIBOOT_WRITE_MBI_02}
 * 12. Call `memcpy()` with the arguments `name`, `loader_name` and
 *     `sizeof(loader_name)`. \llrref{LLR_MULTIBOOT_WRITE_MBI_02}
 *
 * @calls
 * - `put32()`
 * - `put_mmap_entry()`
 * - `memcpy()`
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
    (void) memcpy(name, loader_name, sizeof(loader_name));
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
 * - \hlrid{HLR_MULTIBOOT_VM_LOAD_01}: `vm_load()` shall load a VM at its first
 *   start and at each reset: clear its RAM, copy its image, write the Multiboot
 *   information and reset its devices.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_VM_004}
 *   - \srqref{SRQ_HV_VM_005}
 * - \hlrid{HLR_MULTIBOOT_VM_LOAD_02}: `vm_load()` shall stop, with a message,
 *   if the image does not fit into the RAM of the VM or its entry is outside
 *   it.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_003}
 *
 * @llr
 * - \llrid{LLR_MULTIBOOT_VM_LOAD_01}: If the load address of the configuration
 *   is less than `HIGH_MEMORY_START`, or the load address plus the size of the
 *   image is more than `bss_end`, or `bss_end` is more than `memory_size`, or
 *   `entry` is less than the load address, or `entry` is not less than the load
 *   address plus the size of the image, `vm_load()` shall call `vm_print()`
 *   with the VM and `"image doesn't fit into the VM's RAM"`, and then
 *   `hv_fatal()` with `"bad VM image"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MULTIBOOT_VM_LOAD_02}
 * - \llrid{LLR_MULTIBOOT_VM_LOAD_02}: If the load address of the configuration
 *   is `HIGH_MEMORY_START` or more, the load address plus the size of the image
 *   is not more than `bss_end`, `bss_end` is not more than `memory_size`, and
 *   `entry` is from the load address to below the load address plus the size of
 *   the image, `vm_load()` shall, in this order, call `zero_ram()` with the RAM
 *   base of the VM and `memory_size`, copy the image to the RAM base plus the
 *   load address with `memcpy()`, call `write_mbi()`, `vpic_reset()` for the
 *   PIC of the VM and `vpit_reset()` for its PIT, and set every byte of
 *   `io_logged` of the VM to 0 with `memset()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MULTIBOOT_VM_LOAD_01}
 *
 * @design
 * 1. Set `cfg` to `vm->config`. \llrref{LLR_MULTIBOOT_VM_LOAD_01},
 *    \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 * 2. Set `image_size` to the value at `cfg->image_size`.
 *    \llrref{LLR_MULTIBOOT_VM_LOAD_01}, \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 * 3. Set `load` to `cfg->load_addr`. \llrref{LLR_MULTIBOOT_VM_LOAD_01},
 *    \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 * 4. Set `load_end` to `load + image_size`. \llrref{LLR_MULTIBOOT_VM_LOAD_01},
 *    \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 * 5. If `load` is less than `HIGH_MEMORY_START` or `load_end` is greater than
 *    `cfg->bss_end` or `cfg->bss_end` is greater than `cfg->memory_size` or
 *    `cfg->entry` is less than `load` or `cfg->entry` is greater than or equal
 *    to `load_end`: \llrref{LLR_MULTIBOOT_VM_LOAD_01}
 *    1. Call `vm_print()` with the arguments `vm` and
 *       `"image doesn't fit into the VM's RAM"`.
 *    2. Call `hv_fatal()` with the argument `"bad VM image"`.
 * 6. Call `zero_ram()` with the arguments `vm->ram_base` and
 *    `cfg->memory_size`. \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 * 7. Call `memcpy()` with the arguments
 *    `(uint8_t *) (uintptr_t) (vm->ram_base + load)`, `cfg->image` and
 *    `(size_t) image_size`. \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 * 8. Call `write_mbi()` with the argument `vm`.
 *    \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 * 9. Call `vpic_reset()` with the address of `vm->arch.pic`.
 *    \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 * 10. Call `vpit_reset()` with the address of `vm->arch.pit`.
 *     \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 * 11. Call `memset()` with the arguments `vm->arch.io_logged`, `0` and
 *     `sizeof(vm->arch.io_logged)`. \llrref{LLR_MULTIBOOT_VM_LOAD_02}
 *
 * @calls
 * - `vm_print()`
 * - `hv_fatal()`
 * - `zero_ram()`
 * - `memcpy()`
 * - `write_mbi()`
 * - `vpic_reset()`
 * - `vpit_reset()`
 * - `memset()`
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
    (void) memcpy((uint8_t *) (uintptr_t) (vm->ram_base + load), cfg->image,
                  (size_t) image_size);
    write_mbi(vm);

    // Devices as after power-on; unhandled ports are reported again.
    vpic_reset(&vm->arch.pic);
    vpit_reset(&vm->arch.pit);
    (void) memset(vm->arch.io_logged, 0, sizeof(vm->arch.io_logged));
}
