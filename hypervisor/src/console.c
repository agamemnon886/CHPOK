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
 * (hv_arch_cpu_id()) instead of x86_64 instructions and the APIC ID.
 */

/*
 * The hypervisor's console: a serial port shared by all physical CPUs.
 * A message is written under a lock, which the CPU holding it may take
 * again (so that a fault while printing can still be reported).
 */

#include <hv/hv.h>

static uint16_t console_port = HV_PORT_NONE;

static volatile uint32_t console_locked = 0U;
static volatile uint32_t console_owner = 0U;    // CPU ID + 1, or 0
static uint32_t console_depth = 0U;

/*
 * Use the serial port at port (or none, HV_PORT_NONE) as the hypervisor
 * console.
 */
void hv_console_init(uint16_t port)
{
    console_port = port;
    if (port != HV_PORT_NONE) {
        uart_init(port);
    }
}

/*
 * Take the console lock. The CPU that holds it may take it again (so that a
 * fault while printing can be reported); each call needs an
 * hv_console_unlock().
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

// Release the console lock taken by hv_console_lock().
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

/*
 * Write length characters to the console, with CR before each LF. The caller
 * holds the console lock.
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

// Report a fatal error of the hypervisor on this CPU and halt the CPU.
void hv_fatal(const char *message)
{
    hv_arch_irq_disable();
    hv_printf("[hv] fatal error on CPU %u: %s\n", hv_arch_cpu_id(),
              message);
    hv_halt_forever();
}
