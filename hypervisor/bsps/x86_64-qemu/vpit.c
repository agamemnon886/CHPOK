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
 * hypervisor/arch/x86_64/vpit.c (the code of the board, its devices and the
 * virtual devices of the VMs is in hypervisor/bsps).
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
 */

/**
 * @file
 * Virtual 8254 PIT (ports 0x40-0x43) and port B (0x61). The counters are
 * computed from the TSC, so reading them costs no timer. Channel 0 in a
 * periodic mode (2 or 3) raises IRQ 0: the local APIC timer of the physical
 * CPU is programmed with the same period, and each of its interrupts becomes
 * one IRQ 0 (svm.c). Channel 2 is gated by port B bit 0, and its output is
 * port B bit 5 (used to calibrate the TSC). Channel 1 counts but drives
 * nothing. BCD counting and the read-back status are not emulated.
 */

#include <arch/internal.h>

/**
 * @funcname `channel_reset`
 *
 * @description
 * Resets one channel of the virtual PIT: mode 0, LSB then MSB access, no
 * count loaded, not counting.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `vpit_channel_t *` | [out] The channel. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_VPIT_CHANNEL_RESET_01}: `channel_reset()` shall reset one
 *   channel of the emulated 8254 timer: mode 0, low then high byte, no count
 *   loaded, not counting.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_CHANNEL_RESET_01}: `channel_reset()` shall set, in the
 *   channel, `mode` to 0, `access` to `PIT_ACCESS_WORD`, `write_msb`,
 *   `read_msb` and `latched` to false, `written_lsb` and `latch` to 0, `reload`
 *   to `PIT_MAX_COUNT`, `loaded` and `counting` to false, and `start_tsc` to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_CHANNEL_RESET_01}
 *
 * @design
 * 1. Set `ch->mode` to `0U`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 2. Set `ch->access` to `PIT_ACCESS_WORD`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 3. Set `ch->write_msb` to `false`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 4. Set `ch->read_msb` to `false`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 5. Set `ch->latched` to `false`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 6. Set `ch->written_lsb` to `0U`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 7. Set `ch->latch` to `0U`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 8. Set `ch->reload` to `PIT_MAX_COUNT`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 9. Set `ch->loaded` to `false`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 10. Set `ch->counting` to `false`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 * 11. Set `ch->start_tsc` to `0U`. \llrref{LLR_VPIT_CHANNEL_RESET_01}
 *
 * @calls
 * None.
 */
static void channel_reset(vpit_channel_t *ch)
{
    ch->mode = 0U;
    ch->access = PIT_ACCESS_WORD;
    ch->write_msb = false;
    ch->read_msb = false;
    ch->latched = false;
    ch->written_lsb = 0U;
    ch->latch = 0U;
    ch->reload = PIT_MAX_COUNT;
    ch->loaded = false;
    ch->counting = false;
    ch->start_tsc = 0U;
}

/**
 * @funcname `vpit_reset`
 *
 * @description
 * Resets the virtual PIT and port B of a VM (on load and reset), and stops
 * the local APIC timer behind channel 0.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit` | `vpit_t *` | [out] The virtual PIT. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of the channel |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_VPIT_RESET_01}: `vpit_reset()` shall reset the emulated
 *   8254 timer and port B of a VM when it is loaded, and stop the timer behind
 *   channel 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *   - \srqref{SRQ_HV_TMR_006}
 *   - \srqref{SRQ_HV_VM_004}
 *
 * @llr
 * - \llrid{LLR_VPIT_VPIT_RESET_01}: `vpit_reset()` shall call `channel_reset()`
 *   once for each of the `PIT_CHANNELS` channels of `pit`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_RESET_01}
 * - \llrid{LLR_VPIT_VPIT_RESET_02}: `vpit_reset()` shall set `gate2` and
 *   `speaker` of `pit` to false and call `lapic_timer_stop()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_RESET_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `PIT_CHANNELS`, repeat (after
 *    each pass: increment `i`): \llrref{LLR_VPIT_VPIT_RESET_01}
 *    1. Call `channel_reset()` with the address of `pit->channel[i]`.
 * 2. Set `pit->gate2` to `false`. \llrref{LLR_VPIT_VPIT_RESET_02}
 * 3. Set `pit->speaker` to `false`. \llrref{LLR_VPIT_VPIT_RESET_02}
 * 4. Call `lapic_timer_stop()`. \llrref{LLR_VPIT_VPIT_RESET_02}
 *
 * @calls
 * - `channel_reset()`
 * - `lapic_timer_stop()`
 */
void vpit_reset(vpit_t *pit)
{
    for (uint32_t i = 0U; i < PIT_CHANNELS; i++) {
        channel_reset(&pit->channel[i]);
    }
    pit->gate2 = false;
    pit->speaker = false;
    lapic_timer_stop();
}

/**
 * @funcname `periodic`
 *
 * @description
 * Tells whether a channel is in a periodic mode (2 or 3).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `const vpit_channel_t *` | [in] The channel. |
 *
 * @return true for modes 2 and 3.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_VPIT_PERIODIC_01}: `periodic()` shall tell whether a channel is
 *   in a periodic mode, 2 or 3.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *   - \srqref{SRQ_HV_TMR_006}
 *
 * @llr
 * - \llrid{LLR_VPIT_PERIODIC_01}: If `mode` of the channel is 2 or 3,
 *   `periodic()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_PERIODIC_01}
 * - \llrid{LLR_VPIT_PERIODIC_02}: If `mode` of the channel is neither 2 nor 3,
 *   `periodic()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_PERIODIC_01}
 *
 * @design
 * 1. Return `(ch->mode == 2U) || (ch->mode == 3U)`.
 *    \llrref{LLR_VPIT_PERIODIC_01}, \llrref{LLR_VPIT_PERIODIC_02}
 *
 * @calls
 * None.
 */
static bool periodic(const vpit_channel_t *ch)
{
    return (ch->mode == 2U) || (ch->mode == 3U);
}

