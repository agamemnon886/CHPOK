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
 * Modified by Kirill Brazhnikov, 2026-11-08: for both architectures: the
 * lock uses the hv_arch_*() functions and the CPU's hardware number
 * (hv_arch_cpu_id()) instead of x86_64 instructions and the APIC ID. Doxygen
 * headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/src/console.c (the common code and its headers are in
 * hypervisor/core).
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
 */

/**
 * @file
 * The hypervisor's console: a serial port shared by all physical CPUs. A
 * message is written under a lock, which the CPU holding it may take again
 * (so that a fault while printing can still be reported).
 */

#include <hv/hv.h>

static uint16_t console_port = HV_PORT_NONE;

static volatile uint32_t console_locked = 0U;
static volatile uint32_t console_owner = 0U;    // CPU ID + 1, or 0
static uint32_t console_depth = 0U;

/**
 * @funcname `hv_console_init`
 *
 * @description
 * Uses a serial port as the hypervisor console, and initializes it. Called
 * once at boot.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `uint16_t` | [in] The serial port, or HV_PORT_NONE for no console. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `console_port` | `uint16_t` | The console's serial port |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE_HV_CONSOLE_INIT_01}: `hv_console_init()` shall make a
 *   serial port the console of the hypervisor and set it up, or have no
 *   console.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *   - \srqref{SRQ_HV_CON_007}
 *
 * @llr
 * - \llrid{LLR_CONSOLE_HV_CONSOLE_INIT_01}: `hv_console_init()` shall set
 *   `console_port` to `port`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_CONSOLE_INIT_01}
 * - \llrid{LLR_CONSOLE_HV_CONSOLE_INIT_02}: If and only if `port` is not
 *   `HV_PORT_NONE`, `hv_console_init()` shall call `uart_init()` once with
 *   `port`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_CONSOLE_INIT_01}
 *
 * @design
 * 1. Set `console_port` to `port`. \llrref{LLR_CONSOLE_HV_CONSOLE_INIT_01}
 * 2. If `port` is not equal to `HV_PORT_NONE`:
 *    \llrref{LLR_CONSOLE_HV_CONSOLE_INIT_02}
 *    1. Call `uart_init()` with the argument `port`.
 *
 * @calls
 * - `uart_init()`
 */
void hv_console_init(uint16_t port)
{
    console_port = port;
    if (port != HV_PORT_NONE) {
        uart_init(port);
    }
}

/**
 * @funcname `hv_console_lock`
 *
 * @description
 * Takes the console lock. The CPU that holds it may take it again (so that
 * a fault while printing can be reported); each call needs an
 * hv_console_unlock().
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `console_locked` | `volatile uint32_t` | The lock word: 1 if taken |
 * | `console_owner` | `volatile uint32_t` | The holder's CPU ID + 1, or 0 |
 * | `console_depth` | `uint32_t` | How many times the holder took the lock |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `me` | `uint32_t` | This CPU's ID + 1 |
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE_HV_CONSOLE_LOCK_01}: `hv_console_lock()` shall give one
 *   CPU at a time the console, the CPU that holds it being able to take it
 *   again to report a fatal error of its own.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_002}
 *   - \srqref{SRQ_HV_CON_008}
 *
 * @llr
 * - \llrid{LLR_CONSOLE_HV_CONSOLE_LOCK_01}: If `console_owner` is the result of
 *   `hv_arch_cpu_id()` plus 1, `hv_console_lock()` shall increment
 *   `console_depth`, without calling `hv_arch_xchg32()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_CONSOLE_LOCK_01}
 * - \llrid{LLR_CONSOLE_HV_CONSOLE_LOCK_02}: If `console_owner` is not the
 *   result of `hv_arch_cpu_id()` plus 1, `hv_console_lock()` shall call
 *   `hv_arch_xchg32()` with the address of `console_locked` and 1 until it
 *   returns 0, calling `hv_arch_relax()` once after each other result, and then
 *   set `console_owner` to the result of `hv_arch_cpu_id()` plus 1 and
 *   `console_depth` to 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_CONSOLE_LOCK_01}
 *
 * @design
 * 1. Set `me` to `hv_arch_cpu_id() + 1U`.
 *    \llrref{LLR_CONSOLE_HV_CONSOLE_LOCK_01},
 *    \llrref{LLR_CONSOLE_HV_CONSOLE_LOCK_02}
 * 2. If `console_owner` is equal to `me`:
 *    \llrref{LLR_CONSOLE_HV_CONSOLE_LOCK_01}
 *    1. Increment `console_depth`.
 * 3. Otherwise: \llrref{LLR_CONSOLE_HV_CONSOLE_LOCK_02}
 *    1. While `hv_arch_xchg32(&console_locked, 1U)` is not equal to `0U`:
 *       1. Call `hv_arch_relax()`.
 *    2. Set `console_owner` to `me`.
 *    3. Set `console_depth` to `1U`.
 *
 * @calls
 * - `hv_arch_cpu_id()`
 * - `hv_arch_xchg32()`
 * - `hv_arch_relax()`
 */
