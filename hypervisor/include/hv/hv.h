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
 * Hypervisor data structures and the interfaces between its parts.
 *
 * Each physical CPU (hv_cpu_t) runs exactly one virtual CPU (struct
 * hv_vcpu) of one virtual machine (hv_vm_t), so there is no vCPU
 * scheduler. Only vCPU 0 of a VM runs guest code (JetOS is single-core);
 * the other vCPUs of the VM are parked. A VM's devices are therefore only
 * used by one physical CPU and need no locks.
 */

#ifndef HV_HV_H
#define HV_HV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <hv/config.h>
#include <hv/svm.h>

#define HV_PAGE_SIZE        0x1000UL
#define HV_LARGE_PAGE_SIZE  0x200000UL      // 2 MiB

// Host interrupt vectors.
#define HV_VECTOR_TIMER     0xF0U
#define HV_VECTOR_SPURIOUS  0xFFU

// Frequency of the 8254 PIT input clock, Hz.
#define HV_PIT_HZ           1193182U

// Guest-physical address of the Multiboot information block.
#define HV_GUEST_MBI        0x9000U

/*
 * GNU attributes.
 * MISRA C:2025 Dir 1.2 deviation: alignment, format checking and
 * no-return are GNU extensions (the code is compiled with -std=gnu99).
 */
#define HV_ALIGNED(n)       __attribute__((aligned(n)))
#define HV_PRINTF(f, a)     __attribute__((format(printf, f, a)))
#define HV_NORETURN         __attribute__((noreturn))

// Virtual 8259A interrupt controller: one chip.
typedef struct {
    uint8_t irr;            // Requested interrupts
    uint8_t isr;            // Interrupts in service
    uint8_t imr;            // Masked interrupts
    uint8_t vector_base;    // ICW2
    uint8_t init_step;      // Next ICW expected (0: none)
    bool single;            // ICW1 SNGL: no ICW3
    bool icw4_needed;
    bool read_isr;          // OCW3: port reads return the ISR
    bool auto_eoi;          // ICW4 AEOI
} vpic_chip_t;

// Master (0) and slave (1) chips, cascaded on IRQ 2.
typedef struct {
    vpic_chip_t chip[2];
} vpic_t;

// Virtual 8254 timer: one channel.
typedef struct {
    uint8_t mode;           // Operating mode 0-5
    uint8_t access;         // 1: LSB, 2: MSB, 3: LSB then MSB
    bool write_msb;         // The next write is the MSB
    bool read_msb;          // The next read is the MSB
    bool latched;           // A counter latch command is pending
    uint8_t written_lsb;
    uint16_t latch;
    uint32_t reload;        // Count, 1-65536
    bool loaded;            // A count was written since the mode was set
    bool counting;          // Loaded and gated on (always for 0 and 1)
    uint64_t start_tsc;     // TSC when counting started
} vpit_channel_t;

typedef struct {
    vpit_channel_t channel[3];
    bool gate2;             // Port 0x61 bit 0
    bool speaker;           // Port 0x61 bit 1
} vpit_t;

// Number of bytes of a bitmap with one bit per I/O port.
#define HV_IO_BITMAP_SIZE   (0x10000U / 8U)

typedef struct {
    const hv_vm_config_t *config;
    uint32_t index;
    uint32_t first_cpu;         // Physical CPU running vCPU 0
    uint64_t ram_base;          // Host-physical base of the guest RAM
    uint64_t npt_root;          // Host-physical address of the NPT PML4
    uint64_t iopm_pa;           // Host-physical address of the IOPM
    uint8_t *iopm;
    vpic_t pic;
    vpit_t pit;
    uint32_t resets;
    uint8_t io_logged[HV_IO_BITMAP_SIZE];   // Unhandled ports reported
} hv_vm_t;

