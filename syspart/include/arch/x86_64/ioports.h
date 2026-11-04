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
 */


#ifndef POK_SYSPART_X86_64_IOPORTS_H
#define POK_SYSPART_X86_64_IOPORTS_H
#include <stdint.h>

#define outb(port, data)                                        \
  asm volatile ("outb %b0,%w1"                                  \
                :						\
		:"a" (data),"d" (port))

#define inb(port)                                               \
({                                                              \
  uint8_t res;                                            \
  asm volatile ("inb %w1,%0"                                    \
                :"=a" (res)                                     \
                :"d" (port));                                   \
  res;                                                          \
})

#define outw(port, data)                                        \
  asm volatile ("outw %w0,%w1"					\
                :						\
		:"a" (data),"d" (port))

#define inw(port)                                               \
({                                                              \
  uint16_t res;						\
  asm volatile ("inw %w1,%0"					\
                :"=a" (res)                                     \
                :"d" (port));					\
  res;                                                          \
})

#define outl(port, data)                                        \
  asm volatile ("outl %k0,%w1"					\
                :						\
		:"a" (data),"d" (port))

#define inl(port)                                               \
({                                                              \
  uint32_t res;						\
  asm volatile ("inl %w1,%0"					\
                :"=a" (res)                                     \
                :"d" (port));					\
  res;                                                          \
})

#endif /* POK_SYSPART_X86_64_IOPORTS_H */
