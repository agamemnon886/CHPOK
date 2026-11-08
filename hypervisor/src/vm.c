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
 * too (hv_port_name()).
 */

/*
 * Virtual machines: their RAM, a contiguous host region after the
 * hypervisor, one VM after the other in configuration order. Each
 * architecture maps it to the guest (nested paging, stage 2) in
 * hv_arch_vm_init() and loads the image in vm_load().
 */

#include <hv/hv.h>

hv_vm_t hv_vms[HV_MAX_VMS];

// End of the hypervisor image (hv.lds).
extern uint8_t __hv_end[];

// Print a message about a VM on the hypervisor console.
void vm_print(const hv_vm_t *vm, const char *message)
{
    hv_printf("[hv] %s: %s\n", vm->config->name, message);
}

// Round value up to a multiple of alignment (a power of 2).
static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1U) & ~(alignment - 1U);
}

/*
 * Set up the VMs of the configuration: RAM (after the hypervisor, checked
 * against the end of the host RAM, ram_end; the host RAM starts at
 * ram_start) and what the architecture needs for each one. Stops the
 * hypervisor if the configuration doesn't fit.
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
