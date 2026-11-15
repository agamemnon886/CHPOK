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
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros and the type of
 * boards/x86_64-qemu/cons.c are moved here; BOARD_RAM_SIZE and
 * BOARD_PARTITION_PHYS_START are documented once, with their x86_64 definitions
 * (Doxygen merged the two); the Doxygen comment of the file is removed (header
 * files are not documented).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/x86_64-qemu/include/board/bsp.h (the board support packages are in
 * kernel/bsps); the comments name the new paths.
 */

#ifndef JET_X86_64_QEMU_BSP_H
#define JET_X86_64_QEMU_BSP_H

#include <stdint.h>

/**
 * @macroname `PIT_IRQ`
 *
 * @description
 * The IRQ of the 8254 PIT (the system tick).
 */
#define PIT_IRQ 0U

/**
 * @macroname `EXCEPTION_TIMER`
 *
 * @description
 * The interrupt vector of the PIT (its IRQ after the 32 exception vectors,
 * as the PIC is programmed).
 */
#define EXCEPTION_TIMER (PIT_IRQ + 32U)

/**
 * @macroname `BOARD_RAM_SIZE`
 *
 * @description
 * The size of the RAM, as given with '-m 1G' (1 GiB, the same on arm64,
 * kernel/bsps/arm64-qemu/include/board/bsp.h). On x86_64, the kernel is
 * loaded at 1 MiB.
 */
#define BOARD_RAM_SIZE 0x40000000ULL

/**
 * @macroname `BOARD_PARTITION_PHYS_START`
 *
 * @description
 * Partitions' memory: its physical start address, 16 MiB into the RAM
 * (0x1000000 on x86_64; 0x41000000 on arm64, whose RAM starts at 1 GiB,
 * kernel/bsps/arm64-qemu/include/board/bsp.h). Below it: the kernel image (with
 * partitions' ELFs embedded) and the kernel heap.
 */
#define BOARD_PARTITION_PHYS_START 0x1000000ULL

/**
 * @name Serial ports
 * The I/O port addresses of the 16550 UARTs.
 * @{
 */

/**
 * @macroname `COM0`
 *
 * @description
 * The first serial port (the PC's COM1, I/O port 0x3f8): the main
 * console.
 */
#define  COM0      0x3F8

/**
 * @macroname `COM1`
 *
 * @description
 * The second serial port (the PC's COM2, I/O port 0x2f8): the debug
 * stream (the GDB server).
 */
#define  COM1      0x2f8

/** @} */

/**
 * @typename `s_cons_t`
 *
 * @description
 * The cursor of the VGA text screen (80 x 25 characters) on which the
 * main console's output is copied: its position and the attribute (colors)
 * of the next characters.
 */
typedef struct
{
  int32_t row; /**< The row of the cursor (0 to 24). */
  int32_t col; /**< The column of the cursor (0 to 79). */
  char cur_attr; /**< The attribute of the characters written next. */
} s_cons_t;

/**
 * @name 16550 UART registers
 * The offsets of the registers of a 16550 UART (from its I/O port address)
 * and the bits of its line status register that the console uses.
 * @{
 */

/**
 * @macroname `COM_LSR`
 *
 * @description
 * In: Line Status Register (COM_LSR_DATA, COM_LSR_RFE).
 */
#define COM_LSR		5

/**
 * @macroname `COM_RX`
 *
 * @description
 * In: Receive buffer (DLAB=0).
 */
#define COM_RX		0

/**
 * @macroname `COM_LSR_DATA`
 *
 * @description
 * COM_LSR: Data available.
 */
#define   COM_LSR_DATA	0x01

/**
 * @macroname `COM_LSR_RFE`
 *
 * @description
 * COM_LSR: Error in Received FIFO.
 */
#define   COM_LSR_RFE	0x80

/** @} */

#endif /* JET_X86_64_QEMU_BSP_H */
