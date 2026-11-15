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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; internal
 * linkage for file-local definitions; system registers and PSCI calls are
 * reached through small inline-assembly functions; the system time is
 * computed in an unsigned variable before it is converted; deviations are
 * documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-05: the system time stops while
 * the GDB server holds the system (ja_system_time_stop() and
 * ja_system_time_resume()).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: the timer interrupt sets the
 * next compare value before the end of interrupt, and is ignored if the
 * timer condition doesn't hold, so that under the hypervisor (where the
 * guest's timer interrupt is mapped to the physical one) a tick is never
 * counted twice. Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros and the declaration of
 * ja_boot_from_el2 are moved to boards/arm64-qemu/include/board/bsp.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/arm64-qemu/bsp.c (the board support packages are in kernel/bsps).
 */

/**
 * @file
 * Board support for QEMU 'virt' (arm64): the GICv2 interrupt controller, the
 * EL1 virtual timer as the system tick, the PL031 RTC for the calendar time
 * and PSCI for reset.
 */

#include <libc.h>
#include <core/time.h>
#include <core/debug.h>
#include <asp/entries.h>
#include <asp/time.h>
#include <arch/ioports.h>
#include <bsp/bsp.h>

static uint64_t timer_freq;
static uint64_t timer_period;   /* in counter ticks */
static uint64_t timer_start;    /* counter value at boot */
static uint64_t timer_next;     /* counter value for the next tick */

static time_t base_calendar_time;

#ifdef POK_NEEDS_GDB
/*
 * The system time is stopped while the GDB server holds the system: it
 * stays at the counter value 'counter_stopped_at', and the time spent
 * stopped is added to 'timer_start' (and to the next tick) when the system
 * resumes.
 */
static pok_bool_t time_stopped = FALSE;
static uint64_t counter_stopped_at;
#endif /* POK_NEEDS_GDB */

/**
 * @funcname `read_cntvct`
 *
 * @description
 * Reads the virtual counter (CNTVCT_EL0), after an instruction barrier so
 * that the read isn't done early.
 *
 * @return The counter value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `val` | `uint64_t` | The value read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `isb; mrs %0, cntvct_el0`.
 * 2. Return `val`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (the functions below): system
 * registers and privileged instructions are reached with GNU inline assembly,
 * each isolated in a small function.
 */
static uint64_t read_cntvct(void)
{
    uint64_t val;
    asm volatile("isb\n mrs %0, cntvct_el0" : "=r" (val) :: "memory");
    return val;
}

/**
 * @funcname `read_cntfrq`
 *
 * @description
 * Reads the frequency of the system counter (CNTFRQ_EL0).
 *
 * @return The counter frequency, in ticks per second.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `val` | `uint64_t` | The value read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `mrs %0, cntfrq_el0`.
 * 2. Return `val`.
 *
 * @calls
 * None.
 */
static uint64_t read_cntfrq(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (val));
    return val;
}

/**
 * @funcname `write_cntv_cval`
 *
 * @description
 * Sets the compare value of the virtual timer (CNTV_CVAL_EL0): the timer
 * condition holds when the virtual counter reaches it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `val` | `uint64_t` | [in] The counter value of the next timer interrupt. |
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
 * 1. Execute the assembly code `msr cntv_cval_el0, %0`.
 *
 * @calls
 * None.
 */
static void write_cntv_cval(uint64_t val)
{
    asm volatile("msr cntv_cval_el0, %0" :: "r" (val));
}

/**
 * @funcname `write_cntv_ctl`
 *
 * @description
 * Writes the control register of the virtual timer (CNTV_CTL_EL0).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `val` | `uint64_t` | [in] The new value (ENABLE, IMASK bits). |
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
 * 1. Execute the assembly code `msr cntv_ctl_el0, %0`.
 *
 * @calls
 * None.
 */
static void write_cntv_ctl(uint64_t val)
{
    asm volatile("msr cntv_ctl_el0, %0" :: "r" (val));
}