/**
 * @funcname `vpit_timer_running`
 *
 * @description
 * Tells whether channel 0 raises IRQ 0 periodically (svm.c then turns the
 * local APIC timer ticks into IRQ 0).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit` | `const vpit_t *` | [in] The virtual PIT. |
 *
 * @return true if channel 0 counts in a periodic mode.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_VPIT_VPIT_TIMER_RUNNING_01}: `vpit_timer_running()` shall tell
 *   whether channel 0 raises IRQ 0 periodically.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_006}
 *
 * @llr
 * - \llrid{LLR_VPIT_VPIT_TIMER_RUNNING_01}: If `counting` of channel 0 is true
 *   and `periodic()` returns true for channel 0, `vpit_timer_running()` shall
 *   return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_TIMER_RUNNING_01}
 * - \llrid{LLR_VPIT_VPIT_TIMER_RUNNING_02}: If `counting` of channel 0 is
 *   false, or `periodic()` returns false for channel 0, `vpit_timer_running()`
 *   shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_TIMER_RUNNING_01}
 *
 * @design
 * 1. Return `pit->channel[0].counting && periodic(&pit->channel[0])`.
 *    \llrref{LLR_VPIT_VPIT_TIMER_RUNNING_01},
 *    \llrref{LLR_VPIT_VPIT_TIMER_RUNNING_02}
 *
 * @calls
 * - `periodic()`
 */
bool vpit_timer_running(const vpit_t *pit)
{
    return pit->channel[0].counting && periodic(&pit->channel[0]);
}

/**
 * @funcname `elapsed`
 *
 * @description
 * Returns the PIT ticks since a channel started counting, from the TSC.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `const vpit_channel_t *` | [in] The channel. |
 *
 * @return The PIT ticks.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_VPIT_ELAPSED_01}: `elapsed()` shall give the ticks of the 8254
 *   timer since a channel started counting.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_ELAPSED_01}: `elapsed()` shall return the result of
 *   `hv_tsc_to_pit_ticks()` for the result of `x86_rdtsc()` minus `start_tsc`
 *   of the channel.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_ELAPSED_01}
 *
 * @design
 * 1. Call `hv_tsc_to_pit_ticks()` with the argument
 *    `x86_rdtsc() - ch->start_tsc`, and return its result.
 *    \llrref{LLR_VPIT_ELAPSED_01}
 *
 * @calls
 * - `hv_tsc_to_pit_ticks()`
 * - `x86_rdtsc()`
 */
static uint64_t elapsed(const vpit_channel_t *ch)
{
    return hv_tsc_to_pit_ticks(x86_rdtsc() - ch->start_tsc);
}

/**
 * @funcname `current_count`
 *
 * @description
 * Returns the count of a channel now.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `const vpit_channel_t *` | [in] The channel. |
 *
 * @return The 16-bit count.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `uint32_t` | The count |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_CURRENT_COUNT_01}: `current_count()` shall give the present
 *   count of a channel.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_CURRENT_COUNT_01}: If `counting` of the channel is false,
 *   `current_count()` shall return the low 16 bits of `reload`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_CURRENT_COUNT_01}
 * - \llrid{LLR_VPIT_CURRENT_COUNT_02}: If `counting` is true and `periodic()`
 *   returns true for the channel, `current_count()` shall return the low 16
 *   bits of `reload` minus the remainder of the result of `elapsed()` divided
 *   by `reload`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_CURRENT_COUNT_01}
 * - \llrid{LLR_VPIT_CURRENT_COUNT_03}: If `counting` is true and `periodic()`
 *   returns false for the channel, `current_count()` shall return the low 16
 *   bits of `reload` minus the remainder of the result of `elapsed()` divided
 *   by `PIT_MAX_COUNT`, modulo `PIT_MAX_COUNT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_CURRENT_COUNT_01}
 *
 * @design
 * 1. If `ch->counting` is false: \llrref{LLR_VPIT_CURRENT_COUNT_01}
 *    1. Set `count` to `ch->reload`.
 * 2. Otherwise, if `periodic(ch)` is true: \llrref{LLR_VPIT_CURRENT_COUNT_02}
 *    1. Set `count` to `ch->reload - (uint32_t) (elapsed(ch) % ch->reload)`.
 * 3. Otherwise: \llrref{LLR_VPIT_CURRENT_COUNT_03}
 *    1. Set `count` to
 *       `(uint32_t) ((ch->reload - (elapsed(ch) % PIT_MAX_COUNT)) %`
 *       `PIT_MAX_COUNT)`.
 * 4. Return `(uint16_t) count`. \llrref{LLR_VPIT_CURRENT_COUNT_01},
 *    \llrref{LLR_VPIT_CURRENT_COUNT_02}, \llrref{LLR_VPIT_CURRENT_COUNT_03}
 *
 * @calls
 * - `periodic()`
 * - `elapsed()`
 */
static uint16_t current_count(const vpit_channel_t *ch)
{
    uint32_t count;

    if (!ch->counting) {
        count = ch->reload;
    } else if (periodic(ch)) {
        // Periodic modes reload the count when it reaches 0.
        count = ch->reload - (uint32_t) (elapsed(ch) % ch->reload);
    } else {
        // Modes 0, 1, 4, 5 count down once, then wrap around.
        count = (uint32_t) ((ch->reload - (elapsed(ch) % PIT_MAX_COUNT))
                            % PIT_MAX_COUNT);
    }
    return (uint16_t) count;
}

/**
 * @funcname `output`
 *
 * @description
 * Returns the output (OUT) of a channel now.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `const vpit_channel_t *` | [in] The channel. |
 *
 * @return true if the output is high.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `bool` | The output |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_OUTPUT_01}: `output()` shall give the present output of a
 *   channel.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_OUTPUT_01}: If `counting` of the channel is false and
 *   `mode` is not 0, `output()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_OUTPUT_01}
 * - \llrid{LLR_VPIT_OUTPUT_02}: If `counting` is false and `mode` is 0,
 *   `output()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_OUTPUT_01}
 * - \llrid{LLR_VPIT_OUTPUT_03}: If `counting` is true, `mode` is 0 and the
 *   result of `elapsed()` is `reload` or more, `output()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_OUTPUT_01}
 * - \llrid{LLR_VPIT_OUTPUT_04}: If `counting` is true, `mode` is 0 and the
 *   result of `elapsed()` is less than `reload`, `output()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_OUTPUT_01}
 * - \llrid{LLR_VPIT_OUTPUT_05}: If `counting` is true, `mode` is 3 and the
 *   remainder of the result of `elapsed()` divided by `reload` is less than
 *   `(reload + 1) / 2`, `output()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_OUTPUT_01}
 * - \llrid{LLR_VPIT_OUTPUT_06}: If `counting` is true, `mode` is 3 and the
 *   remainder of the result of `elapsed()` divided by `reload` is not less than
 *   `(reload + 1) / 2`, `output()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_OUTPUT_01}
 * - \llrid{LLR_VPIT_OUTPUT_07}: If `counting` is true and `mode` is neither 0
 *   nor 3, `output()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_OUTPUT_01}
 *
 * @design
 * 1. If `ch->counting` is false: \llrref{LLR_VPIT_OUTPUT_01},
 *    \llrref{LLR_VPIT_OUTPUT_02}
 *    1. Set `out` to `ch->mode != 0U`.
 * 2. Otherwise, if `ch->mode` is equal to `0U`: \llrref{LLR_VPIT_OUTPUT_03},
 *    \llrref{LLR_VPIT_OUTPUT_04}
 *    1. Set `out` to `elapsed(ch) >= ch->reload`.
 * 3. Otherwise, if `ch->mode` is equal to `3U`: \llrref{LLR_VPIT_OUTPUT_05},
 *    \llrref{LLR_VPIT_OUTPUT_06}
 *    1. Set `out` to `(elapsed(ch) % ch->reload) < ((ch->reload + 1U) / 2U)`.
 * 4. Otherwise: \llrref{LLR_VPIT_OUTPUT_07}
 *    1. Set `out` to `true`.
 * 5. Return `out`. \llrref{LLR_VPIT_OUTPUT_01}, \llrref{LLR_VPIT_OUTPUT_02},
 *    \llrref{LLR_VPIT_OUTPUT_03}, \llrref{LLR_VPIT_OUTPUT_04},
 *    \llrref{LLR_VPIT_OUTPUT_05}, \llrref{LLR_VPIT_OUTPUT_06},
 *    \llrref{LLR_VPIT_OUTPUT_07}
 *
 * @calls
 * - `elapsed()`
 */
