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
 * x86 I/O port instructions, for the kernel (through the ja_inb()...
 * macros of kernel/csps/x86_64/include/arch/ioports.h) and the hypervisor.
 *
 * The functions were in hypervisor/csps/x86_64/include/arch/x86.h; the
 * kernel had its own macros with the instructions.
 */

#ifndef JET_CPU_IO_H
#define JET_CPU_IO_H

#include <stdint.h>

/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (this header): the I/O port
 * instructions are GNU inline assembly, isolated in these small static
 * inline functions.
 */

/*
 * Writes a byte to an I/O port.
 */
static inline void x86_outb(uint16_t port, uint8_t value)
{
    asm volatile ("outb %0, %1" : : "a" (value), "Nd" (port) : "memory");
}

/*
 * Writes a 16-bit word to an I/O port.
 */
static inline void x86_outw(uint16_t port, uint16_t value)
{
    asm volatile ("outw %0, %1" : : "a" (value), "Nd" (port) : "memory");
}

/*
 * Writes a 32-bit word to an I/O port.
 */
static inline void x86_outl(uint16_t port, uint32_t value)
{
    asm volatile ("outl %0, %1" : : "a" (value), "Nd" (port) : "memory");
}

/*
 * Reads a byte from an I/O port.
 */
static inline uint8_t x86_inb(uint16_t port)
{
    uint8_t value;

    asm volatile ("inb %1, %0" : "=a" (value) : "Nd" (port) : "memory");
    return value;
}

/*
 * Reads a 16-bit word from an I/O port.
 */
static inline uint16_t x86_inw(uint16_t port)
{
    uint16_t value;

    asm volatile ("inw %1, %0" : "=a" (value) : "Nd" (port) : "memory");
    return value;
}

/*
 * Reads a 32-bit word from an I/O port.
 */
static inline uint32_t x86_inl(uint16_t port)
{
    uint32_t value;

    asm volatile ("inl %1, %0" : "=a" (value) : "Nd" (port) : "memory");
    return value;
}

#endif /* JET_CPU_IO_H */
