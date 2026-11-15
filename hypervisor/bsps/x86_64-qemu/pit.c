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
 * arch/x86_64/. Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and external
 * declarations are in arch/internal.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/x86_64/pit.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps).
 */

/**
 * @file
 * Physical legacy timers: the 8259A PIC is masked (the hypervisor uses the
 * local APIC timer), and the TSC and local APIC timer frequencies are
 * measured once against PIT channel 2.
 */

#include <arch/internal.h>

static uint64_t tsc_hz;
static uint64_t lapic_hz;

/**
 * @funcname `hv_legacy_pic_disable`
 *
 * @description
 * Programs the legacy 8259A PIC pair away from the exception vectors (to
 * 0x20-0x2F) and masks all its interrupts: the hypervisor uses the local
 * APIC.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `x86_outb()` with the arguments `PIC_MASTER` and `0x11U`.
 * 2. Call `x86_outb()` with the arguments `PIC_SLAVE` and `0x11U`.
 * 3. Call `x86_outb()` with the arguments `PIC_MASTER + 1U` and `0x20U`.
 * 4. Call `x86_outb()` with the arguments `PIC_SLAVE + 1U` and `0x28U`.
 * 5. Call `x86_outb()` with the arguments `PIC_MASTER + 1U` and `0x04U`.
 * 6. Call `x86_outb()` with the arguments `PIC_SLAVE + 1U` and `0x02U`.
 * 7. Call `x86_outb()` with the arguments `PIC_MASTER + 1U` and `0x01U`.
 * 8. Call `x86_outb()` with the arguments `PIC_SLAVE + 1U` and `0x01U`.
 * 9. Call `x86_outb()` with the arguments `PIC_MASTER + 1U` and `0xFFU`.
 * 10. Call `x86_outb()` with the arguments `PIC_SLAVE + 1U` and `0xFFU`.
 *
 * @calls
 * - `x86_outb()`
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

/**
 * @funcname `hv_timing_init`
 *
 * @description
 * Measures the TSC and local APIC timer frequencies against 50 ms of PIT
 * channel 2 (gated by port B, mode 0). Stops the hypervisor if they can't be
 * measured.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tsc_hz` | `uint64_t` | The TSC frequency, Hz |
 * | `lapic_hz` | `uint64_t` | The local APIC timer frequency, Hz |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port_b` | `uint8_t` | Port B before the measure |
 * | `tsc_start` | `uint64_t` | The TSC when channel 2 starts |
 * | `tsc_end` | `uint64_t` | The TSC when channel 2 ends |
 * | `lapic_end` | `uint32_t` | The local APIC timer count when channel 2 ends |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `x86_inb()` with the argument `PORT_B`, and store the result in
 *    `port_b`.
 * 2. Call `x86_outb()` with the arguments `PORT_B` and
 *    `(uint8_t) ((port_b & ~PORT_B_SPEAKER) | PORT_B_GATE2)`.
 * 3. Call `x86_outb()` with the arguments `PIT_COMMAND` and `0xB0U`.
 * 4. Call `x86_outb()` with the arguments `PIT_CHANNEL2` and
 *    `(uint8_t) (CALIBRATION_TICKS & 0xFFU)`.
 * 5. Call `lapic_timer_oneshot()` with the argument `LAPIC_TIMER_START`.
 * 6. Call `x86_rdtsc()`, and store the result in `tsc_start`.
 * 7. Call `x86_outb()` with the arguments `PIT_CHANNEL2` and
 *    `(uint8_t) (CALIBRATION_TICKS >> 8)`.
 * 8. While `(x86_inb(PORT_B) & PORT_B_OUT2)` is equal to `0U`:
 *    1. Call `x86_pause()`.
 * 9. Call `x86_rdtsc()`, and store the result in `tsc_end`.
 * 10. Call `lapic_timer_current()`, and store the result in `lapic_end`.
 * 11. Call `x86_outb()` with the arguments `PORT_B` and `port_b`.
 * 12. Call `lapic_timer_stop()`.
 * 13. Set `tsc_hz` to
 *     `((tsc_end - tsc_start) * HV_PIT_HZ) / CALIBRATION_TICKS`.
 * 14. Set `lapic_hz` to
 *     `(((uint64_t) LAPIC_TIMER_START - (uint64_t) lapic_end) * HV_PIT_HZ) /`
 *     `CALIBRATION_TICKS`.
 * 15. If `tsc_hz` is equal to `0U` or `lapic_hz` is equal to `0U`:
 *     1. Call `hv_fatal()` with the argument
 *        `"TSC or local APIC timer calibration failed"`.
 *
 * @calls
 * - `x86_inb()`
 * - `x86_outb()`
 * - `lapic_timer_oneshot()`
 * - `x86_rdtsc()`
 * - `x86_pause()`
 * - `lapic_timer_current()`
 * - `lapic_timer_stop()`
 * - `hv_fatal()`
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

/**
 * @funcname `hv_tsc_hz`
 *
 * @description
 * Returns the TSC frequency measured at boot.
 *
 * @return The TSC frequency, Hz.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tsc_hz` | `uint64_t` | The TSC frequency, Hz |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `tsc_hz`.
 *
 * @calls
 * None.
 */
uint64_t hv_tsc_hz(void)
{
    return tsc_hz;
}

/**
 * @funcname `hv_lapic_hz`
 *
 * @description
 * Returns the local APIC timer frequency (after the divider) measured at
 * boot.
 *
 * @return The local APIC timer frequency, Hz.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lapic_hz` | `uint64_t` | The local APIC timer frequency, Hz |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `lapic_hz`.
 *
 * @calls
 * None.
 */
uint64_t hv_lapic_hz(void)
{
    return lapic_hz;
}

/**
 * @funcname `hv_udelay`
 *
 * @description
 * Busy-waits for a number of microseconds, by the TSC.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `microseconds` | `uint64_t` | [in] The time to wait. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tsc_hz` | `uint64_t` | The TSC frequency, Hz |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `end` | `uint64_t` | The TSC value at which the wait ends |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `end` to `x86_rdtsc() + ((microseconds * tsc_hz) / 1000000U)`.
 * 2. While `x86_rdtsc()` is less than `end`:
 *    1. Call `x86_pause()`.
 *
 * @calls
 * - `x86_rdtsc()`
 * - `x86_pause()`
 */
void hv_udelay(uint64_t microseconds)
{
    uint64_t end = x86_rdtsc() + ((microseconds * tsc_hz) / 1000000U);

    // The TSC doesn't wrap around in practice (64 bits).

    while (x86_rdtsc() < end) {
        x86_pause();
    }
}

/**
 * @funcname `hv_tsc_to_pit_ticks`
 *
 * @description
 * Converts a number of TSC ticks to PIT ticks (1.193182 MHz), without
 * overflow.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tsc_ticks` | `uint64_t` | [in] The TSC ticks. |
 *
 * @return The PIT ticks.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tsc_hz` | `uint64_t` | The TSC frequency, Hz |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return
 *    `((tsc_ticks / tsc_hz) * HV_PIT_HZ) + (((tsc_ticks % tsc_hz) * HV_PIT_HZ)`
 *    `/ tsc_hz)`.
 *
 * @calls
 * None.
 */
uint64_t hv_tsc_to_pit_ticks(uint64_t tsc_ticks)
{
    // Whole seconds and the rest separately: the product can't overflow.
    return ((tsc_ticks / tsc_hz) * HV_PIT_HZ)
           + (((tsc_ticks % tsc_hz) * HV_PIT_HZ) / tsc_hz);
}