/**
 * @funcname `read_cntv_ctl`
 *
 * @description
 * Reads the control register of the virtual timer (CNTV_CTL_EL0), whose
 * ISTATUS bit tells whether the timer condition holds.
 *
 * @return The register value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `val` | `uint64_t` | The value read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `mrs %0, cntv_ctl_el0`.
 * 2. Return `val`.
 *
 * @calls
 * None.
 */
static uint64_t read_cntv_ctl(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntv_ctl_el0" : "=r" (val));
    return val;
}

/**
 * @funcname `wait_for_interrupt`
 *
 * @description
 * Waits for an interrupt (wfi), with the CPU idle.
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
 * 1. Execute the assembly code `wfi`.
 *
 * @calls
 * None.
 */
static void wait_for_interrupt(void)
{
    asm volatile("wfi");
}

/**
 * @funcname `psci_call_smc`
 *
 * @description
 * Calls a PSCI function through the secure monitor (smc). Used when the
 * kernel was entered at EL2.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fn` | `uint64_t` | [in] The PSCI function ID. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x0` | `uint64_t` | Register x0: the function ID, then its result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `x0` to `fn`.
 * 2. Execute the assembly code `smc #0`.
 *
 * @calls
 * None.
 */
static void psci_call_smc(uint64_t fn)
{
    register uint64_t x0 asm("x0") = fn;
    asm volatile("smc #0" : "+r" (x0) :: "memory");
}

/**
 * @funcname `psci_call_hvc`
 *
 * @description
 * Calls a PSCI function through the hypervisor (hvc). Used when the kernel
 * was entered at EL1, under QEMU's firmware or a hypervisor.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fn` | `uint64_t` | [in] The PSCI function ID. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x0` | `uint64_t` | Register x0: the function ID, then its result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `x0` to `fn`.
 * 2. Execute the assembly code `hvc #0`.
 *
 * @calls
 * None.
 */
static void psci_call_hvc(uint64_t fn)
{
    register uint64_t x0 asm("x0") = fn;
    asm volatile("hvc #0" : "+r" (x0) :: "memory");
}

/**
 * @funcname `gic_init`
 *
 * @description
 * Initializes the GICv2: enables the virtual timer interrupt (PPI 27) in
 * the distributor and lets every priority through the CPU interface.
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
 * 1. Call `ja_mmio_write32()` with the arguments `BOARD_GICD_BASE + GICD_CTLR`
 *    and `0`.
 * 2. Call `ja_mmio_write8()` with the arguments
 *    `BOARD_GICD_BASE + GICD_IPRIORITYR(BOARD_IRQ_VTIMER)` and `0xa0U`.
 * 3. Call `ja_mmio_write32()` with the arguments
 *    `BOARD_GICD_BASE + GICD_ISENABLER(BOARD_IRQ_VTIMER / 32U)` and
 *    `(uint32_t) 1U << (BOARD_IRQ_VTIMER % 32U)`.
 * 4. Call `ja_mmio_write32()` with the arguments `BOARD_GICD_BASE + GICD_CTLR`
 *    and `1`.
 * 5. Call `ja_mmio_write32()` with the arguments `BOARD_GICC_BASE + GICC_PMR`
 *    and `0xff`.
 * 6. Call `ja_mmio_write32()` with the arguments `BOARD_GICC_BASE + GICC_CTLR`
 *    and `1`.
 *
 * @calls
 * - `ja_mmio_write32()`
 * - `ja_mmio_write8()`
 */
static void gic_init(void)
{
    ja_mmio_write32(BOARD_GICD_BASE + GICD_CTLR, 0);
    ja_mmio_write8(BOARD_GICD_BASE + GICD_IPRIORITYR(BOARD_IRQ_VTIMER), 0xa0U);
    ja_mmio_write32(BOARD_GICD_BASE + GICD_ISENABLER(BOARD_IRQ_VTIMER / 32U),
        (uint32_t) 1U << (BOARD_IRQ_VTIMER % 32U));
    ja_mmio_write32(BOARD_GICD_BASE + GICD_CTLR, 1);

    ja_mmio_write32(BOARD_GICC_BASE + GICC_PMR, 0xff);
    ja_mmio_write32(BOARD_GICC_BASE + GICC_CTLR, 1);
}

