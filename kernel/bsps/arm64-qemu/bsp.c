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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
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
 * - \hlrid{HLR_BSP_READ_CNTVCT_01}: `read_cntvct()` shall give the virtual
 *   counter of the processor, read after the instructions before it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_001}
 *   - \srqref{SRQ_JET_BSP_003}
 *
 * @llr
 * - \llrid{LLR_BSP_READ_CNTVCT_01}: `read_cntvct()` shall execute the
 *   instruction `isb` and then return the value of CNTVCT_EL0.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_READ_CNTVCT_01}
 *
 * @design
 * 1. Execute the assembly code `isb; mrs %0, cntvct_el0`.
 *    \llrref{LLR_BSP_READ_CNTVCT_01}
 * 2. Return `val`. \llrref{LLR_BSP_READ_CNTVCT_01}
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
 * - \hlrid{HLR_BSP_READ_CNTFRQ_01}: `read_cntfrq()` shall give the frequency of
 *   the system counter.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_001}
 *   - \srqref{SRQ_JET_BSP_003}
 *
 * @llr
 * - \llrid{LLR_BSP_READ_CNTFRQ_01}: `read_cntfrq()` shall return the value of
 *   CNTFRQ_EL0.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_READ_CNTFRQ_01}
 *
 * @design
 * 1. Execute the assembly code `mrs %0, cntfrq_el0`.
 *    \llrref{LLR_BSP_READ_CNTFRQ_01}
 * 2. Return `val`. \llrref{LLR_BSP_READ_CNTFRQ_01}
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
 * - \hlrid{HLR_BSP_WRITE_CNTV_CVAL_01}: `write_cntv_cval()` shall set the
 *   counter value at which the virtual timer fires.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *
 * @llr
 * - \llrid{LLR_BSP_WRITE_CNTV_CVAL_01}: `write_cntv_cval()` shall write `val`
 *   to CNTV_CVAL_EL0.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_WRITE_CNTV_CVAL_01}
 *
 * @design
 * 1. Execute the assembly code `msr cntv_cval_el0, %0`.
 *    \llrref{LLR_BSP_WRITE_CNTV_CVAL_01}
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
 * - \hlrid{HLR_BSP_WRITE_CNTV_CTL_01}: `write_cntv_ctl()` shall set the control
 *   register of the virtual timer.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *
 * @llr
 * - \llrid{LLR_BSP_WRITE_CNTV_CTL_01}: `write_cntv_ctl()` shall write `val` to
 *   CNTV_CTL_EL0.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_WRITE_CNTV_CTL_01}
 *
 * @design
 * 1. Execute the assembly code `msr cntv_ctl_el0, %0`.
 *    \llrref{LLR_BSP_WRITE_CNTV_CTL_01}
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
 * - \hlrid{HLR_BSP_READ_CNTV_CTL_01}: `read_cntv_ctl()` shall give the control
 *   register of the virtual timer, which tells whether it fired.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *
 * @llr
 * - \llrid{LLR_BSP_READ_CNTV_CTL_01}: `read_cntv_ctl()` shall return the value
 *   of CNTV_CTL_EL0.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_READ_CNTV_CTL_01}
 *
 * @design
 * 1. Execute the assembly code `mrs %0, cntv_ctl_el0`.
 *    \llrref{LLR_BSP_READ_CNTV_CTL_01}
 * 2. Return `val`. \llrref{LLR_BSP_READ_CNTV_CTL_01}
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
 * - \hlrid{HLR_BSP_WAIT_FOR_INTERRUPT_01}: `wait_for_interrupt()` shall let the
 *   processor wait, idle, for an interrupt.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_003}
 *
 * @llr
 * - \llrid{LLR_BSP_WAIT_FOR_INTERRUPT_01}: `wait_for_interrupt()` shall execute
 *   the instruction `wfi` once.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_WAIT_FOR_INTERRUPT_01}
 *
 * @design
 * 1. Execute the assembly code `wfi`. \llrref{LLR_BSP_WAIT_FOR_INTERRUPT_01}
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
 * - \hlrid{HLR_BSP_PSCI_CALL_SMC_01}: `psci_call_smc()` shall call a PSCI
 *   function through the secure monitor, for a kernel entered at exception
 *   level 2.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_003}
 *
 * @llr
 * - \llrid{LLR_BSP_PSCI_CALL_SMC_01}: `psci_call_smc()` shall execute the
 *   instruction `smc #0` once with `fn` in the register x0.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_PSCI_CALL_SMC_01}
 *
 * @design
 * 1. Set `x0` to `fn`. \llrref{LLR_BSP_PSCI_CALL_SMC_01}
 * 2. Execute the assembly code `smc #0`. \llrref{LLR_BSP_PSCI_CALL_SMC_01}
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
 * - \hlrid{HLR_BSP_PSCI_CALL_HVC_01}: `psci_call_hvc()` shall call a PSCI
 *   function through the hypervisor, for a kernel entered at exception level 1.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_003}
 *
 * @llr
 * - \llrid{LLR_BSP_PSCI_CALL_HVC_01}: `psci_call_hvc()` shall execute the
 *   instruction `hvc #0` once with `fn` in the register x0.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_PSCI_CALL_HVC_01}
 *
 * @design
 * 1. Set `x0` to `fn`. \llrref{LLR_BSP_PSCI_CALL_HVC_01}
 * 2. Execute the assembly code `hvc #0`. \llrref{LLR_BSP_PSCI_CALL_HVC_01}
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
 * - \hlrid{HLR_BSP_GIC_INIT_01}: `gic_init()` shall set up the GICv2 so that it
 *   delivers the interrupt of the virtual timer and lets every priority
 *   through.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_003}
 *   - \srqref{SRQ_JET_INT_001}
 *   - \srqref{SRQ_JET_INT_006}
 *
 * @llr
 * - \llrid{LLR_BSP_GIC_INIT_01}: `gic_init()` shall write, in this order, 0 to
 *   `BOARD_GICD_BASE + GICD_CTLR` with `ja_mmio_write32()`, `0xa0` to
 *   `BOARD_GICD_BASE + GICD_IPRIORITYR(BOARD_IRQ_VTIMER)` with
 *   `ja_mmio_write8()`, and then with `ja_mmio_write32()` the bit
 *   `BOARD_IRQ_VTIMER % 32` to `BOARD_GICD_BASE +
 *   GICD_ISENABLER(BOARD_IRQ_VTIMER / 32)`, 1 to `BOARD_GICD_BASE + GICD_CTLR`,
 *   `0xff` to `BOARD_GICC_BASE + GICC_PMR` and 1 to `BOARD_GICC_BASE +
 *   GICC_CTLR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_GIC_INIT_01}
 *
 * @design
 * 1. Call `ja_mmio_write32()` with the arguments `BOARD_GICD_BASE + GICD_CTLR`
 *    and `0`. \llrref{LLR_BSP_GIC_INIT_01}
 * 2. Call `ja_mmio_write8()` with the arguments
 *    `BOARD_GICD_BASE + GICD_IPRIORITYR(BOARD_IRQ_VTIMER)` and `0xa0U`.
 *    \llrref{LLR_BSP_GIC_INIT_01}
 * 3. Call `ja_mmio_write32()` with the arguments
 *    `BOARD_GICD_BASE + GICD_ISENABLER(BOARD_IRQ_VTIMER / 32U)` and
 *    `(uint32_t) 1U << (BOARD_IRQ_VTIMER % 32U)`. \llrref{LLR_BSP_GIC_INIT_01}
 * 4. Call `ja_mmio_write32()` with the arguments `BOARD_GICD_BASE + GICD_CTLR`
 *    and `1`. \llrref{LLR_BSP_GIC_INIT_01}
 * 5. Call `ja_mmio_write32()` with the arguments `BOARD_GICC_BASE + GICC_PMR`
 *    and `0xff`. \llrref{LLR_BSP_GIC_INIT_01}
 * 6. Call `ja_mmio_write32()` with the arguments `BOARD_GICC_BASE + GICC_CTLR`
 *    and `1`. \llrref{LLR_BSP_GIC_INIT_01}
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
 * - \hlrid{HLR_BSP_TIMER_INIT_01}: `timer_init()` shall start the system time
 *   at boot and the virtual timer at the frequency of the build.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *   - \srqref{SRQ_JET_BSP_003}
 *
 * @llr
 * - \llrid{LLR_BSP_TIMER_INIT_01}: `timer_init()` shall set `timer_freq` to the
 *   result of `read_cntfrq()`, `timer_period` to `timer_freq` divided by
 *   `POK_TIMER_FREQUENCY`, `timer_start` to the result of `read_cntvct()` and
 *   `timer_next` to `timer_start` plus `timer_period`, and then call
 *   `write_cntv_cval()` with `timer_next` and `write_cntv_ctl()` with
 *   `CNTV_CTL_ENABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_TIMER_INIT_01}
 *
 * @design
 * 1. Call `read_cntfrq()`, and store the result in `timer_freq`.
 *    \llrref{LLR_BSP_TIMER_INIT_01}
 * 2. Set `timer_period` to `timer_freq / POK_TIMER_FREQUENCY`.
 *    \llrref{LLR_BSP_TIMER_INIT_01}
 * 3. Call `read_cntvct()`, and store the result in `timer_start`.
 *    \llrref{LLR_BSP_TIMER_INIT_01}
 * 4. Set `timer_next` to `timer_start + timer_period`.
 *    \llrref{LLR_BSP_TIMER_INIT_01}
 * 5. Call `write_cntv_cval()` with the argument `timer_next`.
 *    \llrref{LLR_BSP_TIMER_INIT_01}
 * 6. Call `write_cntv_ctl()` with the argument `CNTV_CTL_ENABLE`.
 *    \llrref{LLR_BSP_TIMER_INIT_01}
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
 * - \hlrid{HLR_BSP_ARM64_JA_BSP_INIT_01}: `ja_bsp_init()` shall initialize the
 *   board when the kernel starts: the consoles, the calendar time from the
 *   real-time clock, the interrupt controller and the timer.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_002}
 *   - \srqref{SRQ_JET_BOOT_003}
 *   - \srqref{SRQ_JET_BSP_003}
 *   - \srqref{SRQ_JET_BSP_005}
 * - \hlrid{HLR_BSP_ARM64_JA_BSP_INIT_02}: `ja_bsp_init()` shall take the
 *   calendar time at the start from the PL031 real-time clock.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_006}
 *
 * @llr
 * - \llrid{LLR_BSP_ARM64_JA_BSP_INIT_01}: `ja_bsp_init()` shall call, in this
 *   order, `jet_console_init_all()`, `gic_init()` and `timer_init()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_ARM64_JA_BSP_INIT_01}
 * - \llrid{LLR_BSP_ARM64_JA_BSP_INIT_02}: `ja_bsp_init()` shall set
 *   `base_calendar_time` to the value read with `ja_mmio_read32()` from
 *   `BOARD_RTC_BASE + RTC_DR`, after `jet_console_init_all()` and before
 *   `gic_init()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_ARM64_JA_BSP_INIT_02}
 *
 * @design
 * 1. Call `jet_console_init_all()`. \llrref{LLR_BSP_ARM64_JA_BSP_INIT_01}
 * 2. Set `base_calendar_time` to
 *    `(time_t)ja_mmio_read32(BOARD_RTC_BASE + RTC_DR)`.
 *    \llrref{LLR_BSP_ARM64_JA_BSP_INIT_02}
 * 3. Call `gic_init()`. \llrref{LLR_BSP_ARM64_JA_BSP_INIT_01}
 * 4. Call `timer_init()`. \llrref{LLR_BSP_ARM64_JA_BSP_INIT_01}
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
 * - \hlrid{HLR_BSP_SET_NEXT_TICK_01}: `set_next_tick()` shall program the next
 *   tick of the timer one period after the last one, or one period after the
 *   present time if it is due already, which lowers the timer's interrupt line.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *
 * @llr
 * - \llrid{LLR_BSP_SET_NEXT_TICK_01}: If `timer_next` plus `timer_period` is
 *   greater than the result of `read_cntvct()`, `set_next_tick()` shall set
 *   `timer_next` to `timer_next` plus `timer_period`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_SET_NEXT_TICK_01}
 * - \llrid{LLR_BSP_SET_NEXT_TICK_02}: If `timer_next` plus `timer_period` is
 *   not greater than the result of `read_cntvct()`, `set_next_tick()` shall set
 *   `timer_next` to that result plus `timer_period`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_SET_NEXT_TICK_01}
 * - \llrid{LLR_BSP_SET_NEXT_TICK_03}: `set_next_tick()` shall then call
 *   `write_cntv_cval()` once with the new `timer_next`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_SET_NEXT_TICK_01}
 *
 * @design
 * 1. Call `read_cntvct()`, and store the result in `now`.
 *    \llrref{LLR_BSP_SET_NEXT_TICK_01}, \llrref{LLR_BSP_SET_NEXT_TICK_02}
 * 2. Add `timer_period` to `timer_next`. \llrref{LLR_BSP_SET_NEXT_TICK_01},
 *    \llrref{LLR_BSP_SET_NEXT_TICK_02}
 * 3. If `timer_next` is less than or equal to `now`:
 *    \llrref{LLR_BSP_SET_NEXT_TICK_01}, \llrref{LLR_BSP_SET_NEXT_TICK_02}
 *    1. Set `timer_next` to `now + timer_period`.
 *       \llrref{LLR_BSP_SET_NEXT_TICK_02}
 * 4. Call `write_cntv_cval()` with the argument `timer_next`.
 *    \llrref{LLR_BSP_SET_NEXT_TICK_03}
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
 * - \hlrid{HLR_BSP_JA_BSP_PROCESS_IRQ_01}: `ja_bsp_process_irq()` shall
 *   acknowledge an interrupt at the GIC, count a tick for the virtual timer,
 *   ignore a spurious interrupt and report any other one.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_001}
 *   - \srqref{SRQ_JET_INT_006}
 *   - \srqref{SRQ_JET_INT_005}
 * - \hlrid{HLR_BSP_JA_BSP_PROCESS_IRQ_02}: `ja_bsp_process_irq()` shall let the
 *   kernel count each tick of the timer once, after the next tick is
 *   programmed.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_004}
 *
 * @llr
 * - \llrid{LLR_BSP_JA_BSP_PROCESS_IRQ_01}: If the interrupt ID (the low 10 bits
 *   of the value read with `ja_mmio_read32()` from `BOARD_GICC_BASE +
 *   GICC_IAR`) is `GIC_SPURIOUS` or more, `ja_bsp_process_irq()` shall return
 *   without writing `BOARD_GICC_BASE + GICC_EOIR`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_BSP_PROCESS_IRQ_01}
 * - \llrid{LLR_BSP_JA_BSP_PROCESS_IRQ_02}: If the interrupt ID is
 *   `BOARD_IRQ_VTIMER` and `CNTV_CTL_ISTATUS` is set in the result of
 *   `read_cntv_ctl()`, `ja_bsp_process_irq()` shall call `set_next_tick()`,
 *   then write the value read from `GICC_IAR` to `BOARD_GICC_BASE + GICC_EOIR`
 *   with `ja_mmio_write32()`, and then call `jet_on_tick()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_BSP_PROCESS_IRQ_02}
 * - \llrid{LLR_BSP_JA_BSP_PROCESS_IRQ_03}: If the interrupt ID is
 *   `BOARD_IRQ_VTIMER` and `CNTV_CTL_ISTATUS` is not set in the result of
 *   `read_cntv_ctl()`, `ja_bsp_process_irq()` shall write the value read from
 *   `GICC_IAR` to `BOARD_GICC_BASE + GICC_EOIR` with `ja_mmio_write32()`,
 *   without calling `set_next_tick()` or `jet_on_tick()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_BSP_PROCESS_IRQ_01}
 * - \llrid{LLR_BSP_JA_BSP_PROCESS_IRQ_04}: If the interrupt ID is less than
 *   `GIC_SPURIOUS` and is not `BOARD_IRQ_VTIMER`, `ja_bsp_process_irq()` shall
 *   write the value read from `GICC_IAR` to `BOARD_GICC_BASE + GICC_EOIR` with
 *   `ja_mmio_write32()`, and then call `printf()` once with `"Unexpected IRQ
 *   %%u\n"` and the interrupt ID.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_BSP_PROCESS_IRQ_01}
 *
 * @design
 * 1. Call `ja_mmio_read32()` with the argument `BOARD_GICC_BASE + GICC_IAR`,
 *    and store the result in `iar`. \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_01},
 *    \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_02},
 *    \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_03},
 *    \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_04}
 * 2. Set `irq` to `iar & 0x3ffU`. \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_01},
 *    \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_02},
 *    \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_03},
 *    \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_04}
 * 3. If `irq` is greater than or equal to `GIC_SPURIOUS`:
 *    \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_01}
 *    1. Return.
 * 4. If `irq` is equal to `BOARD_IRQ_VTIMER`:
 *    \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_02},
 *    \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_03}
 *    1. Set `tick` to `(read_cntv_ctl() & CNTV_CTL_ISTATUS) != 0U`.
 *    2. If `tick` is true: \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_02},
 *       \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_03}
 *       1. Call `set_next_tick()`. \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_02}
 *    3. Call `ja_mmio_write32()` with the arguments
 *       `BOARD_GICC_BASE + GICC_EOIR` and `iar`.
 *    4. If `tick` is true: \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_02},
 *       \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_03}
 *       1. Call `jet_on_tick()`. \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_02}
 * 5. Otherwise: \llrref{LLR_BSP_JA_BSP_PROCESS_IRQ_04}
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
 * - \hlrid{HLR_BSP_JA_SYSTEM_TIME_01}: `ja_system_time()` shall give the system
 *   time in nanoseconds since the start, from the virtual counter.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_001}
 * - \hlrid{HLR_BSP_JA_SYSTEM_TIME_02}: `ja_system_time()` shall keep the system
 *   time where it stopped while the debugger holds the module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_013}
 *
 * @llr
 * - \llrid{LLR_BSP_JA_SYSTEM_TIME_01}: If `time_stopped` is false,
 *   `ja_system_time()` shall return the nanoseconds of the result of
 *   `read_cntvct()` minus `timer_start`: these ticks divided by `timer_freq`,
 *   times 1000000000, plus the remainder of that division times 1000000000
 *   divided by `timer_freq`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_SYSTEM_TIME_01}
 * - \llrid{LLR_BSP_JA_SYSTEM_TIME_02}: If `time_stopped` is true,
 *   `ja_system_time()` shall return the nanoseconds, computed in the same way,
 *   of `counter_stopped_at` minus `timer_start`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_SYSTEM_TIME_02}
 *
 * @design
 * 1. Call `read_cntvct()`, and store the result in `now`.
 *    \llrref{LLR_BSP_JA_SYSTEM_TIME_01}, \llrref{LLR_BSP_JA_SYSTEM_TIME_02}
 * 2. If `time_stopped` is true: \llrref{LLR_BSP_JA_SYSTEM_TIME_01},
 *    \llrref{LLR_BSP_JA_SYSTEM_TIME_02}
 *    1. Set `now` to `counter_stopped_at`. \llrref{LLR_BSP_JA_SYSTEM_TIME_02}
 * 3. Set `ticks` to `now - timer_start`. \llrref{LLR_BSP_JA_SYSTEM_TIME_01},
 *    \llrref{LLR_BSP_JA_SYSTEM_TIME_02}
 * 4. Set `ns` to
 *    `((ticks / timer_freq) * 1000000000ULL) + (((ticks % timer_freq) *`
 *    `1000000000ULL) / timer_freq)`. \llrref{LLR_BSP_JA_SYSTEM_TIME_01},
 *    \llrref{LLR_BSP_JA_SYSTEM_TIME_02}
 * 5. Return `(pok_time_t) ns`. \llrref{LLR_BSP_JA_SYSTEM_TIME_01},
 *    \llrref{LLR_BSP_JA_SYSTEM_TIME_02}
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
 * - \hlrid{HLR_BSP_JA_CALENDAR_TIME_01}: `ja_calendar_time()` shall give the
 *   calendar time: the time of the real-time clock at the start plus the system
 *   time, in seconds.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_TIME_003}
 *
 * @llr
 * - \llrid{LLR_BSP_JA_CALENDAR_TIME_01}: `ja_calendar_time()` shall return
 *   `base_calendar_time` plus the result of `ja_system_time()` divided
 *   by 1000000000.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_CALENDAR_TIME_01}
 *
 * @design
 * 1. Return `base_calendar_time + (time_t)(ja_system_time() / 1000000000)`.
 *    \llrref{LLR_BSP_JA_CALENDAR_TIME_01}
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
 * - \hlrid{HLR_BSP_JA_SYSTEM_TIME_STOP_01}: `ja_system_time_stop()` shall stop
 *   the system time while the debugger holds the module.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_013}
 *
 * @llr
 * - \llrid{LLR_BSP_JA_SYSTEM_TIME_STOP_01}: If and only if `timer_freq` is not
 *   0 and `time_stopped` is false, `ja_system_time_stop()` shall set
 *   `counter_stopped_at` to the result of `read_cntvct()` and `time_stopped` to
 *   `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_SYSTEM_TIME_STOP_01}
 *
 * @design
 * 1. If `timer_freq` is not equal to `0U` and `time_stopped` is false:
 *    \llrref{LLR_BSP_JA_SYSTEM_TIME_STOP_01}
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
 * - \hlrid{HLR_BSP_JA_SYSTEM_TIME_RESUME_01}: `ja_system_time_resume()` shall
 *   resume the system time when the debugger lets the module go on, without
 *   counting the time stopped, the tick that came meanwhile being no longer
 *   due.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_013}
 *
 * @llr
 * - \llrid{LLR_BSP_JA_SYSTEM_TIME_RESUME_01}: If and only if `time_stopped` is
 *   true, `ja_system_time_resume()` shall add the result of `read_cntvct()`
 *   minus `counter_stopped_at` to `timer_start` and to `timer_next`, call
 *   `write_cntv_cval()` once with the new `timer_next`, and set `time_stopped`
 *   to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_SYSTEM_TIME_RESUME_01}
 *
 * @design
 * 1. If `time_stopped` is true: \llrref{LLR_BSP_JA_SYSTEM_TIME_RESUME_01}
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
 * - \hlrid{HLR_BSP_JA_BSP_RESET_01}: `ja_bsp_reset()` shall reset the module
 *   through PSCI, and wait for interrupts for ever if that does not reset it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BSP_003}
 *
 * @llr
 * - \llrid{LLR_BSP_JA_BSP_RESET_01}: If `ja_boot_from_el2` is not `'\0'`,
 *   `ja_bsp_reset()` shall call `psci_call_smc()` once with
 *   `PSCI_SYSTEM_RESET`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_BSP_RESET_01}
 * - \llrid{LLR_BSP_JA_BSP_RESET_02}: If `ja_boot_from_el2` is `'\0'`,
 *   `ja_bsp_reset()` shall call `psci_call_hvc()` once with
 *   `PSCI_SYSTEM_RESET`.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_BSP_RESET_01}
 * - \llrid{LLR_BSP_JA_BSP_RESET_03}: `ja_bsp_reset()` shall then call
 *   `wait_for_interrupt()` again and again, without returning.
 *   <br>Traces to:
 *   - \hlrref{HLR_BSP_JA_BSP_RESET_01}
 *
 * @design
 * 1. If `ja_boot_from_el2` is not equal to `'\0'`:
 *    \llrref{LLR_BSP_JA_BSP_RESET_01}
 *    1. Call `psci_call_smc()` with the argument `PSCI_SYSTEM_RESET`.
 * 2. Otherwise: \llrref{LLR_BSP_JA_BSP_RESET_02}
 *    1. Call `psci_call_hvc()` with the argument `PSCI_SYSTEM_RESET`.
 * 3. While `true` is true: \llrref{LLR_BSP_JA_BSP_RESET_03}
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