static bool output(const vpit_channel_t *ch)
{
    bool out;

    if (!ch->counting) {
        out = ch->mode != 0U;   // Mode 0 sets the output low
    } else if (ch->mode == 0U) {
        out = elapsed(ch) >= ch->reload;
    } else if (ch->mode == 3U) {
        out = (elapsed(ch) % ch->reload) < ((ch->reload + 1U) / 2U);
    } else {
        out = true;
    }
    return out;
}

/**
 * @funcname `update_timer`
 *
 * @description
 * Programs the local APIC timer of this CPU for channel 0's period, or
 * stops it if channel 0 isn't counting periodically.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `const vpit_channel_t *` | [in] Channel 0. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `uint64_t` | The period in local APIC timer ticks |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_UPDATE_TIMER_01}: `update_timer()` shall give the guest IRQ
 *   0 at the rate programmed for channel 0, by the local APIC timer of its CPU.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_006}
 *
 * @llr
 * - \llrid{LLR_VPIT_UPDATE_TIMER_01}: If `counting` of the channel is true,
 *   `periodic()` returns true for it and `reload` times the result of
 *   `hv_lapic_hz()` divided by `HV_PIT_HZ` is 0, `update_timer()` shall call
 *   `lapic_timer_periodic()` once with 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_UPDATE_TIMER_01}
 * - \llrid{LLR_VPIT_UPDATE_TIMER_02}: If `counting` is true, `periodic()`
 *   returns true and `reload` times the result of `hv_lapic_hz()` divided by
 *   `HV_PIT_HZ` is more than `LAPIC_MAX_COUNT`, `update_timer()` shall call
 *   `lapic_timer_periodic()` once with `LAPIC_MAX_COUNT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_UPDATE_TIMER_01}
 * - \llrid{LLR_VPIT_UPDATE_TIMER_03}: If `counting` is true, `periodic()`
 *   returns true and `reload` times the result of `hv_lapic_hz()` divided by
 *   `HV_PIT_HZ` is from 1 to `LAPIC_MAX_COUNT`, `update_timer()` shall call
 *   `lapic_timer_periodic()` once with that value.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_UPDATE_TIMER_01}
 * - \llrid{LLR_VPIT_UPDATE_TIMER_04}: If `counting` of the channel is false, or
 *   `periodic()` returns false for it, `update_timer()` shall call
 *   `lapic_timer_stop()` once, without calling `lapic_timer_periodic()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_UPDATE_TIMER_01}
 *
 * @design
 * 1. If `ch->counting` is true and `periodic(ch)` is true:
 *    \llrref{LLR_VPIT_UPDATE_TIMER_01}, \llrref{LLR_VPIT_UPDATE_TIMER_02},
 *    \llrref{LLR_VPIT_UPDATE_TIMER_03}
 *    1. Set `count` to `((uint64_t) ch->reload * hv_lapic_hz()) / HV_PIT_HZ`.
 *    2. If `count` is equal to `0U`: \llrref{LLR_VPIT_UPDATE_TIMER_01}
 *       1. Set `count` to `1U`.
 *    3. Otherwise, if `count` is greater than `LAPIC_MAX_COUNT`:
 *       \llrref{LLR_VPIT_UPDATE_TIMER_02}, \llrref{LLR_VPIT_UPDATE_TIMER_03}
 *       1. Set `count` to `LAPIC_MAX_COUNT`.
 *    4. Call `lapic_timer_periodic()` with the argument `(uint32_t) count`.
 * 2. Otherwise: \llrref{LLR_VPIT_UPDATE_TIMER_04}
 *    1. Call `lapic_timer_stop()`.
 *
 * @calls
 * - `periodic()`
 * - `hv_lapic_hz()`
 * - `lapic_timer_periodic()`
 * - `lapic_timer_stop()`
 */
static void update_timer(const vpit_channel_t *ch)
{
    if (ch->counting && periodic(ch)) {
        // The same period in local APIC timer ticks.
        uint64_t count = ((uint64_t) ch->reload * hv_lapic_hz())
                         / HV_PIT_HZ;

        if (count == 0U) {
            count = 1U;
        } else if (count > LAPIC_MAX_COUNT) {
            count = LAPIC_MAX_COUNT;
        } else {
            // In range.
        }
        lapic_timer_periodic((uint32_t) count);
    } else {
        lapic_timer_stop();
    }
}

/**
 * @funcname `start`
 *
 * @description
 * Starts a channel counting if its count is loaded and its gate is on.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `vpit_channel_t *` | [in,out] The channel. |
 * | `gate` | `bool` | [in] The channel's gate. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_VPIT_START_01}: `start()` shall start a channel counting if its
 *   count is loaded and its gate is on.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_START_01}: If `loaded` of the channel is true and `gate` is
 *   true, `start()` shall set `counting` to true and `start_tsc` to the result
 *   of `x86_rdtsc()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_START_01}
 * - \llrid{LLR_VPIT_START_02}: If `loaded` of the channel is false, or `gate`
 *   is false, `start()` shall set `counting` to false and leave `start_tsc` as
 *   it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_START_01}
 *
 * @design
 * 1. Set `ch->counting` to `ch->loaded && gate`. \llrref{LLR_VPIT_START_01},
 *    \llrref{LLR_VPIT_START_02}
 * 2. If `ch->counting` is true: \llrref{LLR_VPIT_START_01}
 *    1. Call `x86_rdtsc()`, and store the result in `ch->start_tsc`.
 *
 * @calls
 * - `x86_rdtsc()`
 */
