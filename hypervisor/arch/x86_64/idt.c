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
 * structure.
 */

/*
 * Interrupt descriptor table of the hypervisor, shared by all physical
 * CPUs, and the C part of the interrupt handling (stubs in isr.S).
 *
 * The local APIC timer counts ticks for the virtual PIT of the VM which
 * runs on the CPU. Exceptions in the hypervisor are fatal.
 */

#include <arch/internal.h>

#define IDT_ENTRIES         256U
#define ISR_STUB_SIZE       16U
#define GDT_CODE64          0x08U
#define IDT_INTERRUPT_GATE  0x8EU   // Present, DPL 0, 64-bit interrupt
#define EXCEPTION_VECTORS   32U
#define LEGACY_PIC_FIRST    0x20U
#define LEGACY_PIC_LAST     0x2FU

// Registers saved by isr.S, then the CPU's interrupt frame.
typedef struct {
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} hv_int_frame_t;

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} idt_gate_t;

// Operand of lidt.
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idt_descriptor_t;

static idt_gate_t idt[IDT_ENTRIES] HV_ALIGNED(16);

extern const uint8_t hv_isr_stubs[];

void hv_interrupt(hv_int_frame_t *frame);

/*
 * Fill the IDT: every vector goes to its stub in isr.S (interrupt gates, ring
 * 0).
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

// Load the IDT on this CPU.
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

// Read CR2 (the address of the last page fault).
static uint64_t read_cr2(void)
{
    uint64_t value;

    asm volatile ("mov %%cr2, %0" : "=r" (value));
    return value;
}

/*
 * Handle an interrupt or exception of the hypervisor (called by isr.S with the
 * saved registers): count timer ticks, ignore spurious interrupts, stop on
 * exceptions.
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
