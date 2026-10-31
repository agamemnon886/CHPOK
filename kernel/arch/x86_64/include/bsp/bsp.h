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
 * kernel/arch/x86/include/bsp/bsp.h to x86_64 (64-bit).
 */

#ifndef __JA_X86_64_BSP_BSP_H__
#define __JA_X86_64_BSP_BSP_H__

#include <arch/interrupt_frame.h>

/* Initialize all board-specific stuff. */
void ja_bsp_init(void);

/*
 * Board-specific header should define:
 *
 *  - macro EXCEPTION_TIMER as integer constant
 *    It corresponds to interrupt index of the timer.
 *  - BOARD_RAM_SIZE: amount of RAM (starting at 0), identity-mapped
 *    for the kernel; a multiple of 2 MiB, at most 1 GiB.
 *  - BOARD_PARTITION_PHYS_START: beginning of RAM used for partitions.
 */
#include <board/bsp.h>

/* Called when interrupt from the timer occures. */
void ja_bsp_process_timer(interrupt_frame* frame);

#endif /* __JA_X86_64_BSP_BSP_H__ */
