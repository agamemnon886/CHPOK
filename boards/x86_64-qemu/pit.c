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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; the system time
 * is computed in an unsigned variable before it is converted; deviations
 * are documented where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: one declarator
 * per declaration; base_calendar_time is defined in the function that
 * uses it; operands of mixed precedence are parenthesized; deviations are
 * documented where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-04: ja_system_time_stop() and
 * ja_system_time_resume() stop the system time while the GDB server holds
 * the system.
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

#define OSCILLATOR_RATE 1193180U /** The oscillation rate of x86 clock */
#define PIT_BASE 0x40

/* Port 0x61: PIT channel 2 gate (bit 0), speaker (bit 1), channel 2 output (bit 5). */
#define PORT_B 0x61
#define PORT_B_GATE2 0x01
#define PORT_B_SPEAKER 0x02
#define PORT_B_OUT2 0x20U

/*
 * Duration of TSC calibration (in PIT ticks, at most 0xffff): 50 ms.
 */
#define CALIBRATION_PIT_TICKS (OSCILLATOR_RATE / 20U)

/*
 * System time is read from the TSC, not counted in timer ticks: ticks
 * may be lost (e.g. QEMU on macOS delivers less than 1000 timer
 * interrupts per second), which would slow down the system time.
 */
static uint64_t tsc_freq;   /* TSC ticks per second */
static uint64_t tsc_start;  /* TSC value at boot */

#ifdef POK_NEEDS_GDB
/*
 * The system time is stopped while the GDB server holds the system: it
 * stays at the TSC value 'tsc_stopped_at', and the time spent stopped is
 * added to 'tsc_start' when the system resumes.
 */
static pok_bool_t time_stopped = FALSE;
static uint64_t tsc_stopped_at;
#endif /* POK_NEEDS_GDB */

/*
 * Hardcoded calendar time at the beginning of the OS loading.
 * 
 * Could be obtained on https://www.timeanddate.com/.
 */


void ja_bsp_process_timer(interrupt_frame* frame)
{
   (void) frame;
   pok_pic_eoi (PIT_IRQ);

   jet_on_tick();
}

/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (read_tsc()): privileged
 * instructions are written as GNU inline assembly, each isolated in a small
 * function.
 */
static uint64_t read_tsc(void)
{
   uint32_t low;
   uint32_t high;
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
   outb (PIT_BASE + 2, CALIBRATION_PIT_TICKS & 0xffU);
   outb (PIT_BASE + 2, (CALIBRATION_PIT_TICKS >> 8) & 0xffU);

   uint64_t tsc_begin = read_tsc();
   while ((inb(PORT_B) & PORT_B_OUT2) == 0) {
      ;
   }
   uint64_t tsc_end = read_tsc();

   outb(PORT_B, port_b);

   return ((tsc_end - tsc_begin) * OSCILLATOR_RATE) / CALIBRATION_PIT_TICKS;
}

void pok_x86_qemu_timer_init(void)
{
   tsc_freq = calibrate_tsc();
   tsc_start = read_tsc();

   uint16_t pit_freq;

   pit_freq = POK_TIMER_FREQUENCY;

   outb (PIT_BASE + 3, 0x34); /* Channel0, rate generator, Set LSB then MSB */
   outb (PIT_BASE, (OSCILLATOR_RATE / pit_freq) & 0xffU);
   outb (PIT_BASE, ((OSCILLATOR_RATE / pit_freq) >> 8) & 0xffU);

   (void) pok_pic_unmask (PIT_IRQ);
}

pok_time_t ja_system_time(void)
{
   if (tsc_freq == 0U) {
      return 0; // Timer is not initialized yet.
   }

   uint64_t now = read_tsc();
#ifdef POK_NEEDS_GDB
   if (time_stopped) {
      now = tsc_stopped_at;
   }
#endif /* POK_NEEDS_GDB */
   uint64_t ticks = now - tsc_start;

   // Split to avoid overflow: (ticks % freq) * 1e9 < freq * 1e9.
   uint64_t ns = ((ticks / tsc_freq) * 1000000000ULL)
      + (((ticks % tsc_freq) * 1000000000ULL) / tsc_freq);

   return (pok_time_t) ns;
}

time_t ja_calendar_time(void)
{
   static time_t base_calendar_time = 1480330081; // On 28.11.2016

   return base_calendar_time + (time_t)(ja_system_time() / 1000000000);
}

#ifdef POK_NEEDS_GDB
void ja_system_time_stop(void)
{
   if ((tsc_freq != 0U) && !time_stopped)
   {
      tsc_stopped_at = read_tsc();
      time_stopped = TRUE;
   }
}

void ja_system_time_resume(void)
{
   if (time_stopped)
   {
      tsc_start += read_tsc() - tsc_stopped_at;
      time_stopped = FALSE;
   }
}
#endif /* POK_NEEDS_GDB */
