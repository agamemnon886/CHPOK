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
 * Modified by Kirill Brazhnikov, 2026-10-31: removed PowerPC support.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of vga.c are here; the
 * macros are documented.
 */

#ifndef VBE_H
#define VBE_H

/**
 * @macroname `VBE_DISPI_IOPORT_INDEX`
 *
 * @description
 * The I/O port of the index of the Bochs VBE registers.
 */
#define VBE_DISPI_IOPORT_INDEX 0x01CE

/**
 * @macroname `VBE_DISPI_IOPORT_DATA`
 *
 * @description
 * The I/O port of the data of the Bochs VBE registers.
 */
#define VBE_DISPI_IOPORT_DATA 0x01CF

enum {
    VBE_DISPI_INDEX_ID,
    VBE_DISPI_INDEX_XRES,
    VBE_DISPI_INDEX_YRES,
    VBE_DISPI_INDEX_BPP,
    VBE_DISPI_INDEX_ENABLE,
    VBE_DISPI_INDEX_BANK,
    VBE_DISPI_INDEX_VIRT_WIDTH,
    VBE_DISPI_INDEX_VIRT_HEIGHT,
    VBE_DISPI_INDEX_X_OFFSET,
    VBE_DISPI_INDEX_Y_OFFSET,
};

enum {
    VBE_DISPI_BPP_4 = 0x04,
    VBE_DISPI_BPP_8 = 0x08,
    VBE_DISPI_BPP_15 = 0x0F,
    VBE_DISPI_BPP_16 = 0x10,
    VBE_DISPI_BPP_24 = 0x18,
    VBE_DISPI_BPP_32 = 0x20,
};

/**
 * @macroname `VGA_PAS`
 *
 * @description
 * The palette address source bit of the VGA attribute controller: 1 enables the
 * display.
 */
#define VGA_PAS 0x20

/**
 * @macroname `SCREEN_WIDTH`
 *
 * @description
 * The width of the screen, in pixels.
 */
#define SCREEN_WIDTH 800

/**
 * @macroname `SCREEN_HEIGHT`
 *
 * @description
 * The height of the screen, in pixels.
 */
#define SCREEN_HEIGHT 600

/**
 * @macroname `SCREEN_BPP`
 *
 * @description
 * The bits per pixel of the screen (32).
 */
#define SCREEN_BPP VBE_DISPI_BPP_32

/**
 * @macroname `DEFAULT_FB_DESCRIPTOR`
 *
 * @description
 * The descriptor of the framebuffer that vga.c gives to its user.
 */
#define DEFAULT_FB_DESCRIPTOR 1

#endif //VBE_H
