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
 * hypervisor/arch/x86_64/lapic.c (the code of the processor architecture is in
 * hypervisor/csps).
 */

/**
 * @file
 * Local APIC of the physical CPUs (xAPIC, memory-mapped): the timer that
 * drives the guests' virtual PIT, end of interrupt, and the INIT/SIPI
 * messages that start the other CPUs. The guests have no access to it.
 */

#include <arch/internal.h>

// Physical (= virtual) address of the local APIC registers.
static uintptr_t lapic_base = 0xFEE00000UL;

/**
 * @funcname `lapic_read`
 *
 * @description
 * Reads a local APIC register.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `reg` | `uint32_t` | [in] The register offset. |
 *
 * @return The register value.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lapic_base` | `uintptr_t` | The address of the local APIC registers |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `const volatile uint32_t *` | The register |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `(const volatile uint32_t *) (lapic_base + reg)`.
 * 2. Return the value at `p`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.4 deviation (lapic_read(), lapic_write()): the
 * registers are at a fixed physical address, identity-mapped (uncached)
 * by entry.S, which has to be converted to a pointer.
 */
static uint32_t lapic_read(uint32_t reg)
{
    const volatile uint32_t *p =
        (const volatile uint32_t *) (lapic_base + reg);

    return *p;
}

/**
 * @funcname `lapic_write`
 *
 * @description
 * Writes a local APIC register.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `reg` | `uint32_t` | [in] The register offset. |
 * | `value` | `uint32_t` | [in] The value. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lapic_base` | `uintptr_t` | The address of the local APIC registers |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `p` | `volatile uint32_t *` | The register |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `p` to `(volatile uint32_t *) (lapic_base + reg)`.
 * 2. Set `*p` to `value`.
 *
 * @calls
 * None.
 */
static void lapic_write(uint32_t reg, uint32_t value)
{
    volatile uint32_t *p = (volatile uint32_t *) (lapic_base + reg);

    *p = value;
}

/**
 * @funcname `lapic_init_base`
 *
 * @description
 * Takes the address of the local APIC registers from the IA32_APIC_BASE
 * MSR. Called once at boot.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `lapic_base` | `uintptr_t` | The address of the local APIC registers |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `lapic_base` to
 *    `(uintptr_t) (x86_rdmsr(MSR_APIC_BASE) & APIC_BASE_ADDR_MASK)`.
 *
 * @calls
 * - `x86_rdmsr()`
 */
void lapic_init_base(void)
{
    lapic_base = (uintptr_t) (x86_rdmsr(MSR_APIC_BASE)
                              & APIC_BASE_ADDR_MASK);
}

/**
 * @funcname `lapic_init_cpu`
 *
 * @description
 * Sets up the local APIC of this CPU: software-enabled with the spurious
 * vector, LINT0 masked (the legacy PIC isn't used), LINT1 the NMI, errors
 * masked, the timer stopped and divided by 16.
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
 * 1. Call `lapic_write()` with the arguments `LAPIC_SVR` and
 *    `LAPIC_SVR_ENABLE | HV_VECTOR_SPURIOUS`.
 * 2. Call `lapic_write()` with the arguments `LAPIC_LVT_LINT0` and
 *    `LAPIC_LVT_MASKED`.
 * 3. Call `lapic_write()` with the arguments `LAPIC_LVT_LINT1` and
 *    `LAPIC_LVT_NMI`.
 * 4. Call `lapic_write()` with the arguments `LAPIC_LVT_ERROR` and
 *    `LAPIC_LVT_MASKED`.
 * 5. Call `lapic_write()` with the arguments `LAPIC_TIMER_DIVIDE` and
 *    `LAPIC_TIMER_DIVIDE_16`.
 * 6. Call `lapic_timer_stop()`.
 *
 * @calls
 * - `lapic_write()`
 * - `lapic_timer_stop()`
 */
void lapic_init_cpu(void)
{
    // Software-enable the local APIC; spurious interrupts are ignored.
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | HV_VECTOR_SPURIOUS);
    // The legacy PIC is masked; LINT0 is not used, LINT1 is the NMI.
    lapic_write(LAPIC_LVT_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, LAPIC_LVT_NMI);
    lapic_write(LAPIC_LVT_ERROR, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_TIMER_DIVIDE, LAPIC_TIMER_DIVIDE_16);
    lapic_timer_stop();
}

/**
 * @funcname `lapic_id`
 *
 * @description
 * Returns the local APIC ID of this CPU.
 *
 * @return The APIC ID.
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
 * 1. Return `lapic_read(LAPIC_ID) >> 24`.
 *
 * @calls
 * - `lapic_read()`
 */
uint32_t lapic_id(void)
{
    return lapic_read(LAPIC_ID) >> 24;
}

/**
 * @funcname `lapic_eoi`
 *
 * @description
 * Signals the end of the current interrupt to the local APIC.
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
 * 1. Call `lapic_write()` with the arguments `LAPIC_EOI` and `0U`.
 *
 * @calls
 * - `lapic_write()`
 */
void lapic_eoi(void)
{
    lapic_write(LAPIC_EOI, 0U);
}