static void start(vpit_channel_t *ch, bool gate)
{
    ch->counting = ch->loaded && gate;
    if (ch->counting) {
        ch->start_tsc = x86_rdtsc();
    }
}

/**
 * @funcname `load`
 *
 * @description
 * Loads a count into a channel and starts it; for channel 0, programs the
 * timer.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit` | `vpit_t *` | [in] The virtual PIT. |
 * | `index` | `uint32_t` | [in] The channel, 0-2. |
 * | `count` | `uint32_t` | [in] The count; 0 is 65536. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `vpit_channel_t *` | The channel |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_LOAD_01}: `load()` shall load a count into a channel, 0
 *   meaning 65536, and start the channel.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 * - \hlrid{HLR_VPIT_LOAD_02}: `load()` shall program the timer of the host for
 *   a count loaded into channel 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_006}
 *
 * @llr
 * - \llrid{LLR_VPIT_LOAD_01}: If `count` is 0, `load()` shall set `reload` of
 *   the channel `index` of `pit` to `PIT_MAX_COUNT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_LOAD_01}
 * - \llrid{LLR_VPIT_LOAD_02}: If `count` is not 0, `load()` shall set `reload`
 *   of the channel `index` of `pit` to `count`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_LOAD_01}
 * - \llrid{LLR_VPIT_LOAD_03}: `load()` shall set `loaded` of the channel to
 *   true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_LOAD_01}
 * - \llrid{LLR_VPIT_LOAD_04}: If `index` is not 2, `load()` shall call
 *   `start()` once with the channel and true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_LOAD_01}
 * - \llrid{LLR_VPIT_LOAD_05}: If `index` is 2, `load()` shall call `start()`
 *   once with the channel and `gate2` of `pit`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_LOAD_01}
 * - \llrid{LLR_VPIT_LOAD_06}: If and only if `index` is 0, `load()` shall call
 *   `update_timer()` once with the channel, after `start()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_LOAD_02}
 *
 * @design
 * 1. Set `ch` to the address of `pit->channel[index]`.
 *    \llrref{LLR_VPIT_LOAD_01}, \llrref{LLR_VPIT_LOAD_02},
 *    \llrref{LLR_VPIT_LOAD_03}, \llrref{LLR_VPIT_LOAD_04},
 *    \llrref{LLR_VPIT_LOAD_05}, \llrref{LLR_VPIT_LOAD_06}
 * 2. Set `ch->reload` to `PIT_MAX_COUNT` if `count` is equal to `0U`, otherwise
 *    `count`. \llrref{LLR_VPIT_LOAD_01}, \llrref{LLR_VPIT_LOAD_02}
 * 3. Set `ch->loaded` to `true`. \llrref{LLR_VPIT_LOAD_03}
 * 4. Call `start()` with the arguments `ch` and `(index != 2U) || pit->gate2`.
 *    \llrref{LLR_VPIT_LOAD_04}, \llrref{LLR_VPIT_LOAD_05}
 * 5. If `index` is equal to `0U`: \llrref{LLR_VPIT_LOAD_06}
 *    1. Call `update_timer()` with the argument `ch`.
 *
 * @calls
 * - `start()`
 * - `update_timer()`
 */
static void load(vpit_t *pit, uint32_t index, uint32_t count)
{
    vpit_channel_t *ch = &pit->channel[index];

    ch->reload = (count == 0U) ? PIT_MAX_COUNT : count;
    ch->loaded = true;
    start(ch, (index != 2U) || pit->gate2);
    if (index == 0U) {
        update_timer(ch);
    }
}

/**
 * @funcname `latch`
 *
 * @description
 * Latches the count of a channel (counter latch or read-back command), if
 * it isn't latched already.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `vpit_channel_t *` | [in,out] The channel. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_VPIT_LATCH_01}: `latch()` shall latch the count of a channel,
 *   unless it is latched.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_LATCH_01}: If and only if `latched` of the channel is
 *   false, `latch()` shall set `latch` to the result of `current_count()`,
 *   `latched` to true and `read_msb` to false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_LATCH_01}
 *
 * @design
 * 1. If `ch->latched` is false: \llrref{LLR_VPIT_LATCH_01}
 *    1. Call `current_count()` with the argument `ch`, and store the result in
 *       `ch->latch`.
 *    2. Set `ch->latched` to `true`.
 *    3. Set `ch->read_msb` to `false`.
 *
 * @calls
 * - `current_count()`
 */
static void latch(vpit_channel_t *ch)
{
    if (!ch->latched) {
        ch->latch = current_count(ch);
        ch->latched = true;
        ch->read_msb = false;
    }
}

