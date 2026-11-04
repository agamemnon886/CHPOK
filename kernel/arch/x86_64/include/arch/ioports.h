/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/include/arch/ioports.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; deviations are documented where a
 * guideline can't be followed.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef POK_X86_64_IOPORTS_H
#define POK_X86_64_IOPORTS_H
#include <stdint.h>

#define JET_ARCH_DECLARE_IO_PORT 1

/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (this header): the I/O port
 * instructions are GNU inline assembly, isolated in these macros; the
 * reading macros are GNU statement expressions, so that they yield the
 * value read.
 */

#define ja_outb(port, data)                                        \
  asm volatile ("outb %b0,%w1"                                  \
                :						\
		:"a" (data),"d" (port))

#define ja_inb(port)                                               \
({                                                              \
  uint8_t res;                                            \
  asm volatile ("inb %w1,%0"                                    \
                :"=a" (res)                                     \
                :"d" (port));                                   \
  res;                                                          \
})

#define ja_outw(port, data)                                        \
  asm volatile ("outw %0,%w1"					\
                :						\
		:"a" (data),"d" (port))

#define ja_inw(port)                                               \
({                                                              \
  uint16_t res;						\
  asm volatile ("inw %w1,%0"					\
                :"=a" (res)                                     \
                :"d" (port));					\
  res;                                                          \
})

#define ja_outl(port, data)                                        \
  asm volatile ("outl %k0,%w1"					\
                :						\
		:"a" (data),"d" (port))

#define ja_inl(port)                                               \
({                                                              \
  uint32_t res;						\
  asm volatile ("inl %w1,%0"					\
                :"=a" (res)                                     \
                :"d" (port));					\
  res;                                                          \
})

#endif /* POK_X86_64_IOPORTS_H */