/**
 * @funcname `timer_init`
 *
 * @description
 * Starts the virtual timer at POK_TIMER_FREQUENCY and takes the boot time
 * from the virtual counter.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timer_freq` | `uint64_t` | The counter frequency, in ticks per second |
 * | `timer_period` | `uint64_t` | The counter ticks between two timer ticks |
 * | `timer_start` | `uint64_t` | The counter value at boot |
 * | `timer_next` | `uint64_t` | The counter value of the next timer tick |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `read_cntfrq()`, and store the result in `timer_freq`.
 * 2. Set `timer_period` to `timer_freq / POK_TIMER_FREQUENCY`.
 * 3. Call `read_cntvct()`, and store the result in `timer_start`.
 * 4. Set `timer_next` to `timer_start + timer_period`.
 * 5. Call `write_cntv_cval()` with the argument `timer_next`.
 * 6. Call `write_cntv_ctl()` with the argument `CNTV_CTL_ENABLE`.
 *
 * @calls
 * - `read_cntfrq()`
 * - `read_cntvct()`
 * - `write_cntv_cval()`
 * - `write_cntv_ctl()`
 */
static void timer_init(void)
{
    timer_freq = read_cntfrq();
    timer_period = timer_freq / POK_TIMER_FREQUENCY;

    timer_start = read_cntvct();
    timer_next = timer_start + timer_period;

    write_cntv_cval(timer_next);
    write_cntv_ctl(CNTV_CTL_ENABLE);
}

/**
 * @funcname `ja_bsp_init`
 *
 * @description
 * Initializes the board when the kernel starts: the consoles, the calendar
 * time from the RTC, the interrupt controller and the timer.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base_calendar_time` | `time_t` | The calendar time at boot (PL031) |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_console_init_all()`.
 * 2. Set `base_calendar_time` to
 *    `(time_t)ja_mmio_read32(BOARD_RTC_BASE + RTC_DR)`.
 * 3. Call `gic_init()`.
 * 4. Call `timer_init()`.
 *
 * @calls
 * - `jet_console_init_all()`
 * - `ja_mmio_read32()`
 * - `gic_init()`
 * - `timer_init()`
 */
void ja_bsp_init (void)
{
    jet_console_init_all();

    base_calendar_time = (time_t)ja_mmio_read32(BOARD_RTC_BASE + RTC_DR);

    gic_init();
    timer_init();
}

/**
 * @funcname `set_next_tick`
 *
 * @description
 * Sets the compare value of the next tick. This lowers the timer's
 * interrupt line, which must be done before the end of interrupt: under a
 * hypervisor the end of interrupt also ends the physical interrupt, which
 * would come again at once if the line were still high.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `timer_next` | `uint64_t` | The counter value of the next timer tick |
 * | `timer_period` | `uint64_t` | The counter ticks between two timer ticks |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `now` | `uint64_t` | The current counter value |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `read_cntvct()`, and store the result in `now`.
 * 2. Add `timer_period` to `timer_next`.
 * 3. If `timer_next` is less than or equal to `now`:
 *    1. Set `timer_next` to `now + timer_period`.
 * 4. Call `write_cntv_cval()` with the argument `timer_next`.
 *
 * @calls
 * - `read_cntvct()`
 * - `write_cntv_cval()`
 */
static void set_next_tick(void)
{
    uint64_t now = read_cntvct();

    timer_next += timer_period;
    if (timer_next <= now)
    {
        // Ticks were missed (e.g. the host was busy): don't try to catch up.
        timer_next = now + timer_period;
    }
    write_cntv_cval(timer_next);
}

