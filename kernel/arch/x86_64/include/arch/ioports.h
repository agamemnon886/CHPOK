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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef __POK_X86_64_IOPORTS_H__
#define __POK_X86_64_IOPORTS_H__

#define JET_ARCH_DECLARE_IO_PORT 1

#define ja_outb(port, data)                                        \
  asm volatile ("outb %b0,%w1"                                  \
                :						\
		:"a" (data),"d" (port))

#define ja_inb(port)                                               \
({                                                              \
  unsigned char res;                                            \
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
  unsigned short res;						\
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
  unsigned int res;						\
  asm volatile ("inl %w1,%0"					\
                :"=a" (res)                                     \
                :"d" (port));					\
  res;                                                          \
})

#endif /* __POK_X86_64_IOPORTS_H__ */