/**
 * @funcname `pit_command`
 *
 * @description
 * Handles a write to the PIT command port: read-back, counter latch, or the
 * access and mode of a channel (which stops it until a count is written).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit` | `vpit_t *` | [in,out] The virtual PIT. |
 * | `value` | `uint8_t` | [in] The command. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `index` | `uint32_t` | The channel (bits 7-6; 3 is read-back) |
 * | `access` | `uint32_t` | The access mode (bits 5-4) |
 * | `i` | `uint32_t` | The channel of a read-back |
 * | `ch` | `vpit_channel_t *` | The channel being programmed |
 * | `mode` | `uint8_t` | The operating mode (bits 3-1) |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_PIT_COMMAND_01}: `pit_command()` shall handle a write to
 *   the command port: read-back, counter latch, or the access and mode of a
 *   channel, which stops it until a count is written.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_PIT_COMMAND_01}: If the bits 7 to 6 of `value` are
 *   `PIT_READ_BACK` and `READ_BACK_NO_COUNT` is not set in `value`,
 *   `pit_command()` shall call `latch()` once for each of the `PIT_CHANNELS`
 *   channels `i` whose bit `i` plus 1 is set in `value`, and for no other
 *   channel.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_PIT_COMMAND_01}
 * - \llrid{LLR_VPIT_PIT_COMMAND_02}: If the bits 7 to 6 of `value` are
 *   `PIT_READ_BACK` and `READ_BACK_NO_COUNT` is set in `value`, `pit_command()`
 *   shall leave the channels as they are.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_PIT_COMMAND_01}
 * - \llrid{LLR_VPIT_PIT_COMMAND_03}: If the bits 7 to 6 of `value` are not
 *   `PIT_READ_BACK` and its bits 5 to 4 are `PIT_ACCESS_LATCH`, `pit_command()`
 *   shall call `latch()` once for the channel whose number is in the bits 7
 *   to 6.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_PIT_COMMAND_01}
 * - \llrid{LLR_VPIT_PIT_COMMAND_04}: If the bits 7 to 6 of `value` are not
 *   `PIT_READ_BACK`, its bits 5 to 4 are not `PIT_ACCESS_LATCH` and its bits 3
 *   to 1 are more than 5, `pit_command()` shall set `mode` of the channel whose
 *   number is in the bits 7 to 6 to the bits 3 to 1 minus 4.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_PIT_COMMAND_01}
 * - \llrid{LLR_VPIT_PIT_COMMAND_05}: If the bits 7 to 6 of `value` are not
 *   `PIT_READ_BACK`, its bits 5 to 4 are not `PIT_ACCESS_LATCH` and its bits 3
 *   to 1 are 5 or less, `pit_command()` shall set `mode` of the channel whose
 *   number is in the bits 7 to 6 to the bits 3 to 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_PIT_COMMAND_01}
 * - \llrid{LLR_VPIT_PIT_COMMAND_06}: If the bits 7 to 6 of `value` are not
 *   `PIT_READ_BACK` and its bits 5 to 4 are not `PIT_ACCESS_LATCH`,
 *   `pit_command()` shall set `access` of the channel whose number is in the
 *   bits 7 to 6 to the bits 5 to 4, and its `write_msb`, `read_msb`, `latched`,
 *   `loaded` and `counting` to false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_PIT_COMMAND_01}
 * - \llrid{LLR_VPIT_PIT_COMMAND_07}: If and only if the bits 7 to 6 of `value`
 *   are 0 and its bits 5 to 4 are not `PIT_ACCESS_LATCH`, `pit_command()` shall
 *   call `update_timer()` once with channel 0, after it has changed the
 *   channel.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_PIT_COMMAND_01}
 *
 * @design
 * 1. Set `index` to `(uint32_t) value >> 6`. \llrref{LLR_VPIT_PIT_COMMAND_01},
 *    \llrref{LLR_VPIT_PIT_COMMAND_02}, \llrref{LLR_VPIT_PIT_COMMAND_03},
 *    \llrref{LLR_VPIT_PIT_COMMAND_04}, \llrref{LLR_VPIT_PIT_COMMAND_05},
 *    \llrref{LLR_VPIT_PIT_COMMAND_06}, \llrref{LLR_VPIT_PIT_COMMAND_07}
 * 2. Set `access` to `((uint32_t) value >> 4) & 3U`.
 *    \llrref{LLR_VPIT_PIT_COMMAND_03}, \llrref{LLR_VPIT_PIT_COMMAND_04},
 *    \llrref{LLR_VPIT_PIT_COMMAND_05}, \llrref{LLR_VPIT_PIT_COMMAND_06},
 *    \llrref{LLR_VPIT_PIT_COMMAND_07}
 * 3. If `index` is equal to `PIT_READ_BACK`: \llrref{LLR_VPIT_PIT_COMMAND_01},
 *    \llrref{LLR_VPIT_PIT_COMMAND_02}
 *    1. If `(value & READ_BACK_NO_COUNT)` is equal to `0U`:
 *       \llrref{LLR_VPIT_PIT_COMMAND_01}
 *       1. Set `i` to `0U`; then while `i` is less than `PIT_CHANNELS`, repeat
 *          (after each pass: increment `i`):
 *          1. If `(value & (2U << i))` is not equal to `0U`:
 *             1. Call `latch()` with the address of `pit->channel[i]`.
 * 4. Otherwise, if `access` is equal to `PIT_ACCESS_LATCH`:
 *    \llrref{LLR_VPIT_PIT_COMMAND_03}
 *    1. Call `latch()` with the address of `pit->channel[index]`.
 * 5. Otherwise: \llrref{LLR_VPIT_PIT_COMMAND_04},
 *    \llrref{LLR_VPIT_PIT_COMMAND_05}, \llrref{LLR_VPIT_PIT_COMMAND_06},
 *    \llrref{LLR_VPIT_PIT_COMMAND_07}
 *    1. Set `ch` to the address of `pit->channel[index]`.
 *    2. Set `mode` to `(uint8_t) (((uint32_t) value >> 1) & 7U)`.
 *       \llrref{LLR_VPIT_PIT_COMMAND_04}, \llrref{LLR_VPIT_PIT_COMMAND_05}
 *    3. Set `ch->mode` to `(uint8_t) (mode - 4U)` if `mode` is greater than
 *       `5U`, otherwise `mode`. \llrref{LLR_VPIT_PIT_COMMAND_04},
 *       \llrref{LLR_VPIT_PIT_COMMAND_05}
 *    4. Set `ch->access` to `(uint8_t) access`.
 *       \llrref{LLR_VPIT_PIT_COMMAND_06}
 *    5. Set `ch->write_msb` to `false`. \llrref{LLR_VPIT_PIT_COMMAND_06}
 *    6. Set `ch->read_msb` to `false`. \llrref{LLR_VPIT_PIT_COMMAND_06}
 *    7. Set `ch->latched` to `false`. \llrref{LLR_VPIT_PIT_COMMAND_06}
 *    8. Set `ch->loaded` to `false`. \llrref{LLR_VPIT_PIT_COMMAND_06}
 *    9. Set `ch->counting` to `false`. \llrref{LLR_VPIT_PIT_COMMAND_06}
 *    10. If `index` is equal to `0U`: \llrref{LLR_VPIT_PIT_COMMAND_07}
 *        1. Call `update_timer()` with the argument `ch`.
 *
 * @calls
 * - `latch()`
 * - `update_timer()`
 */
static void pit_command(vpit_t *pit, uint8_t value)
{
    // Bits 7-6: channel (3: read-back), 5-4: access, 3-1: mode.
    uint32_t index = (uint32_t) value >> 6;
    uint32_t access = ((uint32_t) value >> 4) & 3U;

    if (index == PIT_READ_BACK) {
        // Latch the counts of the channels in bits 1-3 (no status).
        if ((value & READ_BACK_NO_COUNT) == 0U) {
            for (uint32_t i = 0U; i < PIT_CHANNELS; i++) {
                if ((value & (2U << i)) != 0U) {
                    latch(&pit->channel[i]);
                }
            }
        }
    } else if (access == PIT_ACCESS_LATCH) {
        latch(&pit->channel[index]);
    } else {
        vpit_channel_t *ch = &pit->channel[index];
        uint8_t mode = (uint8_t) (((uint32_t) value >> 1) & 7U);

        // Modes 6 and 7 are modes 2 and 3.
        ch->mode = (mode > 5U) ? (uint8_t) (mode - 4U) : mode;
        ch->access = (uint8_t) access;
        ch->write_msb = false;
        ch->read_msb = false;
        ch->latched = false;
        ch->loaded = false;
        ch->counting = false;
        if (index == 0U) {
            update_timer(ch);
        }
    }
}

