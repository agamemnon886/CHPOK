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
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from hypervisor/src/ to
 * arch/x86_64/.
 */

/*
 * Physical legacy timers: the 8259A PIC is masked (the hypervisor uses
 * the local APIC timer), and the TSC and local APIC timer frequencies are
 * measured once against PIT channel 2.
 */

#include <arch/internal.h>

#define PIC_MASTER          0x20U
#define PIC_SLAVE           0xA0U

#define PIT_CHANNEL2        0x42U
#define PIT_COMMAND         0x43U
#define PORT_B              0x61U
#define PORT_B_GATE2        0x01U
#define PORT_B_SPEAKER      0x02U
#define PORT_B_OUT2         0x20U

// Calibration time in PIT ticks (at most 0xFFFF): 50 ms.
#define CALIBRATION_TICKS   (HV_PIT_HZ / 20U)

#define LAPIC_TIMER_START   0xFFFFFFFFU

static uint64_t tsc_hz;
static uint64_t lapic_hz;

/*
 * Program the legacy 8259A PIC pair away from the exception vectors (to
 * 0x20-0x2F) and mask all its interrupts: the hypervisor uses the local APIC.
 */
void hv_legacy_pic_disable(void)
{
    // ICW1-ICW4: vectors 0x20 and 0x28, cascade on IRQ 2, 8086 mode.
    x86_outb(PIC_MASTER, 0x11U);
    x86_outb(PIC_SLAVE, 0x11U);
    x86_outb(PIC_MASTER + 1U, 0x20U);
    x86_outb(PIC_SLAVE + 1U, 0x28U);
    x86_outb(PIC_MASTER + 1U, 0x04U);
    x86_outb(PIC_SLAVE + 1U, 0x02U);
    x86_outb(PIC_MASTER + 1U, 0x01U);
    x86_outb(PIC_SLAVE + 1U, 0x01U);
    // All interrupts masked.
    x86_outb(PIC_MASTER + 1U, 0xFFU);
    x86_outb(PIC_SLAVE + 1U, 0xFFU);
}

/*
 * Measure the TSC and local APIC timer frequencies against 50 ms of PIT channel
 * 2 (gated by port B, mode 0). Stops the hypervisor if they can't be measured.
 */
void hv_timing_init(void)
{
    uint8_t port_b = x86_inb(PORT_B);
    uint64_t tsc_start;
    uint64_t tsc_end;
    uint32_t lapic_end;

    // Gate channel 2 on, speaker off; mode 0 (interrupt on terminal).
    x86_outb(PORT_B, (uint8_t) ((port_b & ~PORT_B_SPEAKER) | PORT_B_GATE2));
    x86_outb(PIT_COMMAND, 0xB0U);
    x86_outb(PIT_CHANNEL2, (uint8_t) (CALIBRATION_TICKS & 0xFFU));

    // Writing the MSB starts the count; OUT2 goes high when it ends.
    lapic_timer_oneshot(LAPIC_TIMER_START);
    tsc_start = x86_rdtsc();
    x86_outb(PIT_CHANNEL2, (uint8_t) (CALIBRATION_TICKS >> 8));
    while ((x86_inb(PORT_B) & PORT_B_OUT2) == 0U) {
        x86_pause();
    }
    tsc_end = x86_rdtsc();
    lapic_end = lapic_timer_current();
    x86_outb(PORT_B, port_b);
    lapic_timer_stop();

    // Ticks counted in CALIBRATION_TICKS PIT ticks, scaled to one second.
    tsc_hz = ((tsc_end - tsc_start) * HV_PIT_HZ) / CALIBRATION_TICKS;
    lapic_hz = (((uint64_t) LAPIC_TIMER_START - (uint64_t) lapic_end)
                * HV_PIT_HZ) / CALIBRATION_TICKS;
    if ((tsc_hz == 0U) || (lapic_hz == 0U)) {
        hv_fatal("TSC or local APIC timer calibration failed");
    }
}

// TSC frequency, Hz.
uint64_t hv_tsc_hz(void)
{
    return tsc_hz;
}

// Local APIC timer frequency (after the divider), Hz.
uint64_t hv_lapic_hz(void)
{
    return lapic_hz;
}

// Busy-wait for the given number of microseconds (by the TSC).
void hv_udelay(uint64_t microseconds)
{
    uint64_t end = x86_rdtsc() + ((microseconds * tsc_hz) / 1000000U);

    // The TSC doesn't wrap around in practice (64 bits).

    while (x86_rdtsc() < end) {
        x86_pause();
    }
}

// Convert a number of TSC ticks to PIT ticks, without overflow.
uint64_t hv_tsc_to_pit_ticks(uint64_t tsc_ticks)
{
    // Whole seconds and the rest separately: the product can't overflow.
    return ((tsc_ticks / tsc_hz) * HV_PIT_HZ)
           + (((tsc_ticks % tsc_hz) * HV_PIT_HZ) / tsc_hz);
}
