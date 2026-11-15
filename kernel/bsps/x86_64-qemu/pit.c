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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the
 * file and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-11: the Doxygen comments that don't
 * document a function definition are plain comments, so that only the function
 * headers are in the function documentation.
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros are moved to
 * boards/x86_64-qemu/pit.h.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/x86_64-qemu/pit.c (the board support packages are in kernel/bsps).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * The timer of the x86_64 QEMU board: the PIT (8254) interrupts the kernel
 * at POK_TIMER_FREQUENCY, and the system time is read from the TSC,
 * calibrated against the PIT at boot.
 */


#include <errno.h>
#include <core/time.h>
#include <core/sched.h>
#include <ioports.h>

#include "pic.h"

#include "pit.h"

#include <bsp/bsp.h>

#include <asp/entries.h>

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

/**
 * @funcname `ja_bsp_process_timer`
 *
 * @description
 * The handler of the PIT interrupt (IRQ 0): acknowledges it and lets the
 * kernel count the tick.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in] The interrupted context (not used). |
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
 * 1. Mark `frame` as not used.
 * 2. Call `pok_pic_eoi()` with the argument `PIT_IRQ`.
 * 3. Call `jet_on_tick()`.
 *
 * @calls
 * - `pok_pic_eoi()`
 * - `jet_on_tick()`
 */
void ja_bsp_process_timer(interrupt_frame* frame)
{
   (void) frame;
   pok_pic_eoi (PIT_IRQ);

   jet_on_tick();
}

/**
 * @funcname `read_tsc`
 *
 * @description
 * Reads the time stamp counter of the CPU.
 *
 * @return The TSC value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `low` | `uint32_t` | The low 32 bits of the TSC (EAX) |
 * | `high` | `uint32_t` | The high 32 bits of the TSC (EDX) |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `rdtsc`.
 * 2. Return `((uint64_t)high << 32) | low`.
 *
 * @calls
 * None.
 */
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

/**
 * @funcname `calibrate_tsc`
 *
 * @description
 * Measures the TSC frequency against PIT channel 2: counts the TSC ticks
 * until the channel, started in mode 0 (interrupt on terminal count) for
 * 50 ms, sets its output.
 *
 * @return The TSC frequency, in ticks per second.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port_b` | `uint8_t` | The value of port 0x61 before the measure |
 * | `tsc_begin` | `uint64_t` | The TSC when channel 2 starts |
 * | `tsc_end` | `uint64_t` | The TSC when channel 2 sets its output |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `port_b` to `inb(PORT_B)`.
 * 2. Execute the assembly code
 *    `outb(PORT_B, (port_b & ~PORT_B_SPEAKER) | PORT_B_GATE2)`.
 * 3. Execute the assembly code `outb (PIT_BASE + 3, 0xb0)`.
 * 4. Execute the assembly code
 *    `outb (PIT_BASE + 2, CALIBRATION_PIT_TICKS & 0xffU)`.
 * 5. Execute the assembly code
 *    `outb (PIT_BASE + 2, (CALIBRATION_PIT_TICKS >> 8) & 0xffU)`.
 * 6. Call `read_tsc()`, and store the result in `tsc_begin`.
 * 7. While `(inb(PORT_B) & PORT_B_OUT2)` is equal to `0`:
 *    1. Do nothing.
 * 8. Call `read_tsc()`, and store the result in `tsc_end`.
 * 9. Execute the assembly code `outb(PORT_B, port_b)`.
 * 10. Return
 *     `((tsc_end - tsc_begin) * OSCILLATOR_RATE) / CALIBRATION_PIT_TICKS`.
 *
 * @calls
 * - `read_tsc()`
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

/**
 * @funcname `pok_x86_qemu_timer_init`
 *
 * @description
 * Initializes the system time and the timer interrupt: calibrates the TSC,
 * takes the boot time from it and starts PIT channel 0 at
 * POK_TIMER_FREQUENCY. Called by ja_bsp_init().
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tsc_freq` | `uint64_t` | The TSC frequency, in ticks per second |
 * | `tsc_start` | `uint64_t` | The TSC value at boot |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pit_freq` | `uint16_t` | The timer interrupt frequency |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `calibrate_tsc()`, and store the result in `tsc_freq`.
 * 2. Call `read_tsc()`, and store the result in `tsc_start`.
 * 3. Set `pit_freq` to `POK_TIMER_FREQUENCY`.
 * 4. Execute the assembly code `outb (PIT_BASE + 3, 0x34)`.
 * 5. Execute the assembly code
 *    `outb (PIT_BASE, (OSCILLATOR_RATE / pit_freq) & 0xffU)`.
 * 6. Execute the assembly code
 *    `outb (PIT_BASE, ((OSCILLATOR_RATE / pit_freq) >> 8) & 0xffU)`.
 * 7. Call `pok_pic_unmask()` with the argument `PIT_IRQ`.
 *
 * @calls
 * - `calibrate_tsc()`
 * - `read_tsc()`
 * - `pok_pic_unmask()`
 */
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

