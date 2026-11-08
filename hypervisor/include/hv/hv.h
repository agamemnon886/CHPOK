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
 * Modified by Kirill Brazhnikov, 2026-11-07: hv_crc32() and
 * hv_image_check() (crc.c).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: only the common part is left
 * here (for x86_64 and arm64); the architecture's own state is the arch
 * member of each structure (<arch/arch.h>), and the x86_64 interfaces
 * moved to arch/x86_64/include/arch/internal.h.
 */

/*
 * Hypervisor data structures and the interfaces between its parts.
 *
 * Each physical CPU (hv_cpu_t) runs exactly one virtual CPU (struct
 * hv_vcpu) of one virtual machine (hv_vm_t), so there is no vCPU
 * scheduler. Only vCPU 0 of a VM runs guest code (JetOS is single-core);
 * the other vCPUs of the VM are parked. A VM's devices are therefore only
 * used by one physical CPU and need no locks.
 *
 * The common code (src/) uses the functions declared below as "arch";
 * each architecture (arch/x86_64, arch/arm64) defines them.
 */

#ifndef HV_HV_H
#define HV_HV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <hv/config.h>
#include <arch/arch.h>

#define HV_PAGE_SIZE        0x1000UL
#define HV_LARGE_PAGE_SIZE  0x200000UL      // 2 MiB

// Guest RAM size limit (one page directory of 2 MiB pages).
#define HV_VM_MAX_MEMORY    0x40000000UL

/*
 * GNU attributes.
 * MISRA C:2025 Dir 1.2 deviation: alignment, format checking and
 * no-return are GNU extensions (the code is compiled with -std=gnu99).
 */
#define HV_ALIGNED(n)       __attribute__((aligned(n)))
#define HV_PRINTF(f, a)     __attribute__((format(printf, f, a)))
#define HV_NORETURN         __attribute__((noreturn))

typedef struct {
    const hv_vm_config_t *config;
    uint32_t index;
    uint32_t first_cpu;         // Physical CPU running vCPU 0
    uint64_t ram_base;          // Host-physical base of the guest RAM
    uint32_t resets;
    hv_arch_vm_t arch;
} hv_vm_t;

typedef struct {
    hv_vm_t *vm;
    uint32_t index;             // vCPU number within its VM
    hv_arch_vcpu_t arch;
} hv_vcpu_t;

typedef struct {
    uint32_t index;
    uint32_t hw_id;             // Local APIC ID (x86_64) or MPIDR (arm64)
    volatile uint32_t online;
    hv_arch_cpu_t arch;
    hv_vcpu_t vcpu;
} hv_cpu_t;

// What the run loop does after an exit was handled.
typedef enum {
    HV_CONTINUE,
    HV_RESET,       // Restart the VM
    HV_STOP         // Stop the VM
} hv_action_t;

extern hv_cpu_t hv_cpus[HV_MAX_CPUS];
extern uint32_t hv_nb_cpus;
extern hv_vm_t hv_vms[HV_MAX_VMS];

// cpus.c
uint32_t hv_cpus_needed(void);
void hv_assign_cpus(const uint32_t hw_ids[HV_MAX_CPUS]);
void hv_cpu_main(hv_cpu_t *cpu) HV_NORETURN;

// console.c, printf.c
void hv_console_init(uint16_t port);
void hv_console_lock(void);
void hv_console_unlock(void);
void hv_console_write(const char *s, size_t length);
void hv_printf(const char *format, ...) HV_PRINTF(1, 2);
void hv_fatal(const char *message) HV_NORETURN;

// crc.c
uint32_t hv_crc32(const uint8_t *data, size_t length);
void hv_image_check(void);

// string.c
void hv_memset(void *dst, uint8_t value, size_t size);
void hv_memcpy(void *dst, const void *src, size_t size);

// vm.c
void hv_vms_init(uint64_t ram_start, uint64_t ram_end);
void vm_print(const hv_vm_t *vm, const char *message);

/*
 * arch: CPUs. hv_this_cpu() is the calling CPU's structure (CPU 0 before
 * the CPUs are known), hv_arch_cpu_id() its hardware number for messages
 * and the console lock.
 */
hv_cpu_t *hv_this_cpu(void);
uint32_t hv_arch_cpu_id(void);
void hv_halt_forever(void) HV_NORETURN;

// arch: the host serial port of a COM port name (HV_PORT_COMn).
void uart_init(uint16_t port);
void uart_putc(uint16_t port, char c);
const char *hv_port_name(uint16_t port);

/*
 * arch: virtual machines. hv_arch_vm_init() sets up what a VM needs once
 * (guest-physical memory, devices), vm_load() (re)loads its image and
 * resets its devices, hv_arch_cpu_enable() prepares a CPU to run guests,
 * hv_arch_vcpu_init() gives a vCPU its state at power-on and
 * hv_arch_vcpu_run() runs it.
 */
void hv_arch_vm_init(hv_vm_t *vm);
void vm_load(hv_vm_t *vm);
void hv_arch_cpu_enable(hv_cpu_t *cpu);
void hv_arch_vcpu_init(hv_vcpu_t *vcpu);
void hv_arch_vcpu_run(hv_cpu_t *cpu) HV_NORETURN;

#endif // HV_HV_H
