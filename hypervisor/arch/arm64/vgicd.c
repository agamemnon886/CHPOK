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
 * The GICv2 distributor of a VM (guest-physical 0x08000000), emulated:
 * each access exits (mmio.c). The VM has one CPU and interrupts 0-31
 * (TYPER: ITLinesNumber 0, CPUNumber 0); of these only the virtual timer
 * (27) ever comes (gic.c).
 *
 *   CTLR                       read/write (EnableGrp0)
 *   TYPER, IIDR                read only (IIDR: the host's)
 *   ISENABLER0, ICENABLER0     the PPIs' enables; SGIs are always enabled
 *   IPRIORITYR0-7              byte or word accesses (bits 7:3 kept)
 *   ITARGETSR0-7               read only: CPU 0
 *   ICFGR0                     read only: SGIs edge-triggered
 *   ICFGR1                     stored
 *   IGROUPR0, ISPENDR0, ICPENDR0, ISACTIVER0, ICACTIVER0, SGIR,
 *   CPENDSGIRn, SPENDSGIRn, the ID registers
 *                              read as 0, writes ignored
 *
 * Any other register reads as 0 and ignores writes, and is reported.
 */

#include <arch/internal.h>

#define GICD_CTLR           0x000U
#define GICD_TYPER          0x004U
#define GICD_IIDR           0x008U
#define GICD_IGROUPR0       0x080U
#define GICD_ISENABLER0     0x100U
#define GICD_ICENABLER0     0x180U
#define GICD_ISPENDR0       0x200U
#define GICD_ICPENDR0       0x280U
#define GICD_ISACTIVER0     0x300U
#define GICD_ICACTIVER0     0x380U
#define GICD_IPRIORITYR0    0x400U
#define GICD_IPRIORITYR_END 0x420U
#define GICD_ITARGETSR0     0x800U
#define GICD_ITARGETSR_END  0x820U
#define GICD_ICFGR0         0xC00U
#define GICD_ICFGR1         0xC04U
#define GICD_SGIR           0xF00U
#define GICD_SGI_PEND_FIRST 0xF10U      // CPENDSGIRn, SPENDSGIRn
#define GICD_SGI_PEND_END   0xF30U
#define GICD_ID_FIRST       0xFD0U
#define GICD_ID_END         0x1000U

#define SGIS                0x0000FFFFU // Always enabled
#define PRIORITY_BITS       0xF8U
#define ICFGR0_SGIS         0xAAAAAAAAU
#define TARGETS_CPU0        0x01010101U

// The distributor as at power-on: off, interrupts disabled, priority 0.
void vgicd_reset(hv_vgicd_t *gicd)
{
    hv_memset(gicd, 0U, sizeof(*gicd));
}

// Whether the guest lets the timer interrupt through.
bool vgicd_timer_enabled(const hv_vgicd_t *gicd)
{
    return gicd->enabled
           && ((gicd->enable & ((uint32_t) 1U << HV_IRQ_VTIMER)) != 0U);
}

// The guest's priority of the timer interrupt.
uint8_t vgicd_timer_priority(const hv_vgicd_t *gicd)
{
    return gicd->priority[HV_IRQ_VTIMER];
}

// Whether offset is in [first, end).
static bool in(uint32_t offset, uint32_t first, uint32_t end)
{
    return (offset >= first) && (offset < end);
}

// A word of the priority registers (little-endian bytes).
static uint32_t priority_word(const hv_vgicd_t *gicd, uint32_t first)
{
    uint32_t value = 0U;

    for (uint32_t i = 0U; i < 4U; i++) {
        value |= (uint32_t) gicd->priority[first + i] << (8U * i);
    }
    return value;
}

// Read a 32-bit register; returns false if it isn't emulated.
static bool read_word(const hv_vgicd_t *gicd, uint32_t offset,
                      uint32_t *value)
{
    bool known = true;

    if (offset == GICD_CTLR) {
        *value = gicd->enabled ? 1U : 0U;
    } else if (offset == GICD_TYPER) {
        *value = 0U;
    } else if (offset == GICD_IIDR) {
        *value = gic_host_iidr();
    } else if ((offset == GICD_ISENABLER0) || (offset == GICD_ICENABLER0)) {
        *value = gicd->enable | SGIS;
    } else if (in(offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)) {
        *value = priority_word(gicd, offset - GICD_IPRIORITYR0);
    } else if (in(offset, GICD_ITARGETSR0, GICD_ITARGETSR_END)) {
        *value = TARGETS_CPU0;
    } else if (offset == GICD_ICFGR0) {
        *value = ICFGR0_SGIS;
    } else if (offset == GICD_ICFGR1) {
        *value = gicd->config_ppi;
    } else {
        *value = 0U;
        known = (offset == GICD_IGROUPR0) || (offset == GICD_ISPENDR0)
                || (offset == GICD_ICPENDR0) || (offset == GICD_ISACTIVER0)
                || (offset == GICD_ICACTIVER0)
                || in(offset, GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END)
                || in(offset, GICD_ID_FIRST, GICD_ID_END);
    }
    return known;
}

// Write a 32-bit register; returns false if it isn't emulated.
static bool write_word(hv_vgicd_t *gicd, uint32_t offset, uint32_t value)
{
    bool known = true;

    if (offset == GICD_CTLR) {
        gicd->enabled = (value & 1U) != 0U;
    } else if (offset == GICD_ISENABLER0) {
        gicd->enable |= value & ~SGIS;
    } else if (offset == GICD_ICENABLER0) {
        gicd->enable &= ~(value & ~SGIS);
    } else if (in(offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)) {
        for (uint32_t i = 0U; i < 4U; i++) {
            gicd->priority[(offset - GICD_IPRIORITYR0) + i] =
                (uint8_t) ((value >> (8U * i)) & PRIORITY_BITS);
        }
    } else if (offset == GICD_ICFGR1) {
        gicd->config_ppi = value;
    } else {
        // Read-only, or read as 0 and writes ignored.
        known = (offset == GICD_TYPER) || (offset == GICD_IIDR)
                || in(offset, GICD_ITARGETSR0, GICD_ITARGETSR_END)
                || (offset == GICD_ICFGR0) || (offset == GICD_IGROUPR0)
                || (offset == GICD_ISPENDR0) || (offset == GICD_ICPENDR0)
                || (offset == GICD_ISACTIVER0)
                || (offset == GICD_ICACTIVER0) || (offset == GICD_SGIR)
                || in(offset, GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END);
    }
    return known;
}

/*
 * An access of the guest to its distributor: size 4 bytes at offset, or 1
 * byte in the byte-accessible registers (IPRIORITYR, ITARGETSR,
 * CPENDSGIR, SPENDSGIR). value is read or written. Returns false if the
 * access isn't emulated (it reads as 0). The timer interrupt is given or
 * taken back if the access changed whether it may come.
 */
bool vgicd_access(hv_vm_t *vm, uint32_t offset, uint32_t size, bool write,
                  uint64_t *value)
{
    hv_vgicd_t *gicd = &vm->arch.gicd;
    uint32_t word_offset = offset & ~3U;
    uint32_t shift = 8U * (offset & 3U);
    uint32_t word = 0U;
    bool bytes = in(word_offset, GICD_IPRIORITYR0, GICD_IPRIORITYR_END)
                 || in(word_offset, GICD_ITARGETSR0, GICD_ITARGETSR_END)
                 || in(word_offset, GICD_SGI_PEND_FIRST, GICD_SGI_PEND_END);
    bool known;

    if (!((size == 4U) && (shift == 0U)) && !((size == 1U) && bytes)) {
        *value = 0U;
        return false;
    }
    known = read_word(gicd, word_offset, &word);
    if (!write) {
        uint32_t read = (size == 4U) ? word : ((word >> shift) & 0xFFU);

        *value = read;
    } else if (size == 4U) {
        known = write_word(gicd, word_offset, (uint32_t) *value);
    } else {
        // A byte: the other bytes of the word are written as they are.
        uint32_t mask = 0xFFU << shift;

        word = (word & ~mask) | (((uint32_t) *value << shift) & mask);
        known = write_word(gicd, word_offset, word);
    }
    if (write) {
        gic_update(vm);
    }
    return known;
}