/**
 * @funcname `write_count`
 *
 * @description
 * Writes a byte of a channel's count, as its access mode says: LSB only,
 * MSB only, or LSB then MSB.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit` | `vpit_t *` | [in] The virtual PIT. |
 * | `index` | `uint32_t` | [in] The channel, 0-2. |
 * | `value` | `uint8_t` | [in] The byte. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `vpit_channel_t *` | The channel |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_WRITE_COUNT_01}: `write_count()` shall write a byte of the
 *   count of a channel as its access mode says: low only, high only, or low
 *   then high.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_WRITE_COUNT_01}: If `access` of the channel `index` is
 *   `PIT_ACCESS_LSB`, `write_count()` shall call `load()` once with `pit`,
 *   `index` and `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_WRITE_COUNT_01}
 * - \llrid{LLR_VPIT_WRITE_COUNT_02}: If `access` of the channel is
 *   `PIT_ACCESS_MSB`, `write_count()` shall call `load()` once with `pit`,
 *   `index` and `value` times 256.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_WRITE_COUNT_01}
 * - \llrid{LLR_VPIT_WRITE_COUNT_03}: If `access` of the channel is neither
 *   `PIT_ACCESS_LSB` nor `PIT_ACCESS_MSB` and `write_msb` is false,
 *   `write_count()` shall set `written_lsb` to `value` and `write_msb` to true,
 *   without calling `load()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_WRITE_COUNT_01}
 * - \llrid{LLR_VPIT_WRITE_COUNT_04}: If `access` of the channel is neither
 *   `PIT_ACCESS_LSB` nor `PIT_ACCESS_MSB` and `write_msb` is true,
 *   `write_count()` shall set `write_msb` to false and call `load()` once with
 *   `pit`, `index` and `written_lsb` plus `value` times 256.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_WRITE_COUNT_01}
 *
 * @design
 * 1. Set `ch` to the address of `pit->channel[index]`.
 *    \llrref{LLR_VPIT_WRITE_COUNT_01}, \llrref{LLR_VPIT_WRITE_COUNT_02},
 *    \llrref{LLR_VPIT_WRITE_COUNT_03}, \llrref{LLR_VPIT_WRITE_COUNT_04}
 * 2. If `ch->access` is equal to `PIT_ACCESS_LSB`:
 *    \llrref{LLR_VPIT_WRITE_COUNT_01}
 *    1. Call `load()` with the arguments `pit`, `index` and `value`.
 * 3. Otherwise, if `ch->access` is equal to `PIT_ACCESS_MSB`:
 *    \llrref{LLR_VPIT_WRITE_COUNT_02}
 *    1. Call `load()` with the arguments `pit`, `index` and
 *       `(uint32_t) value << 8`.
 * 4. Otherwise, if `ch->write_msb` is false: \llrref{LLR_VPIT_WRITE_COUNT_03}
 *    1. Set `ch->written_lsb` to `value`.
 *    2. Set `ch->write_msb` to `true`.
 * 5. Otherwise: \llrref{LLR_VPIT_WRITE_COUNT_04}
 *    1. Set `ch->write_msb` to `false`.
 *    2. Call `load()` with the arguments `pit`, `index` and
 *       `(uint32_t) ch->written_lsb | ((uint32_t) value << 8)`.
 *
 * @calls
 * - `load()`
 */
static void write_count(vpit_t *pit, uint32_t index, uint8_t value)
{
    vpit_channel_t *ch = &pit->channel[index];

    if (ch->access == PIT_ACCESS_LSB) {
        load(pit, index, value);
    } else if (ch->access == PIT_ACCESS_MSB) {
        load(pit, index, (uint32_t) value << 8);
    } else if (!ch->write_msb) {
        ch->written_lsb = value;
        ch->write_msb = true;
    } else {
        ch->write_msb = false;
        load(pit, index, (uint32_t) ch->written_lsb
                         | ((uint32_t) value << 8));
    }
}

/**
 * @funcname `read_count`
 *
 * @description
 * Reads a byte of a channel's latched or current count, as its access mode
 * says.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ch` | `vpit_channel_t *` | [in,out] The channel. |
 *
 * @return The byte.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `uint16_t` | The latched or current count |
 * | `value` | `uint8_t` | The byte read |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_READ_COUNT_01}: `read_count()` shall give a byte of the
 *   latched or present count of a channel as its access mode says.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_READ_COUNT_01}: If `latched` of the channel is true,
 *   `read_count()` shall take `latch` as the count.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_READ_COUNT_01}
 * - \llrid{LLR_VPIT_READ_COUNT_02}: If `latched` of the channel is false,
 *   `read_count()` shall take the result of `current_count()` as the count.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_READ_COUNT_01}
 * - \llrid{LLR_VPIT_READ_COUNT_03}: If `access` of the channel is
 *   `PIT_ACCESS_LSB`, `read_count()` shall return the low byte of the count and
 *   set `latched` to false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_READ_COUNT_01}
 * - \llrid{LLR_VPIT_READ_COUNT_04}: If `access` of the channel is
 *   `PIT_ACCESS_MSB`, `read_count()` shall return the high byte of the count
 *   and set `latched` to false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_READ_COUNT_01}
 * - \llrid{LLR_VPIT_READ_COUNT_05}: If `access` of the channel is neither
 *   `PIT_ACCESS_LSB` nor `PIT_ACCESS_MSB` and `read_msb` is false,
 *   `read_count()` shall return the low byte of the count and set `read_msb` to
 *   true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_READ_COUNT_01}
 * - \llrid{LLR_VPIT_READ_COUNT_06}: If `access` of the channel is neither
 *   `PIT_ACCESS_LSB` nor `PIT_ACCESS_MSB` and `read_msb` is true,
 *   `read_count()` shall return the high byte of the count and set `read_msb`
 *   and `latched` to false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_READ_COUNT_01}
 *
 * @design
 * 1. Set `count` to `ch->latch` if `ch->latched` is true, otherwise
 *    `current_count(ch)`. \llrref{LLR_VPIT_READ_COUNT_01},
 *    \llrref{LLR_VPIT_READ_COUNT_02}
 * 2. If `ch->access` is equal to `PIT_ACCESS_LSB`:
 *    \llrref{LLR_VPIT_READ_COUNT_03}
 *    1. Set `value` to `(uint8_t) count`.
 *    2. Set `ch->latched` to `false`.
 * 3. Otherwise, if `ch->access` is equal to `PIT_ACCESS_MSB`:
 *    \llrref{LLR_VPIT_READ_COUNT_04}
 *    1. Set `value` to `(uint8_t) (count >> 8)`.
 *    2. Set `ch->latched` to `false`.
 * 4. Otherwise, if `ch->read_msb` is false: \llrref{LLR_VPIT_READ_COUNT_05}
 *    1. Set `value` to `(uint8_t) count`.
 *    2. Set `ch->read_msb` to `true`.
 * 5. Otherwise: \llrref{LLR_VPIT_READ_COUNT_06}
 *    1. Set `value` to `(uint8_t) (count >> 8)`.
 *    2. Set `ch->read_msb` to `false`.
 *    3. Set `ch->latched` to `false`.
 * 6. Return `value`. \llrref{LLR_VPIT_READ_COUNT_03},
 *    \llrref{LLR_VPIT_READ_COUNT_04}, \llrref{LLR_VPIT_READ_COUNT_05},
 *    \llrref{LLR_VPIT_READ_COUNT_06}
 *
 * @calls
 * - `current_count()`
 */
