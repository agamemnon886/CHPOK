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
 * the vCPUs given to the CPUs and the start of a CPU's vCPU.
 */

/*
 * The physical CPUs and their vCPUs: each vCPU of each VM, in
 * configuration order, gets its own CPU; vCPU 0 of a VM runs the guest,
 * the others are parked.
 */

#include <hv/hv.h>

hv_cpu_t hv_cpus[HV_MAX_CPUS];
uint32_t hv_nb_cpus = 0U;

// Total number of CPUs of all VMs.
uint32_t hv_cpus_needed(void)
{
    uint32_t total = 0U;

    for (uint32_t i = 0U; i < hv_nb_vms; i++) {
        total += hv_vm_configs[i].nb_cpus;
    }
    return total;
}

/*
 * Give the vCPUs of the VMs, in configuration order, to the CPUs; hw_ids
 * are the hardware numbers of the CPUs, the boot CPU first.
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

/*
 * Run the vCPU of a CPU: vCPU 0 of a VM loads the VM and runs it, the others
 * are parked. Doesn't return.
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