/**
 * @funcname `ja_bsp_process_irq`
 *
 * @description
 * The handler of the IRQ exception: acknowledges the interrupt at the GIC,
 * counts a tick for the virtual timer and reports any other interrupt.
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
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `iar` | `uint32_t` | The interrupt acknowledge register value |
 * | `irq` | `uint32_t` | The interrupt ID |
 * | `tick` | `pok_bool_t` | Whether the timer condition holds |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Mark `frame` as not used.
 * 2. Call `ja_mmio_read32()` with the argument `BOARD_GICC_BASE + GICC_IAR`,
 *    and store the result in `iar`.
 * 3. Set `irq` to `iar & 0x3ffU`.
 * 4. If `irq` is greater than or equal to `GIC_SPURIOUS`:
 *    1. Return.
 * 5. If `irq` is equal to `BOARD_IRQ_VTIMER`:
 *    1. Set `tick` to `(read_cntv_ctl() & CNTV_CTL_ISTATUS) != 0U`.
 *    2. If `tick` is true:
 *       1. Call `set_next_tick()`.
 *    3. Call `ja_mmio_write32()` with the arguments
 *       `BOARD_GICC_BASE + GICC_EOIR` and `iar`.
 *    4. If `tick` is true:
 *       1. Call `jet_on_tick()`.
 * 6. Otherwise:
 *    1. Call `ja_mmio_write32()` with the arguments
 *       `BOARD_GICC_BASE + GICC_EOIR` and `iar`.
 *    2. Call `printf()` with the arguments `"Unexpected IRQ %%u\n"` and
 *       `(unsigned)irq`.
 *
 * @calls
 * - `ja_mmio_read32()`
 * - `read_cntv_ctl()`
 * - `set_next_tick()`
 * - `ja_mmio_write32()`
 * - `jet_on_tick()`
 * - `printf()`
 */
void ja_bsp_process_irq(interrupt_frame* frame)
{
    (void) frame;

    uint32_t iar = ja_mmio_read32(BOARD_GICC_BASE + GICC_IAR);
    uint32_t irq = iar & 0x3ffU;

    if (irq >= GIC_SPURIOUS) {
        return;
    }

    if (irq == BOARD_IRQ_VTIMER) {
        /*
         * A tick only if the timer condition holds: an interrupt for a
         * compare value that was already moved on is spurious.
         */
        pok_bool_t tick = (read_cntv_ctl() & CNTV_CTL_ISTATUS) != 0U;

        if (tick) {
            set_next_tick();
        }
        // End of interrupt before jet_on_tick(), which may switch context.
        ja_mmio_write32(BOARD_GICC_BASE + GICC_EOIR, iar);
        if (tick) {
            jet_on_tick();
        }
    } else {
        ja_mmio_write32(BOARD_GICC_BASE + GICC_EOIR, iar);
        (void) printf("Unexpected IRQ %u\n", (unsigned)irq);
    }
}

/**
 * @funcname `ja_system_time`
 *
 * @description
 * Returns the system time: the time since boot, from the virtual counter.
 * While the GDB server holds the system, the time stays where it stopped.
 *
 * @return The time since boot, in nanoseconds.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `counter_stopped_at` | `uint64_t` | The counter when the system stopped |
 * | `time_stopped` | `pok_bool_t` | TRUE while the GDB server holds the system |
 * | `timer_freq` | `uint64_t` | The counter frequency, in ticks per second |
 * | `timer_start` | `uint64_t` | The counter at boot (moved by stopped time) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `now` | `uint64_t` | The current counter value |
 * | `ticks` | `uint64_t` | The counter ticks since boot |
 * | `ns` | `uint64_t` | The time since boot, in nanoseconds |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `read_cntvct()`, and store the result in `now`.
 * 2. If `time_stopped` is true:
 *    1. Set `now` to `counter_stopped_at`.
 * 3. Set `ticks` to `now - timer_start`.
 * 4. Set `ns` to
 *    `((ticks / timer_freq) * 1000000000ULL) + (((ticks % timer_freq) *`
 *    `1000000000ULL) / timer_freq)`.
 * 5. Return `(pok_time_t) ns`.
 *
 * @calls
 * - `read_cntvct()`
 */
