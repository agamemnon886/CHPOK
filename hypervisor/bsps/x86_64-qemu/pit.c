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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: the low level requirements
 * name the register and range helpers that the functions call.
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
 * - \hlrid{HLR_PIT_HV_LEGACY_PIC_DISABLE_01}: `hv_legacy_pic_disable()` shall
 *   move the legacy 8259A pair of the host away from the exception vectors and
 *   mask all its interrupts.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_INT_005}
 *
 * @llr
 * - \llrid{LLR_PIT_HV_LEGACY_PIC_DISABLE_01}: `hv_legacy_pic_disable()` shall
 *   write the initialization words with `x86_outb()`, in this order: `0x11` to
 *   `PIC_MASTER` and to `PIC_SLAVE`, `0x20` to `PIC_MASTER` plus 1, `0x28` to
 *   `PIC_SLAVE` plus 1, `0x04` to `PIC_MASTER` plus 1, `0x02` to `PIC_SLAVE`
 *   plus 1, and `0x01` to `PIC_MASTER` plus 1 and to `PIC_SLAVE` plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 * - \llrid{LLR_PIT_HV_LEGACY_PIC_DISABLE_02}: After the initialization words,
 *   `hv_legacy_pic_disable()` shall write `0xFF` with `x86_outb()` to
 *   `PIC_MASTER` plus 1 and to `PIC_SLAVE` plus 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 *
 * @design
 * 1. Call `x86_outb()` with the arguments `PIC_MASTER` and `0x11U`.
 *    \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 * 2. Call `x86_outb()` with the arguments `PIC_SLAVE` and `0x11U`.
 *    \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 * 3. Call `x86_outb()` with the arguments `PIC_MASTER + 1U` and `0x20U`.
 *    \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 * 4. Call `x86_outb()` with the arguments `PIC_SLAVE + 1U` and `0x28U`.
 *    \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 * 5. Call `x86_outb()` with the arguments `PIC_MASTER + 1U` and `0x04U`.
 *    \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 * 6. Call `x86_outb()` with the arguments `PIC_SLAVE + 1U` and `0x02U`.
 *    \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 * 7. Call `x86_outb()` with the arguments `PIC_MASTER + 1U` and `0x01U`.
 *    \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 * 8. Call `x86_outb()` with the arguments `PIC_SLAVE + 1U` and `0x01U`.
 *    \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_01}
 * 9. Call `x86_outb()` with the arguments `PIC_MASTER + 1U` and `0xFFU`.
 *    \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_02}
 * 10. Call `x86_outb()` with the arguments `PIC_SLAVE + 1U` and `0xFFU`.
 *     \llrref{LLR_PIT_HV_LEGACY_PIC_DISABLE_02}
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
 * - \hlrid{HLR_PIT_HV_TIMING_INIT_01}: `hv_timing_init()` shall measure the
 *   frequencies of the time stamp counter and of the local APIC timer against
 *   50 ms of channel 2 of the 8254 timer, and stop with a message if it cannot.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_005}
 *
 * @llr
 * - \llrid{LLR_PIT_HV_TIMING_INIT_01}: `hv_timing_init()` shall read `PORT_B`
 *   with `x86_inb()` and write the value back with `x86_outb()`, with
 *   `PORT_B_SPEAKER` cleared and `PORT_B_GATE2` set, then write `0xB0` to
 *   `PIT_COMMAND` and the low byte of `CALIBRATION_TICKS` to `PIT_CHANNEL2`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_TIMING_INIT_01}
 * - \llrid{LLR_PIT_HV_TIMING_INIT_02}: `hv_timing_init()` shall then call
 *   `lapic_timer_oneshot()` with `LAPIC_TIMER_START`, read the TSC with
 *   `x86_rdtsc()` and write the high byte of `CALIBRATION_TICKS` to
 *   `PIT_CHANNEL2`, in this order.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_TIMING_INIT_01}
 * - \llrid{LLR_PIT_HV_TIMING_INIT_03}: `hv_timing_init()` shall call
 *   `x86_pause()` while `PORT_B_OUT2` is 0 in the value that `x86_inb()` reads
 *   from `PORT_B`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_TIMING_INIT_01}
 * - \llrid{LLR_PIT_HV_TIMING_INIT_04}: When `PORT_B_OUT2` is set,
 *   `hv_timing_init()` shall read the TSC with `x86_rdtsc()` and the count of
 *   the APIC timer with `lapic_timer_current()`, write the first value read
 *   from `PORT_B` back to it and call `lapic_timer_stop()`, in this order.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_TIMING_INIT_01}
 * - \llrid{LLR_PIT_HV_TIMING_INIT_05}: `hv_timing_init()` shall set `tsc_hz` to
 *   the difference of the two TSC values times `HV_PIT_HZ` divided by
 *   `CALIBRATION_TICKS`, and `lapic_hz` to `LAPIC_TIMER_START` minus the count
 *   of the APIC timer, times `HV_PIT_HZ` divided by `CALIBRATION_TICKS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_TIMING_INIT_01}
 * - \llrid{LLR_PIT_HV_TIMING_INIT_06}: If and only if `tsc_hz` is 0 or
 *   `lapic_hz` is 0, `hv_timing_init()` shall call `hv_fatal()` with `"TSC or
 *   local APIC timer calibration failed"`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_TIMING_INIT_01}
 *
 * @design
 * 1. Call `x86_inb()` with the argument `PORT_B`, and store the result in
 *    `port_b`. \llrref{LLR_PIT_HV_TIMING_INIT_01}
 * 2. Call `x86_outb()` with the arguments `PORT_B` and
 *    `(uint8_t) ((port_b & ~PORT_B_SPEAKER) | PORT_B_GATE2)`.
 *    \llrref{LLR_PIT_HV_TIMING_INIT_01}
 * 3. Call `x86_outb()` with the arguments `PIT_COMMAND` and `0xB0U`.
 *    \llrref{LLR_PIT_HV_TIMING_INIT_01}
 * 4. Call `x86_outb()` with the arguments `PIT_CHANNEL2` and
 *    `(uint8_t) (CALIBRATION_TICKS & 0xFFU)`.
 *    \llrref{LLR_PIT_HV_TIMING_INIT_01}
 * 5. Call `lapic_timer_oneshot()` with the argument `LAPIC_TIMER_START`.
 *    \llrref{LLR_PIT_HV_TIMING_INIT_02}
 * 6. Call `x86_rdtsc()`, and store the result in `tsc_start`.
 *    \llrref{LLR_PIT_HV_TIMING_INIT_02}
 * 7. Call `x86_outb()` with the arguments `PIT_CHANNEL2` and
 *    `(uint8_t) (CALIBRATION_TICKS >> 8)`. \llrref{LLR_PIT_HV_TIMING_INIT_02}
 * 8. While `(x86_inb(PORT_B) & PORT_B_OUT2)` is equal to `0U`:
 *    \llrref{LLR_PIT_HV_TIMING_INIT_03}
 *    1. Call `x86_pause()`.
 * 9. Call `x86_rdtsc()`, and store the result in `tsc_end`.
 *    \llrref{LLR_PIT_HV_TIMING_INIT_04}
 * 10. Call `lapic_timer_current()`, and store the result in `lapic_end`.
 *     \llrref{LLR_PIT_HV_TIMING_INIT_04}
 * 11. Call `x86_outb()` with the arguments `PORT_B` and `port_b`.
 *     \llrref{LLR_PIT_HV_TIMING_INIT_04}
 * 12. Call `lapic_timer_stop()`. \llrref{LLR_PIT_HV_TIMING_INIT_04}
 * 13. Set `tsc_hz` to
 *     `((tsc_end - tsc_start) * HV_PIT_HZ) / CALIBRATION_TICKS`.
 *     \llrref{LLR_PIT_HV_TIMING_INIT_05}
 * 14. Set `lapic_hz` to
 *     `(((uint64_t) LAPIC_TIMER_START - (uint64_t) lapic_end) * HV_PIT_HZ) /`
 *     `CALIBRATION_TICKS`. \llrref{LLR_PIT_HV_TIMING_INIT_05}
 * 15. If `tsc_hz` is equal to `0U` or `lapic_hz` is equal to `0U`:
 *     \llrref{LLR_PIT_HV_TIMING_INIT_06}
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
 * - \hlrid{HLR_PIT_HV_TSC_HZ_01}: `hv_tsc_hz()` shall give the frequency of the
 *   time stamp counter.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_005}
 *
 * @llr
 * - \llrid{LLR_PIT_HV_TSC_HZ_01}: `hv_tsc_hz()` shall return `tsc_hz`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_TSC_HZ_01}
 *
 * @design
 * 1. Return `tsc_hz`. \llrref{LLR_PIT_HV_TSC_HZ_01}
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
 * - \hlrid{HLR_PIT_HV_LAPIC_HZ_01}: `hv_lapic_hz()` shall give the frequency of
 *   the local APIC timer.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_005}
 *
 * @llr
 * - \llrid{LLR_PIT_HV_LAPIC_HZ_01}: `hv_lapic_hz()` shall return `lapic_hz`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_LAPIC_HZ_01}
 *
 * @design
 * 1. Return `lapic_hz`. \llrref{LLR_PIT_HV_LAPIC_HZ_01}
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
 * - \hlrid{HLR_PIT_HV_UDELAY_01}: `hv_udelay()` shall wait for a number of
 *   microseconds, for the start of the other CPUs.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CPU_002}
 *
 * @llr
 * - \llrid{LLR_PIT_HV_UDELAY_01}: `hv_udelay()` shall call `x86_pause()` while
 *   the result of `x86_rdtsc()` is less than its first result plus
 *   `microseconds` times `tsc_hz` divided by 1000000.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_UDELAY_01}
 *
 * @design
 * 1. Set `end` to `x86_rdtsc() + ((microseconds * tsc_hz) / 1000000U)`.
 *    \llrref{LLR_PIT_HV_UDELAY_01}
 * 2. While `x86_rdtsc()` is less than `end`: \llrref{LLR_PIT_HV_UDELAY_01}
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
 * - \hlrid{HLR_PIT_HV_TSC_TO_PIT_TICKS_01}: `hv_tsc_to_pit_ticks()` shall
 *   convert ticks of the time stamp counter into ticks of the 8254 timer,
 *   without overflow.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_PIT_HV_TSC_TO_PIT_TICKS_01}: `hv_tsc_to_pit_ticks()` shall
 *   return the whole quotient of `tsc_ticks` by `tsc_hz` times `HV_PIT_HZ`,
 *   plus the remainder of that division times `HV_PIT_HZ` divided by `tsc_hz`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_HV_TSC_TO_PIT_TICKS_01}
 *
 * @design
 * 1. Return
 *    `((tsc_ticks / tsc_hz) * HV_PIT_HZ) + (((tsc_ticks % tsc_hz) * HV_PIT_HZ)`
 *    `/ tsc_hz)`. \llrref{LLR_PIT_HV_TSC_TO_PIT_TICKS_01}
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
