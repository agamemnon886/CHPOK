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
 * macros are documented; the types shared with x86_64 point to their
 * documentation, and their member comments start with arm64:.
 */

#ifndef HV_ARCH_ARCH_H
#define HV_ARCH_ARCH_H

#include <stdint.h>
#include <stdbool.h>

#include <arch/arm64.h>

/* Documented with its x86_64 definition. */
typedef struct {
    uint64_t x[31]; /**< arm64: X0-X30. */
    uint64_t elr; /**< arm64: ELR_EL2: where the guest goes on. */
    uint64_t spsr; /**< arm64: SPSR_EL2: its PSTATE. */
    uint64_t host_sp; /**< arm64: SP_EL2 in hv_guest_enter(). */
} hv_guest_regs_t;

/**
 * @typename `hv_vgicd_t`
 *
 * @description
 * Emulated GICv2 distributor of a VM: interrupts 0-31 (one CPU).
 */
typedef struct {
    bool enabled; /**< GICD_CTLR.EnableGrp0. */
    uint32_t enable; /**< Enabled PPIs (SGIs are always enabled). */
    uint8_t priority[32]; /**< GICD_IPRIORITYRn. */
    uint32_t config_ppi; /**< GICD_ICFGR1. */
} hv_vgicd_t;

/**
 * @typename `hv_vuart_t`
 *
 * @description
 * Emulated PL011 of a VM (unless passed through).
 */
typedef struct {
    uint16_t host_port; /**< Host COM port, or HV_PORT_NONE. */
    bool passthrough; /**< Mapped to a host PL011 (no exits). */
    uint32_t cr; /**< UARTCR. */
    uint32_t lcr_h; /**< UARTLCR_H. */
    uint32_t ibrd; /**< UARTIBRD. */
    uint32_t fbrd; /**< UARTFBRD. */
    uint32_t ifls; /**< UARTIFLS. */
    uint32_t imsc; /**< UARTIMSC. */
} hv_vuart_t;

/**
 * @macroname `HV_MMIO_GICD`
 *
 * @description
 * The index of the page of the GIC distributor of the guest (the emulated
 * device pages are the GIC distributor, the two PL011s and the RTC).
 */
#define HV_MMIO_GICD        0U

/**
 * @macroname `HV_MMIO_UART0`
 *
 * @description
 * The index of the page of the first PL011 of the guest.
 */
#define HV_MMIO_UART0       1U

/**
 * @macroname `HV_MMIO_UART1`
 *
 * @description
 * The index of the page of the second PL011 of the guest.
 */
#define HV_MMIO_UART1       2U

/**
 * @macroname `HV_MMIO_RTC`
 *
 * @description
 * The index of the page of the PL031 of the guest.
 */
#define HV_MMIO_RTC         3U

/**
 * @macroname `HV_MMIO_DEVICES`
 *
 * @description
 * The number of the emulated device pages.
 */
#define HV_MMIO_DEVICES     4U

/**
 * @macroname `HV_MMIO_LOG_SIZE`
 *
 * @description
 * The number of bytes of the log of a device page: one bit per 32-bit register
 * of a 4 KiB page.
 */
#define HV_MMIO_LOG_SIZE    (0x1000U / 4U / 8U)

/* Documented with its x86_64 definition. */
typedef struct {
    uint64_t vttbr; /**< arm64: VTTBR_EL2: VMID and stage-2 tables. */
    hv_vgicd_t gicd; /**< arm64: The emulated GIC distributor. */
    hv_vuart_t uart[2]; /**< arm64: UART0 (console) and UART1 (GDB port). */
    bool timer_pending; /**< arm64: A timer tick the guest hasn't been given. */
    uint32_t timer_irqs; /**< arm64: Timer interrupts given to the guest. */
    /**
     * arm64: The accesses to unhandled registers that were reported, one bit
     * per register of each device page.
     */
    uint8_t mmio_logged[HV_MMIO_DEVICES][HV_MMIO_LOG_SIZE];
} hv_arch_vm_t;

/* Documented with its x86_64 definition. */
typedef struct {
    hv_guest_regs_t regs; /**< arm64: First: TPIDR_EL2 points to it. */
} hv_arch_vcpu_t;

/* Documented with its x86_64 definition. */
typedef struct {
    uint32_t irqs_logged; /**< arm64: Unexpected SGIs and PPIs reported. */
    bool fiq_logged; /**< arm64: An unexpected FIQ reported. */
} hv_arch_cpu_t;

/*
 * Atomically exchanges a 32-bit word (for the spin locks of the common code).
 */
static inline uint32_t hv_arch_xchg32(volatile uint32_t *word,
                                      uint32_t value)
{
    return arm64_xchg32(word, value);
}

/*
 * The spin-wait hint of the common code's spin locks.
 */
static inline void hv_arch_relax(void)
{
    arm64_yield();
}

/*
 * A memory barrier for the common code: the accesses before it are seen by the
 * other CPUs before those after it.
 */
static inline void hv_arch_barrier(void)
{
    arm64_dmb_ish();
}

/*
 * Masks the interrupts and the other exceptions on this CPU (before a fatal
 * error is reported).
 */
static inline void hv_arch_irq_disable(void)
{
    arm64_mask_all();
}

#endif // HV_ARCH_ARCH_H