static uint8_t read_count(vpit_channel_t *ch)
{
    uint16_t count = ch->latched ? ch->latch : current_count(ch);
    uint8_t value;

    if (ch->access == PIT_ACCESS_LSB) {
        value = (uint8_t) count;
        ch->latched = false;
    } else if (ch->access == PIT_ACCESS_MSB) {
        value = (uint8_t) (count >> 8);
        ch->latched = false;
    } else if (!ch->read_msb) {
        value = (uint8_t) count;
        ch->read_msb = true;
    } else {
        value = (uint8_t) (count >> 8);
        ch->read_msb = false;
        ch->latched = false;
    }
    return value;
}

/**
 * @funcname `vpit_write`
 *
 * @description
 * Handles a write of the guest to a PIT port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit` | `vpit_t *` | [in,out] The virtual PIT. |
 * | `port` | `uint16_t` | [in] The port, 0x40-0x43. |
 * | `value` | `uint8_t` | [in] The value written. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_VPIT_VPIT_WRITE_01}: `vpit_write()` shall handle a write of a
 *   guest to a port of the timer.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_VPIT_WRITE_01}: If `port` is `PIT_COMMAND`, `vpit_write()`
 *   shall call `pit_command()` once with `pit` and `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_WRITE_01}
 * - \llrid{LLR_VPIT_VPIT_WRITE_02}: If `port` is not `PIT_COMMAND`,
 *   `vpit_write()` shall call `write_count()` once with `pit`, `port` minus
 *   `PIT_CHANNEL0` and `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_WRITE_01}
 *
 * @design
 * 1. If `port` is equal to `PIT_COMMAND`: \llrref{LLR_VPIT_VPIT_WRITE_01}
 *    1. Call `pit_command()` with the arguments `pit` and `value`.
 * 2. Otherwise: \llrref{LLR_VPIT_VPIT_WRITE_02}
 *    1. Call `write_count()` with the arguments `pit`,
 *       `(uint32_t) port - PIT_CHANNEL0` and `value`.
 *
 * @calls
 * - `pit_command()`
 * - `write_count()`
 */
void vpit_write(vpit_t *pit, uint16_t port, uint8_t value)
{
    if (port == PIT_COMMAND) {
        pit_command(pit, value);
    } else {
        write_count(pit, (uint32_t) port - PIT_CHANNEL0, value);
    }
}

/**
 * @funcname `vpit_read`
 *
 * @description
 * Handles a read of the guest from a PIT port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit` | `vpit_t *` | [in,out] The virtual PIT. |
 * | `port` | `uint16_t` | [in] The port, 0x40-0x43. |
 *
 * @return A byte of the channel's count, or 0xFF for the command port.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint8_t` | The value read |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_VPIT_READ_01}: `vpit_read()` shall handle a read of a guest
 *   from a port of the timer.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_VPIT_READ_01}: If `port` is `PIT_COMMAND`, `vpit_read()`
 *   shall return `0xFF`, without calling `read_count()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_READ_01}
 * - \llrid{LLR_VPIT_VPIT_READ_02}: If `port` is not `PIT_COMMAND`,
 *   `vpit_read()` shall return the result of `read_count()` for the channel
 *   `port` minus `PIT_CHANNEL0` of `pit`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_READ_01}
 *
 * @design
 * 1. Set `value` to `0xFFU`. \llrref{LLR_VPIT_VPIT_READ_01}
 * 2. If `port` is not equal to `PIT_COMMAND`: \llrref{LLR_VPIT_VPIT_READ_02}
 *    1. Call `read_count()` with the address of
 *       `pit->channel[(uint32_t) port - PIT_CHANNEL0]`, and store the result in
 *       `value`.
 * 3. Return `value`. \llrref{LLR_VPIT_VPIT_READ_01},
 *    \llrref{LLR_VPIT_VPIT_READ_02}
 *
 * @calls
 * - `read_count()`
 */
uint8_t vpit_read(vpit_t *pit, uint16_t port)
{
    uint8_t value = 0xFFU;   // The command port can't be read

    if (port != PIT_COMMAND) {
        value = read_count(&pit->channel[(uint32_t) port - PIT_CHANNEL0]);
    }
    return value;
}

