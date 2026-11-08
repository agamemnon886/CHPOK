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
 * counted twice.
 */

/*
 * Board support for QEMU 'virt': GICv2 interrupt controller, EL1
 * virtual timer as the system tick, PL031 RTC for calendar time and
 * PSCI for reset.
 */

#include <libc.h>
#include <core/time.h>
#include <core/debug.h>
#include <asp/entries.h>
#include <asp/time.h>
#include <arch/ioports.h>
#include <bsp/bsp.h>

#define GICD_CTLR           0x000U
#define GICD_ISENABLER(n)   (0x100U + (4U * (n)))
#define GICD_IPRIORITYR(n)  (0x400U + (n))
#define GICC_CTLR           0x000U
#define GICC_PMR            0x004U
#define GICC_IAR            0x00cU
#define GICC_EOIR           0x010U

#define GIC_SPURIOUS        1020U

#define RTC_DR              0x000U

#define CNTV_CTL_ENABLE     (1U << 0)
#define CNTV_CTL_ISTATUS    (1U << 2)

#define PSCI_SYSTEM_RESET   0x84000009U

/* Set by entry.S. PSCI uses smc if the kernel was entered at EL2, hvc otherwise. */
extern char ja_boot_from_el2;

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

static uint64_t read_cntfrq(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (val));
    return val;
}

static void write_cntv_cval(uint64_t val)
{
    asm volatile("msr cntv_cval_el0, %0" :: "r" (val));
}

static void write_cntv_ctl(uint64_t val)
{
    asm volatile("msr cntv_ctl_el0, %0" :: "r" (val));
}

static uint64_t read_cntv_ctl(void)
{
    uint64_t val;
    asm volatile("mrs %0, cntv_ctl_el0" : "=r" (val));
    return val;
}

static void wait_for_interrupt(void)
{
    asm volatile("wfi");
}

/* Call the PSCI function 'fn' through the secure monitor (smc). */
static void psci_call_smc(uint64_t fn)
{
    register uint64_t x0 asm("x0") = fn;
    asm volatile("smc #0" : "+r" (x0) :: "memory");
}

/* Call the PSCI function 'fn' through the hypervisor (hvc). */
static void psci_call_hvc(uint64_t fn)
{
    register uint64_t x0 asm("x0") = fn;
    asm volatile("hvc #0" : "+r" (x0) :: "memory");
}

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

static void timer_init(void)
{
    timer_freq = read_cntfrq();
    timer_period = timer_freq / POK_TIMER_FREQUENCY;

    timer_start = read_cntvct();
    timer_next = timer_start + timer_period;

    write_cntv_cval(timer_next);
    write_cntv_ctl(CNTV_CTL_ENABLE);
}

void ja_bsp_init (void)
{
    jet_console_init_all();

    base_calendar_time = (time_t)ja_mmio_read32(BOARD_RTC_BASE + RTC_DR);

    gic_init();
    timer_init();
}

/*
 * Program the compare value of the next tick. This lowers the timer's
 * interrupt line, which must be done before the end of interrupt: under a
 * hypervisor, the end of interrupt also ends the physical interrupt, which
 * would come again at once if the line were still high.
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

time_t ja_calendar_time(void)
{
   return base_calendar_time + (time_t)(ja_system_time() / 1000000000);
}

#ifdef POK_NEEDS_GDB
void ja_system_time_stop(void)
{
    if ((timer_freq != 0U) && !time_stopped)
    {
        counter_stopped_at = read_cntvct();
        time_stopped = TRUE;
    }
}

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
