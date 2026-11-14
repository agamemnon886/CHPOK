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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file and
 * its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented); the types and
 * macros are documented; the member comments of the types shared with arm64
 * start with x86_64:.
 */

#ifndef HV_ARCH_ARCH_H
#define HV_ARCH_ARCH_H

#include <stdint.h>
#include <stdbool.h>

#include <arch/svm.h>
#include <arch/x86.h>

/**
 * @typename `vpic_chip_t`
 *
 * @description
 * Virtual 8259A interrupt controller: one chip.
 */
typedef struct {
    uint8_t irr; /**< Requested interrupts. */
    uint8_t isr; /**< Interrupts in service. */
    uint8_t imr; /**< Masked interrupts. */
    uint8_t vector_base; /**< ICW2. */
    uint8_t init_step; /**< Next ICW expected (0: none). */
    bool single; /**< ICW1 SNGL: no ICW3. */
    bool icw4_needed; /**< ICW1 IC4: ICW4 is needed. */
    bool read_isr; /**< OCW3: port reads return the ISR. */
    bool auto_eoi; /**< ICW4 AEOI. */
} vpic_chip_t;

// Master (0) and slave (1) chips, cascaded on IRQ 2.

/**
 * @typename `vpic_t`
 *
 * @description
 * The virtual 8259A interrupt controllers of a VM: the master and the slave.
 */
typedef struct {
    vpic_chip_t chip[2]; /**< The master (0) and slave (1) chips. */
} vpic_t;

/**
 * @typename `vpit_channel_t`
 *
 * @description
 * Virtual 8254 timer: one channel.
 */
typedef struct {
    uint8_t mode; /**< Operating mode 0-5. */
    uint8_t access; /**< 1: LSB, 2: MSB, 3: LSB then MSB. */
    bool write_msb; /**< The next write is the MSB. */
    bool read_msb; /**< The next read is the MSB. */
    bool latched; /**< A counter latch command is pending. */
    /**
     * The low byte written, while the high byte is awaited.
     */
    uint8_t written_lsb;
    uint16_t latch; /**< The latched count. */
    uint32_t reload; /**< Count, 1-65536. */
    bool loaded; /**< A count was written since the mode was set. */
    bool counting; /**< Loaded and gated on (always for 0 and 1). */
    uint64_t start_tsc; /**< TSC when counting started. */
} vpit_channel_t;

/**
 * @typename `vpit_t`
 *
 * @description
 * The virtual 8254 timer of a VM: its three channels and the gates that port
 * 0x61 controls.
 */
typedef struct {
    vpit_channel_t channel[3]; /**< The three channels. */
    bool gate2; /**< Port 0x61 bit 0. */
    bool speaker; /**< Port 0x61 bit 1. */
} vpit_t;

/**
 * @macroname `HV_IO_BITMAP_SIZE`
 *
 * @description
 * The number of bytes of a bitmap with one bit per I/O port.
 */
#define HV_IO_BITMAP_SIZE   (0x10000U / 8U)

/**
 * @typename `hv_arch_vm_t`
 *
 * @description
 * The state of a virtual machine that depends on the architecture. x86_64: the
 * nested page tables, the I/O permission map and the virtual PICs and PIT;
 * arm64: the stage 2 translation, the virtual GIC distributor and UARTs, and
 * the timer.
 */
typedef struct {
    uint64_t npt_root; /**< x86_64: Host-physical address of the NPT PML4. */
    uint64_t iopm_pa; /**< x86_64: Host-physical address of the IOPM. */
    uint8_t *iopm; /**< x86_64: The I/O permission map (its virtual address). */
    vpic_t pic; /**< x86_64: The virtual PICs. */
    vpit_t pit; /**< x86_64: The virtual PIT. */
    /**
     * x86_64: Unhandled ports reported.
     */
    uint8_t io_logged[HV_IO_BITMAP_SIZE];
} hv_arch_vm_t;

/**
 * @typename `hv_arch_vcpu_t`
 *
 * @description
 * The state of a virtual CPU that depends on the architecture. x86_64: its VMCB
 * and registers; arm64: its registers.
 */
typedef struct {
    uint32_t asid; /**< x86_64: The address space identifier of the guest. */
    vmcb_t *vmcb; /**< x86_64: The VMCB (its virtual address). */
    uint64_t vmcb_pa; /**< x86_64: The physical address of the VMCB. */
    /**
     * x86_64: The general-purpose registers of the guest that VMRUN does not
     * keep.
     */
    hv_guest_regs_t regs;
    bool flush_tlb; /**< x86_64: Flush the guest TLB at the next VMRUN. */
} hv_arch_vcpu_t;

/**
 * @typename `hv_arch_cpu_t`
 *
 * @description
 * The state of a physical CPU that depends on the architecture. x86_64: the
 * pending timer interrupts; arm64: what was logged.
 */
typedef struct {
    /**
     * x86_64: LAPIC timer interrupts not yet used.
     */
    volatile uint32_t timer_ticks;
} hv_arch_cpu_t;

/*
 * Atomically exchanges a 32-bit word (for the spin locks of the common code).
 */
static inline uint32_t hv_arch_xchg32(volatile uint32_t *word,
                                      uint32_t value)
{
    return x86_xchg32(word, value);
}

/*
 * The spin-wait hint of the common code's spin locks.
 */
static inline void hv_arch_relax(void)
{
    x86_pause();
}

/*
 * A compiler barrier for the common code: memory accesses are not moved across
 * it (x86 doesn't reorder stores with other stores).
 */
static inline void hv_arch_barrier(void)
{
    x86_barrier();
}

/*
 * Disables interrupts on this CPU (before a fatal error is reported).
 */
static inline void hv_arch_irq_disable(void)
{
    x86_cli();
}

#endif // HV_ARCH_ARCH_H
