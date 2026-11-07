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
 * Virtual 8254 PIT (ports 0x40-0x43) and port B (0x61).
 *
 * The counters are computed from the TSC, so reading them costs no timer.
 * Channel 0 in a periodic mode (2 or 3) raises IRQ 0: the local APIC
 * timer of the physical CPU is programmed with the same period, and each
 * of its interrupts becomes one IRQ 0 (svm.c). Channel 2 is gated by port
 * B bit 0, and its output is port B bit 5 (used to calibrate the TSC).
 * Channel 1 counts but drives nothing. BCD counting and the read-back
 * status are not emulated.
 */

#include <hv/hv.h>
#include <hv/x86.h>

#define PIT_CHANNEL0        0x40U
#define PIT_COMMAND         0x43U
#define PIT_CHANNELS        3U
#define PIT_READ_BACK       3U
#define PIT_ACCESS_LATCH    0U
#define PIT_ACCESS_LSB      1U
#define PIT_ACCESS_MSB      2U
#define PIT_ACCESS_WORD     3U
#define PIT_MAX_COUNT       0x10000U
#define READ_BACK_NO_COUNT  0x20U

#define PORT_B_GATE2        0x01U
#define PORT_B_SPEAKER      0x02U
#define PORT_B_REFRESH      0x10U
#define PORT_B_OUT2         0x20U

// Refresh request toggle period in PIT ticks (about 15 us).
#define REFRESH_PIT_TICKS   18U

#define LAPIC_MAX_COUNT     0xFFFFFFFFUL

// Reset a channel: mode 0, LSB then MSB, no count.
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

// Reset the virtual PIT and port B, and stop the timer behind channel 0.
void vpit_reset(vpit_t *pit)
{
    for (uint32_t i = 0U; i < PIT_CHANNELS; i++) {
        channel_reset(&pit->channel[i]);
    }
    pit->gate2 = false;
    pit->speaker = false;
    lapic_timer_stop();
}

// Check whether a channel counts in a periodic mode (2 or 3).
static bool periodic(const vpit_channel_t *ch)
{
    return (ch->mode == 2U) || (ch->mode == 3U);
}

// Check whether channel 0 raises IRQ 0 periodically.
bool vpit_timer_running(const vpit_t *pit)
{
    return pit->channel[0].counting && periodic(&pit->channel[0]);
}

// PIT ticks since the channel started counting.
static uint64_t elapsed(const vpit_channel_t *ch)
{
    return hv_tsc_to_pit_ticks(x86_rdtsc() - ch->start_tsc);
}

// Count of a channel now (16 bits).
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

// Output (OUT) of a channel now.
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

// Program the local APIC timer for channel 0's period.
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

// Start counting if the count is loaded and the gate is on.
static void start(vpit_channel_t *ch, bool gate)
{
    ch->counting = ch->loaded && gate;
    if (ch->counting) {
        ch->start_tsc = x86_rdtsc();
    }
}

/*
 * Load a count (0 is 65536) into a channel and start it; for channel 0, program
 * the timer.
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

// Latch the count of a channel (counter latch or read-back command).
static void latch(vpit_channel_t *ch)
{
    if (!ch->latched) {
        ch->latch = current_count(ch);
        ch->latched = true;
        ch->read_msb = false;
    }
}

/*
 * Write to the command port: read-back, counter latch, or the access and mode
 * of a channel (which stops it until a count is written).
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

// Write a byte of a channel's count, as its access mode says.
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

/*
 * Read a byte of a channel's (latched or current) count, as its access mode
 * says.
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

// Write of the guest to a PIT port.
void vpit_write(vpit_t *pit, uint16_t port, uint8_t value)
{
    if (port == PIT_COMMAND) {
        pit_command(pit, value);
    } else {
        write_count(pit, (uint32_t) port - PIT_CHANNEL0, value);
    }
}

// Read of the guest from a PIT port.
uint8_t vpit_read(vpit_t *pit, uint16_t port)
{
    uint8_t value = 0xFFU;   // The command port can't be read

    if (port != PIT_COMMAND) {
        value = read_count(&pit->channel[(uint32_t) port - PIT_CHANNEL0]);
    }
    return value;
}

// Write of the guest to port B: channel 2 gate and speaker.
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

/*
 * Read of the guest from port B: gate, speaker, refresh toggle and channel 2
 * output.
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
