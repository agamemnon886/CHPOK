/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * boards/x86-qemu/pit.c to x86_64 (64-bit); the system clock is based on
 * the TSC.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#include <errno.h>
#include <core/time.h>
#include <core/sched.h>
#include <ioports.h>

#include "pic.h"

#include "pit.h"

#include <bsp/bsp.h>

#include <asp/entries.h>

#define OSCILLATOR_RATE 1193180 /** The oscillation rate of x86 clock */
#define PIT_BASE 0x40

/* Port 0x61: PIT channel 2 gate (bit 0), speaker (bit 1), channel 2 output (bit 5). */
#define PORT_B 0x61
#define PORT_B_GATE2 0x01
#define PORT_B_SPEAKER 0x02
#define PORT_B_OUT2 0x20

/*
 * Duration of TSC calibration (in PIT ticks, at most 0xffff): 50 ms.
 */
#define CALIBRATION_PIT_TICKS (OSCILLATOR_RATE / 20)

/*
 * System time is read from the TSC, not counted in timer ticks: ticks
 * may be lost (e.g. QEMU on macOS delivers less than 1000 timer
 * interrupts per second), which would slow down the system time.
 */
static uint64_t tsc_freq;   /* TSC ticks per second */
static uint64_t tsc_start;  /* TSC value at boot */

/*
 * Hardcoded calendar time at the beginning of the OS loading.
 * 
 * Could be obtained on https://www.timeanddate.com/.
 */
static time_t base_calendar_time = 1480330081; // On 28.11.2016


void ja_bsp_process_timer(interrupt_frame* frame)
{
   (void) frame;
   pok_pic_eoi (PIT_IRQ);

   jet_on_tick();
}

static uint64_t read_tsc(void)
{
   uint32_t low, high;
   asm volatile ("rdtsc" : "=a" (low), "=d" (high));
   return ((uint64_t)high << 32) | low;
}

/*
 * Measure TSC frequency against PIT channel 2: count TSC ticks until
 * the channel, started in mode 0 (interrupt on terminal count), sets
 * its output.
 */
static uint64_t calibrate_tsc(void)
{
   uint8_t port_b = inb(PORT_B);

   // Enable the gate of channel 2, but not the speaker.
   outb(PORT_B, (port_b & ~PORT_B_SPEAKER) | PORT_B_GATE2);

   outb (PIT_BASE + 3, 0xb0); /* Channel2, interrupt on terminal count, Set LSB then MSB */
   outb (PIT_BASE + 2, CALIBRATION_PIT_TICKS & 0xff);
   outb (PIT_BASE + 2, (CALIBRATION_PIT_TICKS >> 8) & 0xff);

   uint64_t tsc_begin = read_tsc();
   while ((inb(PORT_B) & PORT_B_OUT2) == 0);
   uint64_t tsc_end = read_tsc();

   outb(PORT_B, port_b);

   return (tsc_end - tsc_begin) * OSCILLATOR_RATE / CALIBRATION_PIT_TICKS;
}

void pok_x86_qemu_timer_init(void)
{
   tsc_freq = calibrate_tsc();
   tsc_start = read_tsc();

   uint16_t pit_freq;

   pit_freq = POK_TIMER_FREQUENCY;

   outb (PIT_BASE + 3, 0x34); /* Channel0, rate generator, Set LSB then MSB */
   outb (PIT_BASE, (OSCILLATOR_RATE / pit_freq) & 0xff);
   outb (PIT_BASE, ((OSCILLATOR_RATE / pit_freq) >> 8) & 0xff);

   pok_pic_unmask (PIT_IRQ);
}

pok_time_t ja_system_time(void)
{
   if (tsc_freq == 0) return 0; // Timer is not initialized yet.

   uint64_t ticks = read_tsc() - tsc_start;

   // Split to avoid overflow: (ticks % freq) * 1e9 < freq * 1e9.
   return (pok_time_t)((ticks / tsc_freq) * 1000000000ULL
      + (ticks % tsc_freq) * 1000000000ULL / tsc_freq);
}

time_t ja_calendar_time(void)
{
   return base_calendar_time + (time_t)(ja_system_time() / 1000000000);
}