/**
 * @funcname `ja_system_time`
 *
 * @description
 * Returns the system time: the time since boot, from the TSC. While the GDB
 * server holds the system, the time stays where it stopped.
 *
 * @return The time since boot, in nanoseconds; 0 before the timer is
 *         initialized.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `time_stopped` | `pok_bool_t` | TRUE while the GDB server holds the system |
 * | `tsc_freq` | `uint64_t` | The TSC frequency, in ticks per second |
 * | `tsc_start` | `uint64_t` | The TSC value at boot (moved by stopped time) |
 * | `tsc_stopped_at` | `uint64_t` | The TSC value when the system stopped |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `now` | `uint64_t` | The current TSC value |
 * | `ticks` | `uint64_t` | The TSC ticks since boot |
 * | `ns` | `uint64_t` | The time since boot, in nanoseconds |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `tsc_freq` is equal to `0U`:
 *    1. Return `0`.
 * 2. Call `read_tsc()`, and store the result in `now`.
 * 3. If `time_stopped` is true:
 *    1. Set `now` to `tsc_stopped_at`.
 * 4. Set `ticks` to `now - tsc_start`.
 * 5. Set `ns` to
 *    `((ticks / tsc_freq) * 1000000000ULL) + (((ticks % tsc_freq) *`
 *    `1000000000ULL) / tsc_freq)`.
 * 6. Return `(pok_time_t) ns`.
 *
 * @calls
 * - `read_tsc()`
 */
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

/**
 * @funcname `ja_calendar_time`
 *
 * @description
 * Returns the calendar time: a fixed calendar time for the boot (28 November
 * 2016) plus the system time.
 *
 * @return The calendar time, in seconds since 1970.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base_calendar_time` | `static time_t` | The calendar time at boot |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `base_calendar_time + (time_t)(ja_system_time() / 1000000000)`.
 *
 * @calls
 * - `ja_system_time()`
 */
time_t ja_calendar_time(void)
{
   static time_t base_calendar_time = 1480330081; // On 28.11.2016

   return base_calendar_time + (time_t)(ja_system_time() / 1000000000);
}

#ifdef POK_NEEDS_GDB
/**
 * @funcname `ja_system_time_stop`
 *
 * @description
 * Stops the system time while the GDB server holds the system.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `time_stopped` | `pok_bool_t` | TRUE while the GDB server holds the system |
 * | `tsc_freq` | `uint64_t` | The TSC frequency (0 before calibration) |
 * | `tsc_stopped_at` | `uint64_t` | The TSC value when the system stopped |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `tsc_freq` is not equal to `0U` and `time_stopped` is false:
 *    1. Call `read_tsc()`, and store the result in `tsc_stopped_at`.
 *    2. Set `time_stopped` to `TRUE`.
 *
 * @calls
 * - `read_tsc()`
 */
void ja_system_time_stop(void)
{
   if ((tsc_freq != 0U) && !time_stopped)
   {
      tsc_stopped_at = read_tsc();
      time_stopped = TRUE;
   }
}

/**
 * @funcname `ja_system_time_resume`
 *
 * @description
 * Resumes the system time when the GDB server lets the system go on: the
 * time spent stopped doesn't count.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `time_stopped` | `pok_bool_t` | TRUE while the GDB server holds the system |
 * | `tsc_start` | `uint64_t` | The TSC value at boot (moved by stopped time) |
 * | `tsc_stopped_at` | `uint64_t` | The TSC value when the system stopped |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `time_stopped` is true:
 *    1. Add `read_tsc() - tsc_stopped_at` to `tsc_start`.
 *    2. Set `time_stopped` to `FALSE`.
 *
 * @calls
 * - `read_tsc()`
 */
void ja_system_time_resume(void)
{
   if (time_stopped)
   {
      tsc_start += read_tsc() - tsc_stopped_at;
      time_stopped = FALSE;
   }
}
#endif /* POK_NEEDS_GDB */
