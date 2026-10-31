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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/space.h for arm64.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef __POK_ARM64_SPACE_H__
#define __POK_ARM64_SPACE_H__

#include <types.h>
#include <arch/deployment.h>
#include <bsp/bsp.h>

/*
 * Virtual address where partition's memory starts (in every user space).
 *
 * Should coincide with 'kshd' in the partition's ldscript.
 *
 * Lower virtual addresses are occupied by the kernel's identity
 * mapping (devices and RAM), which is accessible only from EL1.
 */
#define POK_PARTITION_MEMORY_BASE 0x100000000ULL

/* Maximum size of the memory of one partition (one level-1 entry). */
#define POK_PARTITION_MEMORY_MAX_SIZE 0x40000000ULL

/*
 * Beginning of the phys memory used for partitions.
 */
#define POK_PARTITION_MEMORY_PHYS_START BOARD_PARTITION_PHYS_START

/* Build translation tables for all spaces and enable the MMU. */
void ja_space_init(void);

#endif /* !__POK_ARM64_SPACE_H__ */
