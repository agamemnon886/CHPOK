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
 * arch/x86_64/; the timer tick count is in the arch part of the CPU
 * structure. Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and external
 * declarations are in arch/internal.h, documented there.
 */

/**
 * @file
 * Interrupt descriptor table of the hypervisor, shared by all physical CPUs,
 * and the C part of the interrupt handling (stubs in isr.S). The local APIC
 * timer counts ticks for the virtual PIT of the VM which runs on the CPU.
 * Exceptions in the hypervisor are fatal.
 */

#include <arch/internal.h>

static idt_gate_t idt[IDT_ENTRIES] HV_ALIGNED(16);

/**
 * @funcname `hv_idt_init`
 *
 * @description
 * Fills the IDT: every vector goes to its stub in isr.S, through a 64-bit
 * interrupt gate of ring 0. Called once by the boot CPU.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `hv_isr_stubs` | `const uint8_t[]` | The interrupt stubs (isr.S), 16 bytes each |
 * | `idt` | `idt_gate_t[256]` | The interrupt descriptor table |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stubs` | `uintptr_t` | The address of the first stub |
 * | `vector` | `uint32_t` | The vector |
 * | `handler` | `uint64_t` | The address of the vector's stub |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `stubs` to `(uintptr_t) hv_isr_stubs`.
 * 2. Set `vector` to `0U`; then while `vector` is less than `IDT_ENTRIES`,
 *    repeat (after each pass: increment `vector`):
 *    1. Set `handler` to `stubs + ((uintptr_t) vector * ISR_STUB_SIZE)`.
 *    2. Set `idt[vector].offset_low` to `(uint16_t) handler`.
 *    3. Set `idt[vector].selector` to `(uint16_t) GDT_CODE64`.
 *    4. Set `idt[vector].ist` to `0U`.
 *    5. Set `idt[vector].type` to `(uint8_t) IDT_INTERRUPT_GATE`.
 *    6. Set `idt[vector].offset_mid` to `(uint16_t) (handler >> 16)`.
 *    7. Set `idt[vector].offset_high` to `(uint32_t) (handler >> 32)`.
 *    8. Set `idt[vector].reserved` to `0U`.
 *
 * @calls
 * None.
 */
void hv_idt_init(void)
{
    /*
     * MISRA C:2025 Rule 11.4 deviation: the gates hold the addresses of
     * the stubs as integers.
     */
    uintptr_t stubs = (uintptr_t) hv_isr_stubs;

    for (uint32_t vector = 0U; vector < IDT_ENTRIES; vector++) {
        uint64_t handler = stubs + ((uintptr_t) vector * ISR_STUB_SIZE);

        // The handler address is split into three parts of the gate.
        idt[vector].offset_low = (uint16_t) handler;
        idt[vector].selector = (uint16_t) GDT_CODE64;
        idt[vector].ist = 0U;
        idt[vector].type = (uint8_t) IDT_INTERRUPT_GATE;
        idt[vector].offset_mid = (uint16_t) (handler >> 16);
        idt[vector].offset_high = (uint32_t) (handler >> 32);
        idt[vector].reserved = 0U;
    }
}

/**
 * @funcname `hv_idt_load`
 *
 * @description
 * Loads the IDT on this CPU.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `idt` | `idt_gate_t[256]` | The interrupt descriptor table |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `desc` | `idt_descriptor_t` | The operand of lidt: limit and base |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `desc.limit` to `(uint16_t) (sizeof(idt) - 1U)`.
 * 2. Set `desc.base` to `(uint64_t) (uintptr_t) idt`.
 * 3. Execute the assembly code `lidt %0`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (hv_idt_load()): lidt is
 * GNU inline assembly; the packed descriptor is a GNU attribute (above).
 * MISRA C:2025 Rule 11.4 deviation: the descriptor holds the address of
 * the table.
 */
void hv_idt_load(void)
{
    idt_descriptor_t desc;

    desc.limit = (uint16_t) (sizeof(idt) - 1U);
    desc.base = (uint64_t) (uintptr_t) idt;
    asm volatile ("lidt %0" : : "m" (desc));
}

/**
 * @funcname `read_cr2`
 *
 * @description
 * Reads CR2, the address of the last page fault.
 *
 * @return The value of CR2.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | The value read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `mov %%%cr2, %0`.
 * 2. Return `value`.
 *
 * @calls
 * None.
 */
static uint64_t read_cr2(void)
{
    uint64_t value;

    asm volatile ("mov %%cr2, %0" : "=r" (value));
    return value;
}

/**
 * @funcname `hv_interrupt`
 *
 * @description
 * Handles an interrupt or exception of the hypervisor, called by isr.S with
 * the saved registers: counts timer ticks, ignores spurious interrupts, and
 * stops on exceptions.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `hv_int_frame_t *` | [in] The saved registers, the vector and the CPU's interrupt frame. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vector` | `uint64_t` | The vector |
 * | `cpu` | `hv_cpu_t *` | This CPU |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `vector` to `frame->vector`.
 * 2. If `vector` is equal to `HV_VECTOR_TIMER`:
 *    1. Call `hv_this_cpu()`, and store the result in `cpu`.
 *    2. Increment `cpu->arch.timer_ticks`.
 *    3. Call `lapic_eoi()`.
 * 3. Otherwise, if `vector` is equal to `HV_VECTOR_SPURIOUS`:
 *    1. Do nothing.
 * 4. Otherwise, if `vector` is greater than or equal to `LEGACY_PIC_FIRST` and
 *    `vector` is less than or equal to `LEGACY_PIC_LAST`:
 *    1. Do nothing.
 * 5. Otherwise, if `vector` is less than `EXCEPTION_VECTORS`:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] exception %%lu, error code 0x%%lx, at RIP 0x%%lx, "`
 *       `"RSP 0x%%lx, CR2 0x%%lx\n"`, `vector`, `frame->error_code`,
 *       `frame->rip`, `frame->rsp` and `read_cr2()`.
 *    2. Call `hv_fatal()` with the argument `"exception in the hypervisor"`.
 * 6. Otherwise:
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] unexpected interrupt %%lu\n"` and `vector`.
 *    2. Call `lapic_eoi()`.
 *
 * @calls
 * - `hv_this_cpu()`
 * - `lapic_eoi()`
 * - `hv_printf()`
 * - `read_cr2()`
 * - `hv_fatal()`
 */
void hv_interrupt(hv_int_frame_t *frame)
{
    uint64_t vector = frame->vector;

    if (vector == HV_VECTOR_TIMER) {
        // A tick of this CPU's VM's virtual PIT, used by svm.c.
        hv_cpu_t *cpu = hv_this_cpu();

        cpu->arch.timer_ticks++;
        lapic_eoi();
    } else if (vector == HV_VECTOR_SPURIOUS) {
        // No end of interrupt for spurious interrupts.
    } else if ((vector >= LEGACY_PIC_FIRST) && (vector <= LEGACY_PIC_LAST)) {
        // Spurious interrupt of the masked legacy PIC.
    } else if (vector < EXCEPTION_VECTORS) {
        hv_printf("[hv] exception %lu, error code 0x%lx, at RIP 0x%lx, "
                  "RSP 0x%lx, CR2 0x%lx\n", vector, frame->error_code,
                  frame->rip, frame->rsp, read_cr2());
        hv_fatal("exception in the hypervisor");
    } else {
        hv_printf("[hv] unexpected interrupt %lu\n", vector);
        lapic_eoi();
    }
}