pok_time_t ja_system_time(void)
{
    uint64_t now = read_cntvct();
#ifdef POK_NEEDS_GDB
    if (time_stopped) {
        now = counter_stopped_at;
    }
#endif /* POK_NEEDS_GDB */
    uint64_t ticks = now - timer_start;

    // Split to avoid overflow: (ticks % freq) * 1e9 < freq * 1e9.
    uint64_t ns = ((ticks / timer_freq) * 1000000000ULL)
        + (((ticks % timer_freq) * 1000000000ULL) / timer_freq);

    return (pok_time_t) ns;
}

/**
 * @funcname `ja_calendar_time`
 *
 * @description
 * Returns the calendar time: the RTC's time at boot plus the system time.
 *
 * @return The calendar time, in seconds since 1970.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `base_calendar_time` | `time_t` | The calendar time at boot (PL031) |
 *
 * @locals
 * None.
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
 * | `counter_stopped_at` | `uint64_t` | The counter when the system stopped |
 * | `time_stopped` | `pok_bool_t` | TRUE while the GDB server holds the system |
 * | `timer_freq` | `uint64_t` | The counter frequency (0 before the timer starts) |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `timer_freq` is not equal to `0U` and `time_stopped` is false:
 *    1. Call `read_cntvct()`, and store the result in `counter_stopped_at`.
 *    2. Set `time_stopped` to `TRUE`.
 *
 * @calls
 * - `read_cntvct()`
 */
void ja_system_time_stop(void)
{
    if ((timer_freq != 0U) && !time_stopped)
    {
        counter_stopped_at = read_cntvct();
        time_stopped = TRUE;
    }
}

/**
 * @funcname `ja_system_time_resume`
 *
 * @description
 * Resumes the system time when the GDB server lets the system go on: the
 * time spent stopped doesn't count, and the tick that came meanwhile is no
 * longer due.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `counter_stopped_at` | `uint64_t` | The counter when the system stopped |
 * | `time_stopped` | `pok_bool_t` | TRUE while the GDB server holds the system |
 * | `timer_next` | `uint64_t` | The counter value of the next timer tick |
 * | `timer_start` | `uint64_t` | The counter at boot (moved by stopped time) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stopped` | `uint64_t` | The counter ticks spent stopped |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `time_stopped` is true:
 *    1. Set `stopped` to `read_cntvct() - counter_stopped_at`.
 *    2. Add `stopped` to `timer_start`.
 *    3. Add `stopped` to `timer_next`.
 *    4. Call `write_cntv_cval()` with the argument `timer_next`.
 *    5. Set `time_stopped` to `FALSE`.
 *
 * @calls
 * - `read_cntvct()`
 * - `write_cntv_cval()`
 */
void ja_system_time_resume(void)
{
    if (time_stopped)
    {
        uint64_t stopped = read_cntvct() - counter_stopped_at;

        timer_start += stopped;
        timer_next += stopped;
        // The tick which has come meanwhile is no longer due.
        write_cntv_cval(timer_next);
        time_stopped = FALSE;
    }
}
#endif /* POK_NEEDS_GDB */

/**
 * @funcname `ja_bsp_reset`
 *
 * @description
 * Resets the board through PSCI SYSTEM_RESET: with smc if the kernel was
 * entered at EL2, with hvc otherwise. If PSCI isn't available, waits for
 * interrupts forever.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_boot_from_el2` | `char` | Non-zero if the kernel was entered at EL2 |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `ja_boot_from_el2` is not equal to `'\0'`:
 *    1. Call `psci_call_smc()` with the argument `PSCI_SYSTEM_RESET`.
 * 2. Otherwise:
 *    1. Call `psci_call_hvc()` with the argument `PSCI_SYSTEM_RESET`.
 * 3. While `true` is true:
 *    1. Call `wait_for_interrupt()`.
 *
 * @calls
 * - `psci_call_smc()`
 * - `psci_call_hvc()`
 * - `wait_for_interrupt()`
 */
void ja_bsp_reset(void)
{
    if (ja_boot_from_el2 != '\0') {
        psci_call_smc(PSCI_SYSTEM_RESET);
    } else {
        psci_call_hvc(PSCI_SYSTEM_RESET);
    }

    // PSCI isn't available.
    while (true) {
        wait_for_interrupt();
    }
}
