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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/include/bsp/bsp.h for arm64.
 */

/* Interface between arm64 arch code and the board. */

#ifndef __JA_ARM64_BSP_BSP_H__
#define __JA_ARM64_BSP_BSP_H__

#include <arch/interrupt_frame.h>

/*
 * Board-specific header should define:
 *
 *  - BOARD_DEVICE_BASE, BOARD_DEVICE_SIZE: device (MMIO) region,
 *  - BOARD_RAM_BASE, BOARD_RAM_SIZE: RAM region,
 *    both 1 GiB aligned and identity-mapped for the kernel,
 *  - BOARD_PARTITION_PHYS_START: beginning of RAM used for partitions.
 */
#include <board/bsp.h>

/* Initialize all board-specific stuff. Called with the MMU enabled. */
void ja_bsp_init(void);

/* Called when an IRQ occures (interrupts are disabled). */
void ja_bsp_process_irq(interrupt_frame* frame);

/* Reset or power off the board. Never returns. */
void ja_bsp_reset(void);

#endif /* __JA_ARM64_BSP_BSP_H__ */