void hv_console_lock(void)
{
    uint32_t me = hv_arch_cpu_id() + 1U;

    if (console_owner == me) {
        // Taken again by the holder (a fault while printing).
        console_depth++;
    } else {
        // Spin until the lock word changes from 0 (free) to 1 for us.
        while (hv_arch_xchg32(&console_locked, 1U) != 0U) {
            hv_arch_relax();
        }
        console_owner = me;
        console_depth = 1U;
    }
}

/**
 * @funcname `hv_console_unlock`
 *
 * @description
 * Releases the console lock taken by hv_console_lock(): the lock is free
 * when the holder has released it as many times as it took it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `console_locked` | `volatile uint32_t` | The lock word: 1 if taken |
 * | `console_owner` | `volatile uint32_t` | The holder's CPU ID + 1, or 0 |
 * | `console_depth` | `uint32_t` | How many times the holder took the lock |
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE_HV_CONSOLE_UNLOCK_01}: `hv_console_unlock()` shall
 *   release the console when its holder has released it as many times as it
 *   took it.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_002}
 *
 * @llr
 * - \llrid{LLR_CONSOLE_HV_CONSOLE_UNLOCK_01}: `hv_console_unlock()` shall
 *   decrement `console_depth`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_CONSOLE_UNLOCK_01}
 * - \llrid{LLR_CONSOLE_HV_CONSOLE_UNLOCK_02}: If and only if `console_depth` is
 *   then 0, `hv_console_unlock()` shall set `console_owner` to 0, call
 *   `hv_arch_barrier()` once and then set `console_locked` to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_CONSOLE_UNLOCK_01}
 *
 * @design
 * 1. Decrement `console_depth`. \llrref{LLR_CONSOLE_HV_CONSOLE_UNLOCK_01}
 * 2. If `console_depth` is equal to `0U`:
 *    \llrref{LLR_CONSOLE_HV_CONSOLE_UNLOCK_02}
 *    1. Set `console_owner` to `0U`.
 *    2. Call `hv_arch_barrier()`.
 *    3. Set `console_locked` to `0U`.
 *
 * @calls
 * - `hv_arch_barrier()`
 */
void hv_console_unlock(void)
{
    console_depth--;
    if (console_depth == 0U) {
        // The writes under the lock must be done before it is free.
        console_owner = 0U;
        hv_arch_barrier();
        console_locked = 0U;
    }
}

