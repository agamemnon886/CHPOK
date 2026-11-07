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
 * Local APIC of the physical CPUs (xAPIC, memory-mapped): the timer that
 * drives the guests' virtual PIT, end of interrupt, and the INIT/SIPI
 * messages that start the other CPUs. The guests have no access to it.
 */

#include <hv/hv.h>
#include <hv/x86.h>

#define LAPIC_ID            0x020U
#define LAPIC_EOI           0x0B0U
#define LAPIC_SVR           0x0F0U
#define LAPIC_ICR_LOW       0x300U
#define LAPIC_ICR_HIGH      0x310U
#define LAPIC_LVT_TIMER     0x320U
#define LAPIC_LVT_LINT0     0x350U
#define LAPIC_LVT_LINT1     0x360U
#define LAPIC_LVT_ERROR     0x370U
#define LAPIC_TIMER_INIT    0x380U
#define LAPIC_TIMER_CURRENT 0x390U
#define LAPIC_TIMER_DIVIDE  0x3E0U

#define LAPIC_SVR_ENABLE        ((uint32_t) 1U << 8)
#define LAPIC_LVT_MASKED        ((uint32_t) 1U << 16)
#define LAPIC_LVT_NMI           ((uint32_t) 4U << 8)
#define LAPIC_TIMER_PERIODIC    ((uint32_t) 1U << 17)
#define LAPIC_TIMER_DIVIDE_16   0x3U
#define LAPIC_ICR_PENDING       ((uint32_t) 1U << 12)
#define LAPIC_ICR_INIT          0x00004500U  // INIT, level, assert
#define LAPIC_ICR_STARTUP       0x00004600U  // Start-up, level, assert

#define APIC_BASE_ADDR_MASK     0xFFFFF000UL

// Physical (= virtual) address of the local APIC registers.
static uintptr_t lapic_base = 0xFEE00000UL;

// Read a local APIC register.
/*
 * MISRA C:2025 Rule 11.4 deviation (lapic_read(), lapic_write()): the
 * registers are at a fixed physical address, identity-mapped (uncached)
 * by entry.S, which has to be converted to a pointer.
 */
static uint32_t lapic_read(uint32_t reg)
{
    const volatile uint32_t *p =
        (const volatile uint32_t *) (lapic_base + reg);

    return *p;
}

// Write a local APIC register.
static void lapic_write(uint32_t reg, uint32_t value)
{
    volatile uint32_t *p = (volatile uint32_t *) (lapic_base + reg);

    *p = value;
}

// Take the address of the local APIC registers from IA32_APIC_BASE.
void lapic_init_base(void)
{
    lapic_base = (uintptr_t) (x86_rdmsr(MSR_APIC_BASE)
                              & APIC_BASE_ADDR_MASK);
}

/*
 * Set up the local APIC of this CPU: enabled, LINT0 masked, LINT1 the NMI, the
 * timer stopped and divided by 16.
 */
void lapic_init_cpu(void)
{
    // Software-enable the local APIC; spurious interrupts are ignored.
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | HV_VECTOR_SPURIOUS);
    // The legacy PIC is masked; LINT0 is not used, LINT1 is the NMI.
    lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_NMI);
    lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_TIMER_DIVIDE, LAPIC_TIMER_DIVIDE_16);
    lapic_timer_stop();
}

// Local APIC ID of this CPU.
uint32_t lapic_id(void)
{
    return lapic_read(LAPIC_ID) >> 24;
}

// Signal the end of the current interrupt to the local APIC.
void lapic_eoi(void)
{
    lapic_write(LAPIC_EOI, 0U);
}

// Timer interrupts every 'count' ticks (of the bus clock divided by 16).
void lapic_timer_periodic(uint32_t count)
{
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_PERIODIC | HV_VECTOR_TIMER);
    lapic_write(LAPIC_TIMER_INIT, count);
}

// Count down once from 'count', without an interrupt (calibration).
void lapic_timer_oneshot(uint32_t count)
{
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED | HV_VECTOR_TIMER);
    lapic_write(LAPIC_TIMER_INIT, count);
}

// Stop the local APIC timer of this CPU.
void lapic_timer_stop(void)
{
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED | HV_VECTOR_TIMER);
    lapic_write(LAPIC_TIMER_INIT, 0U);
}

// Current count of the local APIC timer of this CPU.
uint32_t lapic_timer_current(void)
{
    return lapic_read(LAPIC_TIMER_CURRENT);
}

/*
 * Send an interprocessor interrupt (ICR command) to a CPU and wait until it is
 * delivered.
 */
static void lapic_send_ipi(uint32_t apic_id, uint32_t command)
{
    // The destination goes first: writing the low word sends the IPI.
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW, command);
    while ((lapic_read(LAPIC_ICR_LOW) & LAPIC_ICR_PENDING) != 0U) {
        x86_pause();
    }
}

// Send INIT to a CPU (by local APIC ID).
void lapic_send_init(uint32_t apic_id)
{
    lapic_send_ipi(apic_id, LAPIC_ICR_INIT);
}

// Send a start-up IPI to a CPU: it starts in real mode at vector * 4 KiB.
void lapic_send_sipi(uint32_t apic_id, uint8_t vector)
{
    lapic_send_ipi(apic_id, LAPIC_ICR_STARTUP | (uint32_t) vector);
}
