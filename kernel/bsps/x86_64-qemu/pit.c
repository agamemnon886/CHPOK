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
 * boards/x86_64-qemu/pit.c (the board support packages are in kernel/bsps);
 * the function headers follow outb() and inb(), which call x86_outb() and
 * x86_inb(); the value read from port B is compared with 0U.
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 * Modified by Kirill Brazhnikov, 2026-11-17: the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
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
 * - \hlrid{HLR_PIT_JA_BSP_PROCESS_TIMER_01}: `ja_bsp_process_timer()` shall
 *   acknowledge the interrupt of the timer and let the kernel count the tick.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *   - \srqref{SRQ_JET_INT_005}
 *
 * @llr
 * - \llrid{LLR_PIT_JA_BSP_PROCESS_TIMER_01}: `ja_bsp_process_timer()` shall
 *   call `pok_pic_eoi()` once with `PIT_IRQ` and then `jet_on_tick()` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_JA_BSP_PROCESS_TIMER_01}
 *
 * @design
 * 1. Call `pok_pic_eoi()` with the argument `PIT_IRQ`.
 *    \llrref{LLR_PIT_JA_BSP_PROCESS_TIMER_01}
 * 2. Call `jet_on_tick()`. \llrref{LLR_PIT_JA_BSP_PROCESS_TIMER_01}
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
 * - \hlrid{HLR_PIT_READ_TSC_01}: `read_tsc()` shall give the time stamp counter
 *   of the processor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_001}
 *
 * @llr
 * - \llrid{LLR_PIT_READ_TSC_01}: `read_tsc()` shall execute the instruction
 *   `rdtsc` once and return the value it gives in EDX as the high 32 bits and
 *   the value it gives in EAX as the low 32 bits.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_READ_TSC_01}
 *
 * @design
 * 1. Execute the assembly code `rdtsc`. \llrref{LLR_PIT_READ_TSC_01}
 * 2. Return `((uint64_t)high << 32) | low`. \llrref{LLR_PIT_READ_TSC_01}
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
 * - \hlrid{HLR_PIT_CALIBRATE_TSC_01}: `calibrate_tsc()` shall measure the
 *   frequency of the time stamp counter against channel 2 of the 8254 timer,
 *   over 50 ms.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_002}
 *   - \srqref{SRQ_JET_TIME_001}
 *
 * @llr
 * - \llrid{LLR_PIT_CALIBRATE_TSC_01}: `calibrate_tsc()` shall read `PORT_B`
 *   with `inb()`, write the byte read to `PORT_B` with `PORT_B_SPEAKER` cleared
 *   and `PORT_B_GATE2` set, write `0xb0` to `PIT_BASE + 3` and the low and then
 *   the high byte of `CALIBRATION_PIT_TICKS` to `PIT_BASE + 2` (`outb()`), call
 *   `read_tsc()`, read `PORT_B` until `PORT_B_OUT2` is set in the byte read,
 *   call `read_tsc()` again, and write the first byte read back to `PORT_B`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_CALIBRATE_TSC_01}
 * - \llrid{LLR_PIT_CALIBRATE_TSC_02}: `calibrate_tsc()` shall return the second
 *   result of `read_tsc()` minus the first, times `OSCILLATOR_RATE`, divided by
 *   `CALIBRATION_PIT_TICKS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_CALIBRATE_TSC_01}
 *
 * @design
 * 1. Set `port_b` to `inb(PORT_B)`. \llrref{LLR_PIT_CALIBRATE_TSC_01}
 * 2. Use the macro
 *    `outb(PORT_B, (port_b & ~PORT_B_SPEAKER) | PORT_B_GATE2)`.
 *    \llrref{LLR_PIT_CALIBRATE_TSC_01}
 * 3. Use the macro `outb (PIT_BASE + 3, 0xb0)`.
 *    \llrref{LLR_PIT_CALIBRATE_TSC_01}
 * 4. Use the macro
 *    `outb (PIT_BASE + 2, CALIBRATION_PIT_TICKS & 0xffU)`.
 *    \llrref{LLR_PIT_CALIBRATE_TSC_01}
 * 5. Use the macro
 *    `outb (PIT_BASE + 2, (CALIBRATION_PIT_TICKS >> 8) & 0xffU)`.
 *    \llrref{LLR_PIT_CALIBRATE_TSC_01}
 * 6. Call `read_tsc()`, and store the result in `tsc_begin`.
 *    \llrref{LLR_PIT_CALIBRATE_TSC_01}, \llrref{LLR_PIT_CALIBRATE_TSC_02}
 * 7. While `(inb(PORT_B) & PORT_B_OUT2)` is equal to `0U`:
 *    \llrref{LLR_PIT_CALIBRATE_TSC_01}
 * 8. Call `read_tsc()`, and store the result in `tsc_end`.
 *    \llrref{LLR_PIT_CALIBRATE_TSC_01}, \llrref{LLR_PIT_CALIBRATE_TSC_02}
 * 9. Use the macro `outb(PORT_B, port_b)`. \llrref{LLR_PIT_CALIBRATE_TSC_01}
 * 10. Return
 *     `((tsc_end - tsc_begin) * OSCILLATOR_RATE) / CALIBRATION_PIT_TICKS`.
 *     \llrref{LLR_PIT_CALIBRATE_TSC_02}
 *
 * @calls
 * - `x86_inb()`
 * - `x86_outb()`
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
   while ((inb(PORT_B) & PORT_B_OUT2) == 0U) {
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
 * - \hlrid{HLR_PIT_POK_X86_QEMU_TIMER_INIT_01}: `pok_x86_qemu_timer_init()`
 *   shall start the system time at boot and the periodic timer at the frequency
 *   of the build.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *   - \srqref{SRQ_JET_BSP_002}
 *
 * @llr
 * - \llrid{LLR_PIT_POK_X86_QEMU_TIMER_INIT_01}: `pok_x86_qemu_timer_init()`
 *   shall set `tsc_freq` to the result of `calibrate_tsc()` and then
 *   `tsc_start` to the result of `read_tsc()`, write with `outb()` `0x34` to
 *   `PIT_BASE + 3` and the low and then the high byte of `OSCILLATOR_RATE /
 *   POK_TIMER_FREQUENCY` to `PIT_BASE`, and call `pok_pic_unmask()` once with
 *   `PIT_IRQ`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_POK_X86_QEMU_TIMER_INIT_01}
 *
 * @design
 * 1. Call `calibrate_tsc()`, and store the result in `tsc_freq`.
 *    \llrref{LLR_PIT_POK_X86_QEMU_TIMER_INIT_01}
 * 2. Call `read_tsc()`, and store the result in `tsc_start`.
 *    \llrref{LLR_PIT_POK_X86_QEMU_TIMER_INIT_01}
 * 3. Set `pit_freq` to `POK_TIMER_FREQUENCY`.
 *    \llrref{LLR_PIT_POK_X86_QEMU_TIMER_INIT_01}
 * 4. Use the macro `outb (PIT_BASE + 3, 0x34)`.
 *    \llrref{LLR_PIT_POK_X86_QEMU_TIMER_INIT_01}
 * 5. Use the macro
 *    `outb (PIT_BASE, (OSCILLATOR_RATE / pit_freq) & 0xffU)`.
 *    \llrref{LLR_PIT_POK_X86_QEMU_TIMER_INIT_01}
 * 6. Use the macro
 *    `outb (PIT_BASE, ((OSCILLATOR_RATE / pit_freq) >> 8) & 0xffU)`.
 *    \llrref{LLR_PIT_POK_X86_QEMU_TIMER_INIT_01}
 * 7. Call `pok_pic_unmask()` with the argument `PIT_IRQ`.
 *    \llrref{LLR_PIT_POK_X86_QEMU_TIMER_INIT_01}
 *
 * @calls
 * - `calibrate_tsc()`
 * - `read_tsc()`
 * - `x86_outb()`
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
 * - \hlrid{HLR_PIT_JA_SYSTEM_TIME_01}: `ja_system_time()` shall give the system
 *   time in nanoseconds since the start, from the time stamp counter, 0 before
 *   the timer is started.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_001}
 * - \hlrid{HLR_PIT_JA_SYSTEM_TIME_02}: `ja_system_time()` shall keep the system
 *   time where it stopped while the debugger holds the module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_013}
 *
 * @llr
 * - \llrid{LLR_PIT_JA_SYSTEM_TIME_01}: If `tsc_freq` is 0, `ja_system_time()`
 *   shall return 0 without calling `read_tsc()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_JA_SYSTEM_TIME_01}
 * - \llrid{LLR_PIT_JA_SYSTEM_TIME_02}: If `tsc_freq` is not 0 and
 *   `time_stopped` is false, `ja_system_time()` shall return the nanoseconds of
 *   the result of `read_tsc()` minus `tsc_start`: these ticks divided by
 *   `tsc_freq`, times 1000000000, plus the remainder of that division times
 *   1000000000 divided by `tsc_freq`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_JA_SYSTEM_TIME_01}
 * - \llrid{LLR_PIT_JA_SYSTEM_TIME_03}: If `tsc_freq` is not 0 and
 *   `time_stopped` is true, `ja_system_time()` shall return the nanoseconds,
 *   computed in the same way, of `tsc_stopped_at` minus `tsc_start`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_JA_SYSTEM_TIME_02}
 *
 * @design
 * 1. If `tsc_freq` is equal to `0U`: \llrref{LLR_PIT_JA_SYSTEM_TIME_01}
 *    1. Return `0`.
 * 2. Call `read_tsc()`, and store the result in `now`.
 *    \llrref{LLR_PIT_JA_SYSTEM_TIME_02}, \llrref{LLR_PIT_JA_SYSTEM_TIME_03}
 * 3. If `time_stopped` is true: \llrref{LLR_PIT_JA_SYSTEM_TIME_02},
 *    \llrref{LLR_PIT_JA_SYSTEM_TIME_03}
 *    1. Set `now` to `tsc_stopped_at`. \llrref{LLR_PIT_JA_SYSTEM_TIME_03}
 * 4. Set `ticks` to `now - tsc_start`. \llrref{LLR_PIT_JA_SYSTEM_TIME_02},
 *    \llrref{LLR_PIT_JA_SYSTEM_TIME_03}
 * 5. Set `ns` to
 *    `((ticks / tsc_freq) * 1000000000ULL) + (((ticks % tsc_freq) *`
 *    `1000000000ULL) / tsc_freq)`. \llrref{LLR_PIT_JA_SYSTEM_TIME_02},
 *    \llrref{LLR_PIT_JA_SYSTEM_TIME_03}
 * 6. Return `(pok_time_t) ns`. \llrref{LLR_PIT_JA_SYSTEM_TIME_02},
 *    \llrref{LLR_PIT_JA_SYSTEM_TIME_03}
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
 * - \hlrid{HLR_PIT_JA_CALENDAR_TIME_01}: `ja_calendar_time()` shall give the
 *   calendar time: a fixed date (28 November 2016) plus the system time, in
 *   seconds.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_003}
 *   - \srqref{SRQ_JET_TIME_006}
 *
 * @llr
 * - \llrid{LLR_PIT_JA_CALENDAR_TIME_01}: `ja_calendar_time()` shall return
 *   `base_calendar_time` (1480330081) plus the result of `ja_system_time()`
 *   divided by 1000000000.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_JA_CALENDAR_TIME_01}
 *
 * @design
 * 1. Return `base_calendar_time + (time_t)(ja_system_time() / 1000000000)`.
 *    \llrref{LLR_PIT_JA_CALENDAR_TIME_01}
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
 * - \hlrid{HLR_PIT_JA_SYSTEM_TIME_STOP_01}: `ja_system_time_stop()` shall stop
 *   the system time while the debugger holds the module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_013}
 *
 * @llr
 * - \llrid{LLR_PIT_JA_SYSTEM_TIME_STOP_01}: If and only if `tsc_freq` is not 0
 *   and `time_stopped` is false, `ja_system_time_stop()` shall set
 *   `tsc_stopped_at` to the result of `read_tsc()` and `time_stopped` to
 *   `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_JA_SYSTEM_TIME_STOP_01}
 *
 * @design
 * 1. If `tsc_freq` is not equal to `0U` and `time_stopped` is false:
 *    \llrref{LLR_PIT_JA_SYSTEM_TIME_STOP_01}
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
 * - \hlrid{HLR_PIT_JA_SYSTEM_TIME_RESUME_01}: `ja_system_time_resume()` shall
 *   resume the system time when the debugger lets the module go on, without
 *   counting the time stopped.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_013}
 *
 * @llr
 * - \llrid{LLR_PIT_JA_SYSTEM_TIME_RESUME_01}: If and only if `time_stopped` is
 *   true, `ja_system_time_resume()` shall add the result of `read_tsc()` minus
 *   `tsc_stopped_at` to `tsc_start` and set `time_stopped` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PIT_JA_SYSTEM_TIME_RESUME_01}
 *
 * @design
 * 1. If `time_stopped` is true: \llrref{LLR_PIT_JA_SYSTEM_TIME_RESUME_01}
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
