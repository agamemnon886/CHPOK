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
 * x86_64 instructions used by the hypervisor: port I/O, MSRs, CPUID, the
 * TSC, interrupt flag and halt, and an atomic exchange for locks.
 */

/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (this file): privileged
 * instructions are written as GNU inline assembly, each one isolated in a
 * small static inline function.
 */

#ifndef HV_X86_H
#define HV_X86_H

#include <stdint.h>
#include <stdbool.h>

#define X86_RFLAGS_IF   (1UL << 9)

#define X86_CR0_PE      (1UL << 0)
#define X86_CR0_ET      (1UL << 4)

#define MSR_TSC                 0x00000010U
#define MSR_APIC_BASE           0x0000001BU
#define MSR_SYSENTER_CS         0x00000174U
#define MSR_SYSENTER_ESP        0x00000175U
#define MSR_SYSENTER_EIP        0x00000176U
#define MSR_PAT                 0x00000277U
#define MSR_EFER                0xC0000080U
#define MSR_STAR                0xC0000081U
#define MSR_LSTAR               0xC0000082U
#define MSR_CSTAR               0xC0000083U
#define MSR_SFMASK              0xC0000084U
#define MSR_FS_BASE             0xC0000100U
#define MSR_GS_BASE             0xC0000101U
#define MSR_KERNEL_GS_BASE      0xC0000102U
#define MSR_VM_CR               0xC0010114U
#define MSR_VM_HSAVE_PA         0xC0010117U

#define EFER_SVME               (1UL << 12)
#define VM_CR_SVMDIS            (1UL << 4)

// Write a byte to an I/O port.
static inline void x86_outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %0, %1" : : "a" (value), "Nd" (port) : "memory");
}

// Write a 16-bit word to an I/O port.
static inline void x86_outw(uint16_t port, uint16_t value)
{
    asm volatile ("outw %0, %1" : : "a" (value), "Nd" (port) : "memory");
}

// Write a 32-bit word to an I/O port.
static inline void x86_outl(uint16_t port, uint32_t value)
{
    asm volatile ("outl %0, %1" : : "a" (value), "Nd" (port) : "memory");
}

// Read a byte from an I/O port.
static inline uint8_t x86_inb(uint16_t port)
{
    uint8_t value;

    asm volatile ("inb %1, %0" : "=a" (value) : "Nd" (port) : "memory");
    return value;
}

// Read a 16-bit word from an I/O port.
static inline uint16_t x86_inw(uint16_t port)
{
    uint16_t value;

    asm volatile ("inw %1, %0" : "=a" (value) : "Nd" (port) : "memory");
    return value;
}

// Read a 32-bit word from an I/O port.
static inline uint32_t x86_inl(uint16_t port)
{
    uint32_t value;

    asm volatile ("inl %1, %0" : "=a" (value) : "Nd" (port) : "memory");
    return value;
}

// Read a model-specific register.
static inline uint64_t x86_rdmsr(uint32_t msr)
{
    uint32_t low;
    uint32_t high;

    asm volatile ("rdmsr" : "=a" (low), "=d" (high) : "c" (msr));
    return ((uint64_t) high << 32) | (uint64_t) low;
}

// Write a model-specific register.
static inline void x86_wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t low = (uint32_t) value;
    uint32_t high = (uint32_t) (value >> 32);

    asm volatile ("wrmsr" : : "c" (msr), "a" (low), "d" (high)
                  : "memory");
}

// CPUID of leaf and subleaf: regs[0..3] are EAX, EBX, ECX, EDX.
static inline void x86_cpuid(uint32_t leaf, uint32_t subleaf,
                             uint32_t regs[4])
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;

    asm volatile ("cpuid"
                  : "=a" (a), "=b" (b), "=c" (c), "=d" (d)
                  : "a" (leaf), "c" (subleaf));
    regs[0] = a;
    regs[1] = b;
    regs[2] = c;
    regs[3] = d;
}

// Read the time stamp counter.
static inline uint64_t x86_rdtsc(void)
{
    uint32_t low;
    uint32_t high;

    asm volatile ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t) high << 32) | (uint64_t) low;
}

// Disable interrupts.
static inline void x86_cli(void)
{
    asm volatile ("cli" : : : "memory");
}

/*
 * Halt until an interrupt arrives, let it be handled, then disable
 * interrupts again (sti's shadow covers hlt, so no interrupt is missed).
 */
static inline void x86_wait_interrupt(void)
{
    asm volatile ("sti; hlt; cli" : : : "memory");
}

// Halt until the next interrupt (or forever, with interrupts disabled).
static inline void x86_hlt(void)
{
    asm volatile ("hlt" : : : "memory");
}

// Spin-wait hint: lets the other logical CPU (or QEMU) run.
static inline void x86_pause(void)
{
    asm volatile ("pause" : : : "memory");
}

// Atomic exchange of a 32-bit word; returns the old value (spin locks).
static inline uint32_t x86_xchg32(volatile uint32_t *word, uint32_t value)
{
    uint32_t old = value;

    asm volatile ("xchgl %0, %1" : "+r" (old), "+m" (*word) : : "memory");
    return old;
}

// Compiler barrier: memory accesses are not moved across it.
static inline void x86_barrier(void)
{
    asm volatile ("" : : : "memory");
}

#endif // HV_X86_H