/**
 * @funcname `hv_console_write`
 *
 * @description
 * Writes characters to the console, with a carriage return before each line
 * feed. The caller holds the console lock.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `s` | `const char *` | [in] The characters. |
 * | `length` | `size_t` | [in] The number of characters. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `console_port` | `uint16_t` | The console's serial port |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the character |
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE_HV_CONSOLE_WRITE_01}: `hv_console_write()` shall write
 *   characters to the console of the hypervisor, with a carriage return before
 *   each line feed, and drop them without a console.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_001}
 *   - \srqref{SRQ_HV_CON_007}
 *   - \srqref{SRQ_HV_CON_002}
 *
 * @llr
 * - \llrid{LLR_CONSOLE_HV_CONSOLE_WRITE_01}: If `console_port` is not
 *   `HV_PORT_NONE`, `hv_console_write()` shall call `uart_putc()` with
 *   `console_port` and each of the `length` characters of `s`, in their order.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_CONSOLE_WRITE_01}
 * - \llrid{LLR_CONSOLE_HV_CONSOLE_WRITE_02}: If `console_port` is not
 *   `HV_PORT_NONE`, `hv_console_write()` shall call `uart_putc()` once with
 *   `console_port` and `'\r'` before the call for a character of `s` if and
 *   only if that character is `'\n'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_CONSOLE_WRITE_01}
 * - \llrid{LLR_CONSOLE_HV_CONSOLE_WRITE_03}: If `console_port` is
 *   `HV_PORT_NONE`, `hv_console_write()` shall return without calling
 *   `uart_putc()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_CONSOLE_WRITE_01}
 *
 * @design
 * 1. If `console_port` is not equal to `HV_PORT_NONE`:
 *    \llrref{LLR_CONSOLE_HV_CONSOLE_WRITE_01},
 *    \llrref{LLR_CONSOLE_HV_CONSOLE_WRITE_02},
 *    \llrref{LLR_CONSOLE_HV_CONSOLE_WRITE_03}
 *    1. Set `i` to `0U`; then while `i` is less than `length`, repeat (after
 *       each pass: increment `i`): \llrref{LLR_CONSOLE_HV_CONSOLE_WRITE_01},
 *       \llrref{LLR_CONSOLE_HV_CONSOLE_WRITE_02}
 *       1. If `s[i]` is equal to `'\n'`:
 *          \llrref{LLR_CONSOLE_HV_CONSOLE_WRITE_02}
 *          1. Call `uart_putc()` with the arguments `console_port` and `'\r'`.
 *       2. Call `uart_putc()` with the arguments `console_port` and `s[i]`.
 *          \llrref{LLR_CONSOLE_HV_CONSOLE_WRITE_01}
 *
 * @calls
 * - `uart_putc()`
 */
void hv_console_write(const char *s, size_t length)
{
    if (console_port != HV_PORT_NONE) {
        for (size_t i = 0U; i < length; i++) {
            if (s[i] == '\n') {
                uart_putc(console_port, '\r');
            }
            uart_putc(console_port, s[i]);
        }
    }
}

/**
 * @funcname `hv_fatal`
 *
 * @description
 * Reports a fatal error of the hypervisor on this CPU and halts the CPU.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message` | `const char *` | [in] What went wrong. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_CONSOLE_HV_FATAL_01}: `hv_fatal()` shall report a fatal error of
 *   the hypervisor with the number of the CPU and halt that CPU.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_ERR_003}
 *
 * @llr
 * - \llrid{LLR_CONSOLE_HV_FATAL_01}: `hv_fatal()` shall call, in this order,
 *   `hv_arch_irq_disable()`, `hv_printf()` with `"[hv] fatal error on CPU %%u:
 *   %%s\n"`, the result of `hv_arch_cpu_id()` and `message`, and
 *   `hv_halt_forever()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CONSOLE_HV_FATAL_01}
 *
 * @design
 * 1. Call `hv_arch_irq_disable()`. \llrref{LLR_CONSOLE_HV_FATAL_01}
 * 2. Call `hv_printf()` with the arguments
 *    `"[hv] fatal error on CPU %%u: %%s\n"`, `hv_arch_cpu_id()` and `message`.
 *    \llrref{LLR_CONSOLE_HV_FATAL_01}
 * 3. Call `hv_halt_forever()`. \llrref{LLR_CONSOLE_HV_FATAL_01}
 *
 * @calls
 * - `hv_arch_irq_disable()`
 * - `hv_printf()`
 * - `hv_arch_cpu_id()`
 * - `hv_halt_forever()`
 */
void hv_fatal(const char *message)
{
    hv_arch_irq_disable();
    hv_printf("[hv] fatal error on CPU %u: %s\n", hv_arch_cpu_id(),
              message);
    hv_halt_forever();
}
