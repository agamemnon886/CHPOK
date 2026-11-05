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
 * kernel/arch/x86/space.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-04: struct jet_fp_store is defined
 * here (it was private to space.c), for the GDB server.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * \file    arch/x86_64/space.h
 */

#ifndef POK_X86_64_SPACE_H
#define POK_X86_64_SPACE_H

#include <types.h>
#include "thread.h"
#include <arch/deployment.h>
#include <bsp/bsp.h>

/*
 * Virtual address where partition's memory starts (in every user space).
 *
 * Should coincide with 'kshd' in the partition's ldscript. Partitions
 * are below 2 GiB, as required by the default ('small') code model.
 *
 * Lower virtual addresses are occupied by the kernel's identity
 * mapping of RAM, which is not accessible from user space.
 */
#define POK_PARTITION_MEMORY_BASE 0x40000000ULL

/* Maximum size of the memory of one partition (one PDPT entry). */
#define POK_PARTITION_MEMORY_MAX_SIZE 0x40000000ULL

/*
 * Beginning of the phys memory used for partitions.
 */
#define POK_PARTITION_MEMORY_PHYS_START BOARD_PARTITION_PHYS_START

/* Build page tables for all spaces and switch to the kernel's ones. */
void ja_space_init(void);

/*
 * Area for fxsave64/fxrstor64: the x87, MMX and SSE registers of a user
 * thread (see ja_fp_save()). The GDB server reads and writes it too.
 *
 * MISRA C:2025 Dir 1.2 deviation: __attribute__((aligned)) is a GNU
 * extension; fxsave64 needs a 16-byte aligned area.
 */
struct jet_fp_store
{
  uint8_t data[512];
} __attribute__((aligned(16)));

#endif /* !POK_X86_64_SPACE_H */
