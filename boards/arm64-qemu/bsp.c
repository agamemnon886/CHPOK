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

#define GICD_CTLR           0x000
#define GICD_ISENABLER(n)   (0x100 + 4 * (n))
#define GICD_IPRIORITYR(n)  (0x400 + (n))
#define GICC_CTLR           0x000
#define GICC_PMR            0x004
#define GICC_IAR            0x00c
#define GICC_EOIR           0x010

#define GIC_SPURIOUS        1020

#define RTC_DR              0x000

#define CNTV_CTL_ENABLE     (1 << 0)

#define PSCI_SYSTEM_RESET   0x84000009

/* Set by entry.S. PSCI uses smc if the kernel was entered at EL2, hvc otherwise. */
extern char ja_boot_from_el2;

static uint64_t timer_freq;
static uint64_t timer_period;   /* in counter ticks */
static uint64_t timer_start;    /* counter value at boot */
static uint64_t timer_next;     /* counter value for the next tick */

static time_t base_calendar_time;

static uint64_t read_cntvct(void)
{
    uint64_t val;
    asm volatile("isb\n mrs %0, cntvct_el0" : "=r" (val) :: "memory");
    return val;
}

static void gic_init(void)
{
    ja_mmio_write32(BOARD_GICD_BASE + GICD_CTLR, 0);
    ja_mmio_write8(BOARD_GICD_BASE + GICD_IPRIORITYR(BOARD_IRQ_VTIMER), 0xa0);
    ja_mmio_write32(BOARD_GICD_BASE + GICD_ISENABLER(BOARD_IRQ_VTIMER / 32),
        1U << (BOARD_IRQ_VTIMER % 32));
    ja_mmio_write32(BOARD_GICD_BASE + GICD_CTLR, 1);

    ja_mmio_write32(BOARD_GICC_BASE + GICC_PMR, 0xff);
    ja_mmio_write32(BOARD_GICC_BASE + GICC_CTLR, 1);
}

static void timer_init(void)
{
    asm volatile("mrs %0, cntfrq_el0" : "=r" (timer_freq));
    timer_period = timer_freq / POK_TIMER_FREQUENCY;

    timer_start = read_cntvct();
    timer_next = timer_start + timer_period;

    asm volatile("msr cntv_cval_el0, %0" :: "r" (timer_next));
    asm volatile("msr cntv_ctl_el0, %0" :: "r" ((uint64_t)CNTV_CTL_ENABLE));
}

void ja_bsp_init (void)
{
    jet_console_init_all();

    base_calendar_time = (time_t)ja_mmio_read32(BOARD_RTC_BASE + RTC_DR);

    gic_init();
    timer_init();
}

static void process_timer(void)
{
    uint64_t now = read_cntvct();

    timer_next += timer_period;
    if (timer_next <= now)
    {
        // Ticks were missed (e.g. the host was busy): don't try to catch up.
        timer_next = now + timer_period;
    }
    asm volatile("msr cntv_cval_el0, %0" :: "r" (timer_next));

    jet_on_tick();
}

void ja_bsp_process_irq(interrupt_frame* frame)
{
    (void) frame;

    uint32_t iar = ja_mmio_read32(BOARD_GICC_BASE + GICC_IAR);
    uint32_t irq = iar & 0x3ff;

    if (irq >= GIC_SPURIOUS)
        return;

    // End of interrupt before jet_on_tick(), which may switch context.
    ja_mmio_write32(BOARD_GICC_BASE + GICC_EOIR, iar);

    if (irq == BOARD_IRQ_VTIMER)
        process_timer();
    else
        printf("Unexpected IRQ %u\n", (unsigned)irq);
}

pok_time_t ja_system_time(void)
{
    uint64_t ticks = read_cntvct() - timer_start;

    // Split to avoid overflow: (ticks % freq) * 1e9 < freq * 1e9.
    return (pok_time_t)((ticks / timer_freq) * 1000000000ULL
        + (ticks % timer_freq) * 1000000000ULL / timer_freq);
}

time_t ja_calendar_time(void)
{
   return base_calendar_time + (time_t)(ja_system_time() / 1000000000);
}

void ja_bsp_reset(void)
{
    register uint64_t x0 asm("x0") = PSCI_SYSTEM_RESET;

    if (ja_boot_from_el2)
        asm volatile("smc #0" : "+r" (x0) :: "memory");
    else
        asm volatile("hvc #0" : "+r" (x0) :: "memory");

    // PSCI isn't available.
    while (1)
        asm volatile("wfi");
}
