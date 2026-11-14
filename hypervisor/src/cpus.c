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
 * Modified by Kirill Brazhnikov, 2026-11-08: moved here from
 * hypervisor/src/main.c (now arch/x86_64/main.c), for both architectures:
 * the vCPUs given to the CPUs and the start of a CPU's vCPU. Doxygen headers
 * for the file and its functions.
 */

/**
 * @file
 * The physical CPUs and their vCPUs: each vCPU of each VM, in configuration
 * order, gets its own CPU; vCPU 0 of a VM runs the guest, the others are
 * parked.
 */

#include <hv/hv.h>

hv_cpu_t hv_cpus[HV_MAX_CPUS];
uint32_t hv_nb_cpus = 0U;

/**
 * @funcname `hv_cpus_needed`
 *
 * @description
 * Returns the number of CPUs that the configuration needs: the total of the
 * vCPUs of all VMs.
 *
 * @return The number of CPUs needed.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_nb_vms` | `const uint32_t` | The number of VMs (generated configuration) |
 * | `hv_vm_configs` | `const hv_vm_config_t[]` | The configuration of each VM |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `total` | `uint32_t` | The sum so far |
 * | `i` | `uint32_t` | The index of the VM |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `total` to `0U`.
 * 2. Set `i` to `0U`; then while `i` is less than `hv_nb_vms`, repeat (after
 *    each pass: increment `i`):
 *    1. Add `hv_vm_configs[i].nb_cpus` to `total`.
 * 3. Return `total`.
 *
 * @calls
 * None.
 */
uint32_t hv_cpus_needed(void)
{
    uint32_t total = 0U;

    for (uint32_t i = 0U; i < hv_nb_vms; i++) {
        total += hv_vm_configs[i].nb_cpus;
    }
    return total;
}

/**
 * @funcname `hv_assign_cpus`
 *
 * @description
 * Gives the vCPUs of the VMs, in configuration order, to the CPUs: vCPU j of
 * a VM runs on the next free CPU. Called once by the boot CPU.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hw_ids` | `const uint32_t[HV_MAX_CPUS]` | [in] The hardware numbers of the CPUs (APIC IDs or MPIDRs), the boot CPU first. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_cpus` | `hv_cpu_t[HV_MAX_CPUS]` | The CPUs |
 * | `hv_nb_cpus` | `uint32_t` | The number of CPUs used |
 * | `hv_nb_vms` | `const uint32_t` | The number of VMs |
 * | `hv_vm_configs` | `const hv_vm_config_t[]` | The configuration of each VM |
 * | `hv_vms` | `hv_vm_t[HV_MAX_VMS]` | The VMs |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `index` | `uint32_t` | The next CPU to give |
 * | `i` | `uint32_t` | The index of the VM |
 * | `j` | `uint32_t` | The index of the vCPU in the VM |
 * | `cpu` | `hv_cpu_t *` | The CPU being given |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `index` to `0U`.
 * 2. Set `i` to `0U`; then while `i` is less than `hv_nb_vms`, repeat (after
 *    each pass: increment `i`):
 *    1. Set `hv_vms[i].first_cpu` to `index`.
 *    2. Set `j` to `0U`; then while `j` is less than
 *       `hv_vm_configs[i].nb_cpus`, repeat (after each pass: increment `j`):
 *       1. Set `cpu` to the address of `hv_cpus[index]`.
 *       2. Set `cpu->index` to `index`.
 *       3. Set `cpu->hw_id` to `hw_ids[index]`.
 *       4. Set `cpu->vcpu.vm` to the address of `hv_vms[i]`.
 *       5. Set `cpu->vcpu.index` to `j`.
 *       6. Increment `index`.
 * 3. Set `hv_nb_cpus` to `index`.
 *
 * @calls
 * None.
 */
void hv_assign_cpus(const uint32_t hw_ids[HV_MAX_CPUS])
{
    uint32_t index = 0U;

    for (uint32_t i = 0U; i < hv_nb_vms; i++) {
        hv_vms[i].first_cpu = index;
        // vCPU j of VM i runs on CPU index; the next VM takes the next CPUs.
        for (uint32_t j = 0U; j < hv_vm_configs[i].nb_cpus; j++) {
            hv_cpu_t *cpu = &hv_cpus[index];

            cpu->index = index;
            cpu->hw_id = hw_ids[index];
            cpu->vcpu.vm = &hv_vms[i];
            cpu->vcpu.index = j;
            index++;
        }
    }
    hv_nb_cpus = index;
}

/**
 * @funcname `hv_cpu_main`
 *
 * @description
 * Runs the vCPU of a CPU: vCPU 0 of a VM loads the VM and runs the guest,
 * the others are parked (JetOS uses one CPU). Called on every CPU after the
 * boot; doesn't return.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cpu` | `hv_cpu_t *` | [in,out] The CPU. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | The CPU's vCPU |
 * | `vm` | `hv_vm_t *` | The vCPU's VM |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `vcpu` to the address of `cpu->vcpu`.
 * 2. Set `vm` to `vcpu->vm`.
 * 3. Call `hv_arch_cpu_enable()` with the argument `cpu`.
 * 4. If `vcpu->index` is equal to `0U`:
 *    1. Call `vm_load()` with the argument `vm`.
 *    2. Call `hv_arch_vcpu_init()` with the argument `vcpu`.
 *    3. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: starts on CPU %%u\n"`, `vm->config->name` and `cpu->index`.
 *    4. Call `hv_arch_vcpu_run()` with the argument `cpu`.
 * 5. Otherwise:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: vCPU %%u parked on CPU %%u " "(the guest uses one CPU)\n"`,
 *       `vm->config->name`, `vcpu->index` and `cpu->index`.
 *    2. Call `hv_halt_forever()`.
 *
 * @calls
 * - `hv_arch_cpu_enable()`
 * - `vm_load()`
 * - `hv_arch_vcpu_init()`
 * - `hv_printf()`
 * - `hv_arch_vcpu_run()`
 * - `hv_halt_forever()`
 */
void hv_cpu_main(hv_cpu_t *cpu)
{
    hv_vcpu_t *vcpu = &cpu->vcpu;
    hv_vm_t *vm = vcpu->vm;

    hv_arch_cpu_enable(cpu);
    if (vcpu->index == 0U) {
        // The VM's first vCPU loads the VM and runs the guest.
        vm_load(vm);
        hv_arch_vcpu_init(vcpu);
        hv_printf("[hv] %s: starts on CPU %u\n", vm->config->name,
                  cpu->index);
        hv_arch_vcpu_run(cpu);
    } else {
        // JetOS uses one CPU: the VM's other vCPUs only keep their CPUs.
        hv_printf("[hv] %s: vCPU %u parked on CPU %u "
                  "(the guest uses one CPU)\n", vm->config->name,
                  vcpu->index, cpu->index);
        hv_halt_forever();
    }
}