/**
 * @funcname `vpit_write_port_b`
 *
 * @description
 * Handles a write of the guest to port B: the gate of channel 2 and the
 * speaker bit.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit` | `vpit_t *` | [in,out] The virtual PIT. |
 * | `value` | `uint8_t` | [in] The value written. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `gate` | `bool` | The new gate of channel 2 (bit 0) |
 * | `ch` | `vpit_channel_t *` | Channel 2 |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_VPIT_WRITE_PORT_B_01}: `vpit_write_port_b()` shall handle a
 *   write of a guest to port B: the gate of channel 2 and the speaker bit.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_VPIT_WRITE_PORT_B_01}: `vpit_write_port_b()` shall set
 *   `speaker` of `pit` to true if `PORT_B_SPEAKER` is set in `value` and to
 *   false if it is not, and, after it has handled channel 2, `gate2` to true if
 *   `PORT_B_GATE2` is set in `value` and to false if it is not.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_WRITE_PORT_B_01}
 * - \llrid{LLR_VPIT_VPIT_WRITE_PORT_B_02}: If `PORT_B_GATE2` is set in `value`
 *   and `gate2` of `pit` is false, `vpit_write_port_b()` shall call `start()`
 *   once with channel 2 and true.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_WRITE_PORT_B_01}
 * - \llrid{LLR_VPIT_VPIT_WRITE_PORT_B_03}: If `PORT_B_GATE2` is not set in
 *   `value`, `vpit_write_port_b()` shall set `counting` of channel 2 to false.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_WRITE_PORT_B_01}
 * - \llrid{LLR_VPIT_VPIT_WRITE_PORT_B_04}: If `PORT_B_GATE2` is set in `value`
 *   and `gate2` of `pit` is true, `vpit_write_port_b()` shall leave channel 2
 *   as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_WRITE_PORT_B_01}
 *
 * @design
 * 1. Set `gate` to `(value & PORT_B_GATE2) != 0U`.
 *    \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_01},
 *    \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_02},
 *    \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_03},
 *    \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_04}
 * 2. Set `ch` to the address of `pit->channel[2]`.
 *    \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_02},
 *    \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_03}
 * 3. Set `pit->speaker` to `(value & PORT_B_SPEAKER) != 0U`.
 *    \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_01}
 * 4. If `gate` is true and `pit->gate2` is false:
 *    \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_02}
 *    1. Call `start()` with the arguments `ch` and `true`.
 * 5. Otherwise, if `gate` is false: \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_03},
 *    \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_04}
 *    1. Set `ch->counting` to `false`.
 * 6. Set `pit->gate2` to `gate`. \llrref{LLR_VPIT_VPIT_WRITE_PORT_B_01}
 *
 * @calls
 * - `start()`
 */
void vpit_write_port_b(vpit_t *pit, uint8_t value)
{
    bool gate = (value & PORT_B_GATE2) != 0U;
    vpit_channel_t *ch = &pit->channel[2];

    pit->speaker = (value & PORT_B_SPEAKER) != 0U;
    if (gate && !pit->gate2) {
        start(ch, true);    // Rising edge: counting (re)starts
    } else if (!gate) {
        ch->counting = false;   // Gate off: channel 2 stops
    } else {
        // The gate stays on.
    }
    pit->gate2 = gate;
}

/**
 * @funcname `vpit_read_port_b`
 *
 * @description
 * Handles a read of the guest from port B: the gate and speaker bits, the
 * refresh toggle (about every 15 us) and the output of channel 2.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit` | `const vpit_t *` | [in] The virtual PIT. |
 *
 * @return The value of port B.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint8_t` | The value of port B |
 * | `now` | `uint64_t` | The time in PIT ticks |
 *
 * @hlr
 * - \hlrid{HLR_VPIT_VPIT_READ_PORT_B_01}: `vpit_read_port_b()` shall give a
 *   guest port B: the gate and speaker bits, the refresh toggle and the output
 *   of channel 2.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_TMR_002}
 *
 * @llr
 * - \llrid{LLR_VPIT_VPIT_READ_PORT_B_01}: If and only if `gate2` of `pit` is
 *   true, `vpit_read_port_b()` shall set `PORT_B_GATE2` in the value that it
 *   returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_READ_PORT_B_01}
 * - \llrid{LLR_VPIT_VPIT_READ_PORT_B_02}: If and only if `speaker` of `pit` is
 *   true, `vpit_read_port_b()` shall set `PORT_B_SPEAKER` in the value that it
 *   returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_READ_PORT_B_01}
 * - \llrid{LLR_VPIT_VPIT_READ_PORT_B_03}: If and only if the result of
 *   `hv_tsc_to_pit_ticks()` for the result of `x86_rdtsc()`, divided by
 *   `REFRESH_PIT_TICKS`, is odd, `vpit_read_port_b()` shall set
 *   `PORT_B_REFRESH` in the value that it returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_READ_PORT_B_01}
 * - \llrid{LLR_VPIT_VPIT_READ_PORT_B_04}: If and only if `output()` returns
 *   true for channel 2, `vpit_read_port_b()` shall set `PORT_B_OUT2` in the
 *   value that it returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_READ_PORT_B_01}
 * - \llrid{LLR_VPIT_VPIT_READ_PORT_B_05}: `vpit_read_port_b()` shall return 0
 *   in the bits other than `PORT_B_GATE2`, `PORT_B_SPEAKER`, `PORT_B_REFRESH`
 *   and `PORT_B_OUT2`.
 *   <br>Traces to:
 *   - \hlrref{HLR_VPIT_VPIT_READ_PORT_B_01}
 *
 * @design
 * 1. Set `value` to `0U`. \llrref{LLR_VPIT_VPIT_READ_PORT_B_05}
 * 2. Call `hv_tsc_to_pit_ticks()` with the argument `x86_rdtsc()`, and store
 *    the result in `now`. \llrref{LLR_VPIT_VPIT_READ_PORT_B_03}
 * 3. If `pit->gate2` is true: \llrref{LLR_VPIT_VPIT_READ_PORT_B_01}
 *    1. Set the bits `PORT_B_GATE2` in `value`.
 * 4. If `pit->speaker` is true: \llrref{LLR_VPIT_VPIT_READ_PORT_B_02}
 *    1. Set the bits `PORT_B_SPEAKER` in `value`.
 * 5. If `((now / REFRESH_PIT_TICKS) % 2U)` is not equal to `0U`:
 *    \llrref{LLR_VPIT_VPIT_READ_PORT_B_03}
 *    1. Set the bits `PORT_B_REFRESH` in `value`.
 * 6. If `output(&pit->channel[2])` is true:
 *    \llrref{LLR_VPIT_VPIT_READ_PORT_B_04}
 *    1. Set the bits `PORT_B_OUT2` in `value`.
 * 7. Return `value`. \llrref{LLR_VPIT_VPIT_READ_PORT_B_01},
 *    \llrref{LLR_VPIT_VPIT_READ_PORT_B_02},
 *    \llrref{LLR_VPIT_VPIT_READ_PORT_B_03},
 *    \llrref{LLR_VPIT_VPIT_READ_PORT_B_04},
 *    \llrref{LLR_VPIT_VPIT_READ_PORT_B_05}
 *
 * @calls
 * - `hv_tsc_to_pit_ticks()`
 * - `x86_rdtsc()`
 * - `output()`
 */
uint8_t vpit_read_port_b(const vpit_t *pit)
{
    uint8_t value = 0U;
    uint64_t now = hv_tsc_to_pit_ticks(x86_rdtsc());

    if (pit->gate2) {
        value |= PORT_B_GATE2;
    }
    if (pit->speaker) {
        value |= PORT_B_SPEAKER;
    }
    if (((now / REFRESH_PIT_TICKS) % 2U) != 0U) {
        value |= PORT_B_REFRESH;
    }
    if (output(&pit->channel[2])) {
        value |= PORT_B_OUT2;
    }
    return value;
}
