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
 * Modified by Kirill Brazhnikov, 2026-11-08: only the common part is left
 * (the RAM of the VMs); the loading of the image moved to the
 * architectures (arch/x86_64/multiboot.c), the names of the serial ports
 * too (hv_port_name()). Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and declarations
 * are in include/hv/hv.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from hypervisor/src/vm.c
 * (the common code and its headers are in hypervisor/core).
 */

/**
 * @file
 * Virtual machines: their RAM, a contiguous host region after the
 * hypervisor, one VM after the other in configuration order. Each
 * architecture maps it to the guest (nested paging, stage 2) in
 * hv_arch_vm_init() and loads the image in vm_load().
 */

#include <hv/hv.h>

hv_vm_t hv_vms[HV_MAX_VMS];

/**
 * @funcname `vm_print`
 *
 * @description
 * Prints a message about a VM on the hypervisor console, prefixed with the
 * VM's name.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `const hv_vm_t *` | [in] The VM. |
 * | `message` | `const char *` | [in] The message. |
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
 * 1. Call `hv_printf()` with the arguments `"[hv] %%s: %%s\n"`,
 *    `vm->config->name` and `message`.
 *
 * @calls
 * - `hv_printf()`
 */
void vm_print(const hv_vm_t *vm, const char *message)
{
    hv_printf("[hv] %s: %s\n", vm->config->name, message);
}

/**
 * @funcname `align_up`
 *
 * @description
 * Rounds a value up to a multiple of an alignment.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | [in] The value. |
 * | `alignment` | `uint64_t` | [in] The alignment, a power of 2. |
 *
 * @return value rounded up to a multiple of alignment.
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
 * 1. Return `(value + alignment - 1U) & ~(alignment - 1U)`.
 *
 * @calls
 * None.
 */
static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1U) & ~(alignment - 1U);
}

/**
 * @funcname `hv_vms_init`
 *
 * @description
 * Sets up the VMs of the configuration: their RAM, after the hypervisor in
 * configuration order, and what the architecture needs for each one. Stops
 * the hypervisor if the configuration doesn't fit.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ram_start` | `uint64_t` | [in] The start of the host RAM. |
 * | `ram_end` | `uint64_t` | [in] The end of the host RAM. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `__hv_end` | `uint8_t[]` | The end of the hypervisor image (hv.lds) |
 * | `hv_nb_vms` | `const uint32_t` | The number of VMs |
 * | `hv_vm_configs` | `const hv_vm_config_t[]` | The configuration of each VM |
 * | `hv_vms` | `hv_vm_t[HV_MAX_VMS]` | The VMs |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base` | `uint64_t` | The start of the VMs' RAM (the end of the hypervisor, aligned to a large page) |
 * | `next` | `uint64_t` | The start of the next VM's RAM |
 * | `i` | `uint32_t` | The index of the VM |
 * | `size` | `uint64_t` | The RAM size of a VM |
 * | `needed` | `uint64_t` | The RAM needed, in MiB |
 * | `cfg` | `const hv_vm_config_t *` | The configuration of a VM |
 * | `vm` | `hv_vm_t *` | The VM being set up |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `align_up()` with the arguments `(uint64_t) (uintptr_t) __hv_end` and
 *    `HV_LARGE_PAGE_SIZE`, and store the result in `base`.
 * 2. Set `next` to `base`.
 * 3. If `hv_nb_vms` is greater than `HV_MAX_VMS`:
 *    1. Call `hv_fatal()` with the argument
 *       `"too many VMs in the configuration"`.
 * 4. Set `i` to `0U`; then while `i` is less than `hv_nb_vms`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `size` to `hv_vm_configs[i].memory_size`.
 *    2. If `size` is equal to `0U` or `size` is greater than `HV_VM_MAX_MEMORY`
 *       or `(size % HV_LARGE_PAGE_SIZE)` is not equal to `0U`:
 *       1. Call `hv_fatal()` with the argument
 *          `"bad VM memory size in the configuration"`.
 *    3. Add `size` to `next`.
 * 5. If `next` is greater than `ram_end`:
 *    1. Set `needed` to `(next - ram_start) >> 20`.
 *    2. Call `hv_printf()` with the arguments
 *       `"[hv] the VMs need %%lu MiB of RAM (QEMU: -m %%lu)\n"`, `needed` and
 *       `needed + 16U`.
 *    3. Call `hv_fatal()` with the argument `"not enough RAM"`.
 * 6. Set `next` to `base`.
 * 7. Set `i` to `0U`; then while `i` is less than `hv_nb_vms`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `cfg` to the address of `hv_vm_configs[i]`.
 *    2. Set `vm` to the address of `hv_vms[i]`.
 *    3. Set `vm->config` to `cfg`.
 *    4. Set `vm->index` to `i`.
 *    5. Set `vm->ram_base` to `next`.
 *    6. Add `cfg->memory_size` to `next`.
 *    7. Call `hv_arch_vm_init()` with the argument `vm`.
 *    8. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: %%lu MiB at 0x%%lx, %%u CPU(s) from CPU %%u, "`
 *       `"console %%s, GDB %%s\n"`, `cfg->name`, `cfg->memory_size >> 20`,
 *       `vm->ram_base`, `cfg->nb_cpus`, `vm->first_cpu`,
 *       `hv_port_name(cfg->console_port)` and `hv_port_name(cfg->gdb_port)`.
 *
 * @calls
 * - `align_up()`
 * - `hv_fatal()`
 * - `hv_printf()`
 * - `hv_arch_vm_init()`
 * - `hv_port_name()`
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (hv_vms_init()): the end of the
 * hypervisor is a linker address, converted to a host-physical address
 * (identity-mapped).
 */
void hv_vms_init(uint64_t ram_start, uint64_t ram_end)
{
    uint64_t base = align_up((uint64_t) (uintptr_t) __hv_end,
                             HV_LARGE_PAGE_SIZE);
    uint64_t next = base;

    // First check that the configuration fits, then set the VMs up.
    if (hv_nb_vms > HV_MAX_VMS) {
        hv_fatal("too many VMs in the configuration");
    }
    for (uint32_t i = 0U; i < hv_nb_vms; i++) {
        uint64_t size = hv_vm_configs[i].memory_size;

        if ((size == 0U) || (size > HV_VM_MAX_MEMORY)
            || ((size % HV_LARGE_PAGE_SIZE) != 0U)) {
            hv_fatal("bad VM memory size in the configuration");
        }
        next += size;
    }
    if (next > ram_end) {
        uint64_t needed = (next - ram_start) >> 20;

        hv_printf("[hv] the VMs need %lu MiB of RAM (QEMU: -m %lu)\n",
                  needed, needed + 16U);
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
        hv_arch_vm_init(vm);

        hv_printf("[hv] %s: %lu MiB at 0x%lx, %u CPU(s) from CPU %u, "
                  "console %s, GDB %s\n", cfg->name,
                  cfg->memory_size >> 20, vm->ram_base, cfg->nb_cpus,
                  vm->first_cpu, hv_port_name(cfg->console_port),
                  hv_port_name(cfg->gdb_port));
    }
}
