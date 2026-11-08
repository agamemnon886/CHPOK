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
 * Interfaces between the x86_64 parts of the hypervisor (arch/x86_64):
 * local APIC, timers, ACPI, IDT, start-up of the other CPUs, SVM, nested
 * paging, I/O ports and the virtual devices.
 */

#ifndef HV_ARCH_INTERNAL_H
#define HV_ARCH_INTERNAL_H

#include <hv/hv.h>

// Host interrupt vectors.
#define HV_VECTOR_TIMER     0xF0U
#define HV_VECTOR_SPURIOUS  0xFFU

// Frequency of the 8254 PIT input clock, Hz.
#define HV_PIT_HZ           1193182U

// Guest-physical address of the Multiboot information block.
#define HV_GUEST_MBI        0x9000U

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

#endif // HV_ARCH_INTERNAL_H
