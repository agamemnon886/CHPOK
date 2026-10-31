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
 * boards/x86-qemu/include/board/bsp.h to x86_64 (64-bit).
 */

#ifndef __JET_X86_64_QEMU_BSP_H__
#define __JET_X86_64_QEMU_BSP_H__

#define PIT_IRQ 0
#define EXCEPTION_TIMER (PIT_IRQ + 32)

/* RAM, as given with '-m 1G'. The kernel is loaded at 1 MiB. */
#define BOARD_RAM_SIZE 0x40000000ULL

/*
 * Partitions' memory. Below it: the kernel image (with partitions'
 * ELFs embedded) and the kernel heap.
 */
#define BOARD_PARTITION_PHYS_START 0x1000000ULL

#endif /* __JET_X86_64_QEMU_BSP_H__ */