typedef struct {
    hv_vm_t *vm;
    uint32_t index;             // vCPU number within its VM
    uint32_t asid;
    vmcb_t *vmcb;
    uint64_t vmcb_pa;
    hv_guest_regs_t regs;
    bool flush_tlb;             // Flush the guest TLB at the next VMRUN
} hv_vcpu_t;

typedef struct {
    uint32_t index;
    uint32_t apic_id;
    volatile uint32_t online;
    volatile uint32_t timer_ticks;  // LAPIC timer interrupts not yet used
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

// main.c
hv_cpu_t *hv_this_cpu(void);
void hv_cpu_main(hv_cpu_t *cpu) HV_NORETURN;
void hv_halt_forever(void) HV_NORETURN;

// console.c, printf.c
void hv_console_init(uint16_t port);
void hv_console_lock(void);
void hv_console_unlock(void);
void hv_console_write(const char *s, size_t length);
void hv_printf(const char *format, ...) HV_PRINTF(1, 2);
void hv_fatal(const char *message) HV_NORETURN;

// uart.c
void uart_init(uint16_t port);
void uart_putc(uint16_t port, char c);

// string.c
void hv_memset(void *dst, uint8_t value, size_t size);
void hv_memcpy(void *dst, const void *src, size_t size);

// lapic.c
void lapic_init_base(void);
void lapic_init_cpu(void);
uint32_t lapic_id(void);
void lapic_eoi(void);
void lapic_timer_periodic(uint32_t count);
void lapic_timer_oneshot(uint32_t count);
void lapic_timer_stop(void);
uint32_t lapic_timer_current(void);
void lapic_send_init(uint32_t apic_id);
void lapic_send_sipi(uint32_t apic_id, uint8_t vector);

// pit.c: physical timers
void hv_legacy_pic_disable(void);
void hv_timing_init(void);
uint64_t hv_tsc_hz(void);
uint64_t hv_lapic_hz(void);
void hv_udelay(uint64_t microseconds);
uint64_t hv_tsc_to_pit_ticks(uint64_t tsc_ticks);

// acpi.c
uint32_t acpi_find_cpus(uint32_t apic_ids[], uint32_t max);

// idt.c
void hv_idt_init(void);
void hv_idt_load(void);

// smp.c
void hv_smp_start(uint32_t nb_cpus, const uint32_t apic_ids[]);

// svm.c
bool svm_supported(void);
void svm_init(void);
void svm_enable_cpu(hv_cpu_t *cpu);
void svm_vcpu_init(hv_vcpu_t *vcpu);
void svm_vcpu_run(hv_cpu_t *cpu) HV_NORETURN;

// vm.c
void hv_vms_init(uint64_t ram_end);
void vm_load(hv_vm_t *vm);
void vm_print(const hv_vm_t *vm, const char *message);

// npt.c
uint64_t npt_build(uint32_t vm_index, uint64_t ram_base, uint64_t size);

// ioport.c
void io_init_iopm(hv_vm_t *vm);
hv_action_t io_access(hv_vm_t *vm, uint16_t port, uint32_t size,
                      bool is_in, uint32_t *value);

// vpic.c
void vpic_reset(vpic_t *pic);
void vpic_write(vpic_t *pic, uint16_t port, uint8_t value);
uint8_t vpic_read(const vpic_t *pic, uint16_t port);
void vpic_raise(vpic_t *pic, uint32_t irq);
bool vpic_pending(const vpic_t *pic);
bool vpic_next(vpic_t *pic, uint8_t *vector);

// vpit.c
void vpit_reset(vpit_t *pit);
void vpit_write(vpit_t *pit, uint16_t port, uint8_t value);
uint8_t vpit_read(vpit_t *pit, uint16_t port);
void vpit_write_port_b(vpit_t *pit, uint8_t value);
uint8_t vpit_read_port_b(const vpit_t *pit);
bool vpit_timer_running(const vpit_t *pit);

// vuart.c
void vuart_access(uint16_t host_base, uint16_t offset, uint32_t size,
                  bool is_in, uint32_t *value);

#endif // HV_HV_H