/**
 * @funcname `lapic_timer_periodic`
 *
 * @description
 * Starts the local APIC timer in periodic mode: an interrupt (the timer
 * vector) every count ticks of the bus clock divided by 16.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `uint32_t` | [in] The period, in timer ticks. |
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
 * 1. Call `lapic_write()` with the arguments `LAPIC_LVT_TIMER` and
 *    `LAPIC_TIMER_PERIODIC | HV_VECTOR_TIMER`.
 * 2. Call `lapic_write()` with the arguments `LAPIC_TIMER_INIT` and `count`.
 *
 * @calls
 * - `lapic_write()`
 */
void lapic_timer_periodic(uint32_t count)
{
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_PERIODIC | HV_VECTOR_TIMER);
    lapic_write(LAPIC_TIMER_INIT, count);
}

/**
 * @funcname `lapic_timer_oneshot`
 *
 * @description
 * Starts the local APIC timer counting down once from count, without an
 * interrupt (used to calibrate it).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `count` | `uint32_t` | [in] The initial count. |
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
 * 1. Call `lapic_write()` with the arguments `LAPIC_LVT_TIMER` and
 *    `LAPIC_LVT_MASKED | HV_VECTOR_TIMER`.
 * 2. Call `lapic_write()` with the arguments `LAPIC_TIMER_INIT` and `count`.
 *
 * @calls
 * - `lapic_write()`
 */
void lapic_timer_oneshot(uint32_t count)
{
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED | HV_VECTOR_TIMER);
    lapic_write(LAPIC_TIMER_INIT, count);
}

/**
 * @funcname `lapic_timer_stop`
 *
 * @description
 * Stops the local APIC timer of this CPU.
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
 * 1. Call `lapic_write()` with the arguments `LAPIC_LVT_TIMER` and
 *    `LAPIC_LVT_MASKED | HV_VECTOR_TIMER`.
 * 2. Call `lapic_write()` with the arguments `LAPIC_TIMER_INIT` and `0U`.
 *
 * @calls
 * - `lapic_write()`
 */
void lapic_timer_stop(void)
{
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASKED | HV_VECTOR_TIMER);
    lapic_write(LAPIC_TIMER_INIT, 0U);
}

/**
 * @funcname `lapic_timer_current`
 *
 * @description
 * Returns the current count of the local APIC timer of this CPU.
 *
 * @return The current count.
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
 * 1. Call `lapic_read()` with the argument `LAPIC_TIMER_CURRENT`, and return
 *    its result.
 *
 * @calls
 * - `lapic_read()`
 */
uint32_t lapic_timer_current(void)
{
    return lapic_read(LAPIC_TIMER_CURRENT);
}

/**
 * @funcname `lapic_send_ipi`
 *
 * @description
 * Sends an interprocessor interrupt to a CPU and waits until it is
 * delivered.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `apic_id` | `uint32_t` | [in] The destination's local APIC ID. |
 * | `command` | `uint32_t` | [in] The low word of the ICR: delivery mode, level, vector. |
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
 * 1. Call `lapic_write()` with the arguments `LAPIC_ICR_HIGH` and
 *    `apic_id << 24`.
 * 2. Call `lapic_write()` with the arguments `LAPIC_ICR_LOW` and `command`.
 * 3. While `(lapic_read(LAPIC_ICR_LOW) & LAPIC_ICR_PENDING)` is not equal to
 *    `0U`:
 *    1. Call `x86_pause()`.
 *
 * @calls
 * - `lapic_write()`
 * - `lapic_read()`
 * - `x86_pause()`
 */
static void lapic_send_ipi(uint32_t apic_id, uint32_t command)
{
    // The destination goes first: writing the low word sends the IPI.
    lapic_write(LAPIC_ICR_HIGH, apic_id << 24);
    lapic_write(LAPIC_ICR_LOW, command);
    while ((lapic_read(LAPIC_ICR_LOW) & LAPIC_ICR_PENDING) != 0U) {
        x86_pause();
    }
}

/**
 * @funcname `lapic_send_init`
 *
 * @description
 * Sends INIT to a CPU, to start the INIT-SIPI-SIPI sequence.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `apic_id` | `uint32_t` | [in] The CPU's local APIC ID. |
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
 * 1. Call `lapic_send_ipi()` with the arguments `apic_id` and `LAPIC_ICR_INIT`.
 *
 * @calls
 * - `lapic_send_ipi()`
 */
void lapic_send_init(uint32_t apic_id)
{
    lapic_send_ipi(apic_id, LAPIC_ICR_INIT);
}

/**
 * @funcname `lapic_send_sipi`
 *
 * @description
 * Sends a start-up IPI to a CPU: it starts in real mode at vector * 4 KiB.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `apic_id` | `uint32_t` | [in] The CPU's local APIC ID. |
 * | `vector` | `uint8_t` | [in] The start page (address / 4 KiB). |
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
 * 1. Call `lapic_send_ipi()` with the arguments `apic_id` and
 *    `LAPIC_ICR_STARTUP | (uint32_t) vector`.
 *
 * @calls
 * - `lapic_send_ipi()`
 */
void lapic_send_sipi(uint32_t apic_id, uint8_t vector)
{
    lapic_send_ipi(apic_id, LAPIC_ICR_STARTUP | (uint32_t) vector);
}
