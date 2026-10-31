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
 * kernel/arch/x86/tss.h to x86_64 (64-bit).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef __POK_X86_64_TSS_H__
#define __POK_X86_64_TSS_H__

#include <types.h>

/* 64-bit task state segment: only stack pointers are used. */
typedef struct
{
   uint32_t reserved0;
   uint64_t rsp0;
   uint64_t rsp1;
   uint64_t rsp2;
   uint64_t reserved1;
   uint64_t ist[7];
   uint64_t reserved2;
   uint16_t reserved3;
   uint16_t iomap_base;
} __attribute__((packed)) tss_t;

#endif /* !__POK_X86_64_TSS_H__ */
