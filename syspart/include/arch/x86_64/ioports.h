/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * syspart/include/arch/x86/ioports.h to x86_64 (64-bit).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros and types are
 * documented.
 */


#ifndef POK_SYSPART_X86_64_IOPORTS_H
#define POK_SYSPART_X86_64_IOPORTS_H
#include <stdint.h>

/**
 * @macroname `outb`
 *
 * @description
 * Writes the byte `data` to the I/O port `port` (the outb instruction).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 * | `data` | The value written. |
 */
#define outb(port, data)                                        \
  asm volatile ("outb %b0,%w1"                                  \
                :						\
		:"a" (data),"d" (port))

/**
 * @macroname `inb`
 *
 * @description
 * Reads a byte from the I/O port `port` (the inb instruction): evaluates to it
 * (uint8_t).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 */
#define inb(port)                                               \
({                                                              \
  uint8_t res;                                            \
  asm volatile ("inb %w1,%0"                                    \
                :"=a" (res)                                     \
                :"d" (port));                                   \
  res;                                                          \
})

/**
 * @macroname `outw`
 *
 * @description
 * Writes the 16-bit word `data` to the I/O port `port` (the outw instruction).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 * | `data` | The value written. |
 */
#define outw(port, data)                                        \
  asm volatile ("outw %w0,%w1"					\
                :						\
		:"a" (data),"d" (port))

/**
 * @macroname `inw`
 *
 * @description
 * Reads a 16-bit word from the I/O port `port` (the inw instruction): evaluates
 * to it (uint16_t).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 */
#define inw(port)                                               \
({                                                              \
  uint16_t res;						\
  asm volatile ("inw %w1,%0"					\
                :"=a" (res)                                     \
                :"d" (port));					\
  res;                                                          \
})

/**
 * @macroname `outl`
 *
 * @description
 * Writes the 32-bit word `data` to the I/O port `port` (the outl instruction).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 * | `data` | The value written. |
 */
#define outl(port, data)                                        \
  asm volatile ("outl %k0,%w1"					\
                :						\
		:"a" (data),"d" (port))

/**
 * @macroname `inl`
 *
 * @description
 * Reads a 32-bit word from the I/O port `port` (the inl instruction): evaluates
 * to it (uint32_t).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 */
#define inl(port)                                               \
({                                                              \
  uint32_t res;						\
  asm volatile ("inl %w1,%0"					\
                :"=a" (res)                                     \
                :"d" (port));					\
  res;                                                          \
})

#endif /* POK_SYSPART_X86_64_IOPORTS_H */
