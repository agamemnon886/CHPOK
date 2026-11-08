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
 * x86_64 part of the hypervisor's data structures (the arch members of
 * hv_vm_t, hv_vcpu_t and hv_cpu_t in <hv/hv.h>): the virtual 8259A PIC
 * and 8254 PIT of a VM, its nested page tables and I/O permission map,
 * and the VMCB and saved registers of a vCPU. Also the small functions
 * that the common code uses for locks and halting.
 */

#ifndef HV_ARCH_ARCH_H
#define HV_ARCH_ARCH_H

#include <stdint.h>
#include <stdbool.h>

#include <arch/svm.h>
#include <arch/x86.h>

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
    uint64_t npt_root;          // Host-physical address of the NPT PML4
    uint64_t iopm_pa;           // Host-physical address of the IOPM
    uint8_t *iopm;
    vpic_t pic;
    vpit_t pit;
    uint8_t io_logged[HV_IO_BITMAP_SIZE];   // Unhandled ports reported
} hv_arch_vm_t;

typedef struct {
    uint32_t asid;
    vmcb_t *vmcb;
    uint64_t vmcb_pa;
    hv_guest_regs_t regs;
    bool flush_tlb;             // Flush the guest TLB at the next VMRUN
} hv_arch_vcpu_t;

typedef struct {
    volatile uint32_t timer_ticks;  // LAPIC timer interrupts not yet used
} hv_arch_cpu_t;

// Atomic exchange of a 32-bit word; returns the old value (spin locks).
static inline uint32_t hv_arch_xchg32(volatile uint32_t *word,
                                      uint32_t value)
{
    return x86_xchg32(word, value);
}

// Spin-wait hint.
static inline void hv_arch_relax(void)
{
    x86_pause();
}

// Compiler barrier: memory accesses are not moved across it.
static inline void hv_arch_barrier(void)
{
    x86_barrier();
}

// Disable interrupts on this CPU.
static inline void hv_arch_irq_disable(void)
{
    x86_cli();
}

#endif // HV_ARCH_ARCH_H
