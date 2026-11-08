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
 * arm64 part of the hypervisor's data structures (the arch members of
 * hv_vm_t, hv_vcpu_t and hv_cpu_t in <hv/hv.h>): a VM's stage-2
 * translation, its emulated GIC distributor and PL011s, and the saved
 * registers of a vCPU. Also the small functions that the common code uses
 * for locks and halting.
 */

#ifndef HV_ARCH_ARCH_H
#define HV_ARCH_ARCH_H

#include <stdint.h>
#include <stdbool.h>

#include <arch/arm64.h>

/*
 * Guest registers while the hypervisor runs, and the hypervisor's stack
 * pointer while the guest runs. vectors.S uses the offsets (checked in
 * vcpu.c): x at 0, elr at 248, spsr at 256, host_sp at 264.
 */
typedef struct {
    uint64_t x[31];             // X0-X30
    uint64_t elr;               // ELR_EL2: where the guest goes on
    uint64_t spsr;              // SPSR_EL2: its PSTATE
    uint64_t host_sp;           // SP_EL2 in hv_guest_enter()
} hv_guest_regs_t;

// Emulated GICv2 distributor of a VM: interrupts 0-31 (one CPU).
typedef struct {
    bool enabled;               // GICD_CTLR.EnableGrp0
    uint32_t enable;            // Enabled PPIs (SGIs are always enabled)
    uint8_t priority[32];       // GICD_IPRIORITYRn
    uint32_t config_ppi;        // GICD_ICFGR1
} hv_vgicd_t;

// Emulated PL011 of a VM (unless passed through).
typedef struct {
    uint16_t host_port;         // Host COM port, or HV_PORT_NONE
    bool passthrough;           // Mapped to a host PL011 (no exits)
    uint32_t cr;                // UARTCR
    uint32_t lcr_h;             // UARTLCR_H
    uint32_t ibrd;              // UARTIBRD
    uint32_t fbrd;              // UARTFBRD
    uint32_t ifls;              // UARTIFLS
    uint32_t imsc;              // UARTIMSC
} hv_vuart_t;

// Emulated device pages: the GIC distributor, the two PL011s, the RTC.
#define HV_MMIO_GICD        0U
#define HV_MMIO_UART0       1U
#define HV_MMIO_UART1       2U
#define HV_MMIO_RTC         3U
#define HV_MMIO_DEVICES     4U

// One bit per 32-bit register of a 4 KiB page.
#define HV_MMIO_LOG_SIZE    (0x1000U / 4U / 8U)

typedef struct {
    uint64_t vttbr;             // VTTBR_EL2: VMID and stage-2 tables
    hv_vgicd_t gicd;
    hv_vuart_t uart[2];         // UART0 (console) and UART1 (GDB port)
    bool timer_pending;         // A timer tick the guest hasn't been given
    uint32_t timer_irqs;        // Timer interrupts given to the guest
    uint8_t mmio_logged[HV_MMIO_DEVICES][HV_MMIO_LOG_SIZE];
} hv_arch_vm_t;

typedef struct {
    hv_guest_regs_t regs;       // First: TPIDR_EL2 points to it
} hv_arch_vcpu_t;

typedef struct {
    uint32_t irqs_logged;       // Unexpected SGIs and PPIs reported
    bool fiq_logged;            // An unexpected FIQ reported
} hv_arch_cpu_t;

// Atomic exchange of a 32-bit word; returns the old value (spin locks).
static inline uint32_t hv_arch_xchg32(volatile uint32_t *word,
                                      uint32_t value)
{
    return arm64_xchg32(word, value);
}

// Spin-wait hint.
static inline void hv_arch_relax(void)
{
    arm64_yield();
}

// Memory barrier: the accesses before it are seen before those after it.
static inline void hv_arch_barrier(void)
{
    arm64_dmb_ish();
}

// Mask the interrupts (and the other exceptions) on this CPU.
static inline void hv_arch_irq_disable(void)
{
    arm64_mask_all();
}

#endif // HV_ARCH_ARCH_H
