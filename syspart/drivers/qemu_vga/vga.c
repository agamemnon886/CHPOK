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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; discarded results are cast to void; functions
 * without parameters are declared with (void); file-local definitions
 * have internal linkage; essentially Boolean conditions; U suffixes for
 * unsigned constants; operands of mixed precedence are parenthesized;
 * deviations are documented where a guideline can't be followed; file-
 * local objects are static.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros are in vbe.h; the
 * struct types are used by their typedef names.
 */

/**
 * @file
 * The QEMU standard VGA (Bochs VBE) driver: an 800x600 32-bit mode and a
 * double-buffered frame buffer.
 */

#include <pci.h>
#include <ioports.h>
#include <stdio.h>
#include <byteorder.h>

//TODO DELETE
#include "vbe.h"

#include <fb.h>
/*
 * MISRA C:2025 Rule 11.4 and Rule 11.6 deviation (this file): the device's
 * registers, ROM and frame buffer are at addresses that the PCI
 * configuration gives as integers.
 */

static pci_dev_t vga_dev;
static int32_t initialized = 0;



/**
 * @funcname `vbe_write`
 *
 * @description
 * Writes a VBE register.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `reg` | `uint16_t` | [in] The register. |
 * | `val` | `uint16_t` | [in] The value. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vga_dev` | `pci_dev_t` | The PCI device of the VGA |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `iowrite16()` with the arguments `reg` and
 *    `(void *)pci_convert_legacy_port(&vga_dev, VBE_DISPI_IOPORT_INDEX)`.
 * 2. Call `iowrite16()` with the arguments `val` and
 *    `(void *)pci_convert_legacy_port(&vga_dev, VBE_DISPI_IOPORT_DATA)`.
 *
 * @calls
 * - `iowrite16()`
 * - `pci_convert_legacy_port()`
 */
static void vbe_write(uint16_t reg, uint16_t val)
{
    iowrite16(reg, (void *)pci_convert_legacy_port(&vga_dev, VBE_DISPI_IOPORT_INDEX));
    iowrite16(val, (void *)pci_convert_legacy_port(&vga_dev, VBE_DISPI_IOPORT_DATA));
}

/**
 * @funcname `vga_init`
 *
 * @description
 * Initializes the VGA: finds its PCI device, sets the 800x600 32-bit mode and
 * enables the display.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `initialized` | `int32_t` | Whether the VGA is initialized |
 * | `vga_dev` | `pci_dev_t` | The PCI device of the VGA |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `printf()` with the argument `"qemu-vga: initializing\n"`.
 * 2. Call `pci_get_dev_by_bdf()` with the arguments `0`, `1`, `0` and the
 *    address of `vga_dev`.
 * 3. If `vga_dev.vendor_id` is equal to `0xFFFFU`:
 *    1. Call `printf()` with the argument
 *       `"qemu-vga: have not found vga device\n"`.
 *    2. Return.
 * 4. If `vga_dev.resources[PCI_RESOURCE_BAR0].addr` is equal to `0U`:
 *    1. Call `printf()` with the argument `"qemu-vga: BAR0 not mapped\n"`.
 *    2. Return.
 * 5. If `vga_dev.resources[PCI_RESOURCE_ROM].addr` is equal to `0U`:
 *    1. Call `printf()` with the argument `"qemu-vga: ROM not mapped\n"`.
 *    2. Return.
 * 6. Call `printf()` with the arguments `"qemu-vga: bios[0] 0x%%x\n"` and
 *    `ioread8((uint8_t *)vga_dev.resources[PCI_RESOURCE_ROM].addr)`.
 * 7. Call `printf()` with the arguments `"qemu-vga: bios[1] 0x%%x\n"` and
 *    `ioread8((uint8_t *)vga_dev.resources[PCI_RESOURCE_ROM].addr + 1)`.
 * 8. Call `vbe_write()` with the arguments `VBE_DISPI_INDEX_ENABLE` and `0`.
 * 9. Call `vbe_write()` with the arguments `VBE_DISPI_INDEX_XRES` and
 *    `SCREEN_WIDTH`.
 * 10. Call `vbe_write()` with the arguments `VBE_DISPI_INDEX_YRES` and
 *     `SCREEN_HEIGHT`.
 * 11. Call `vbe_write()` with the arguments `VBE_DISPI_INDEX_BPP` and
 *     `SCREEN_BPP`.
 * 12. Call `vbe_write()` with the arguments `VBE_DISPI_INDEX_ENABLE` and `1`.
 * 13. Call `iowrite8()` with the arguments `VGA_PAS` and
 *     `(uint8_t *) pci_convert_legacy_port(&vga_dev, 0x3c0)`.
 * 14. Call `vbe_write()` with the arguments `VBE_DISPI_INDEX_VIRT_WIDTH` and
 *     `SCREEN_WIDTH`.
 * 15. Set `initialized` to `1`.
 *
 * @calls
 * - `printf()`
 * - `pci_get_dev_by_bdf()`
 * - `ioread8()`
 * - `vbe_write()`
 * - `iowrite8()`
 * - `pci_convert_legacy_port()`
 */
void vga_init(void)
{
    (void) printf("qemu-vga: initializing\n");

    pci_get_dev_by_bdf(0, 1, 0, &vga_dev);
    if (vga_dev.vendor_id == 0xFFFFU) {
        (void) printf("qemu-vga: have not found vga device\n");
        return;
    }

    if (vga_dev.resources[PCI_RESOURCE_BAR0].addr == 0U) {
        (void) printf("qemu-vga: BAR0 not mapped\n");
        return;
    }

    if (vga_dev.resources[PCI_RESOURCE_ROM].addr == 0U) {
        (void) printf("qemu-vga: ROM not mapped\n");
        return;
    }

    (void) printf(
        "qemu-vga: bios[0] 0x%x\n",
            ioread8((uint8_t *)vga_dev.resources[PCI_RESOURCE_ROM].addr));
    (void) printf(
        "qemu-vga: bios[1] 0x%x\n",
            ioread8((uint8_t *)vga_dev.resources[PCI_RESOURCE_ROM].addr + 1));

    vbe_write(VBE_DISPI_INDEX_ENABLE, 0);
    vbe_write(VBE_DISPI_INDEX_XRES, SCREEN_WIDTH);
    vbe_write(VBE_DISPI_INDEX_YRES, SCREEN_HEIGHT);
    vbe_write(VBE_DISPI_INDEX_BPP, SCREEN_BPP);
    vbe_write(VBE_DISPI_INDEX_ENABLE, 1);

    //TODO this is mmio bar
    iowrite8(VGA_PAS, (uint8_t *) pci_convert_legacy_port(&vga_dev, 0x3c0));

    vbe_write(VBE_DISPI_INDEX_VIRT_WIDTH, SCREEN_WIDTH);
    initialized = 1;
}

/**
 * @funcname `rgba_to_argb`
 *
 * @description
 * Converts an RGBA color to ARGB.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `rgba_color` | `uint32_t` | [in] The RGBA color. |
 *
 * @return The ARGB color.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `(rgba_color>>8) | ((rgba_color&0xffU)<<24)`.
 *
 * @calls
 * None.
 */
uint32_t rgba_to_argb(uint32_t rgba_color)
{
    return (rgba_color>>8) | ((rgba_color&0xffU)<<24);
}


static int32_t first = 1;
static int32_t fb_initialized = 0;

/**
 * @funcname `uwrm_scm_get_direct_fb`
 *
 * @description
 * Gives out the frame buffer (once): its front and back surfaces, size and
 * format.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fb` | `uwrm_scm_direct_fb_t *` | [out] Where the frame buffer is described. |
 *
 * @return UWRM_OK, or UWRM_ERROR if it was given out already.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fb_initialized` | `int32_t` | Whether the frame buffer was given out |
 * | `vga_dev` | `pci_dev_t` | The PCI device of the VGA |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `fb_initialized` is not equal to `0`:
 *    1. Return `UWRM_ERROR`.
 * 2. Set `fb_initialized` to `1`.
 * 3. Set `fb->hfb` to `DEFAULT_FB_DESCRIPTOR`.
 * 4. Set `fb->back_surface` to
 *    `(uint32_t *)vga_dev.resources[PCI_RESOURCE_BAR0].addr +`
 *    `(SCREEN_WIDTH*SCREEN_HEIGHT)`.
 * 5. Set `fb->front_surface` to
 *    `(void *) vga_dev.resources[PCI_RESOURCE_BAR0].addr`.
 * 6. Set `fb->width` to `SCREEN_WIDTH`.
 * 7. Set `fb->height` to `SCREEN_HEIGHT`.
 * 8. Set `fb->format` to `UWRM_FORMAT_ARGB8888`.
 * 9. Return `UWRM_OK`.
 *
 * @calls
 * None.
 */
int32_t uwrm_scm_get_direct_fb(uwrm_scm_direct_fb_t * fb)
{
    if (fb_initialized != 0) {
        return UWRM_ERROR;
    }
    fb_initialized = 1;
    fb->hfb = DEFAULT_FB_DESCRIPTOR;
    fb->back_surface =
        (uint32_t *)vga_dev.resources[PCI_RESOURCE_BAR0].addr
            + (SCREEN_WIDTH*SCREEN_HEIGHT);
    fb->front_surface = (void *) vga_dev.resources[PCI_RESOURCE_BAR0].addr;
    fb->width = SCREEN_WIDTH;
    fb->height = SCREEN_HEIGHT;
    fb->format = UWRM_FORMAT_ARGB8888;

    return UWRM_OK;
}

/**
 * @funcname `uwrm_scm_fb_swap`
 *
 * @description
 * Swaps the front and back surfaces of the frame buffer and shows the new front
 * one.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fb` | `uwrm_scm_direct_fb_t *` | [in,out] The frame buffer. |
 *
 * @return UWRM_OK, or UWRM_ERROR for another frame buffer.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `first` | `int32_t` | Whether the front surface is the first half of the frame buffer |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `tmp` | `void *` | The front surface |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `fb->hfb` is not equal to `DEFAULT_FB_DESCRIPTOR`:
 *    1. Return `UWRM_ERROR`.
 * 2. Set `tmp` to `fb->front_surface`.
 * 3. Set `fb->front_surface` to `fb->back_surface`.
 * 4. Set `fb->back_surface` to `tmp`.
 * 5. Call `vbe_write()` with the arguments `VBE_DISPI_INDEX_Y_OFFSET` and
 *    `SCREEN_HEIGHT*first`.
 * 6. Set `first` to `1-first`.
 * 7. Return `UWRM_OK`.
 *
 * @calls
 * - `vbe_write()`
 */
int32_t uwrm_scm_fb_swap(uwrm_scm_direct_fb_t *fb)
{
    if (fb->hfb != DEFAULT_FB_DESCRIPTOR) {
        return UWRM_ERROR;
    }

    void *tmp = fb->front_surface;
    fb->front_surface = fb->back_surface;
    fb->back_surface = tmp;

    vbe_write(VBE_DISPI_INDEX_Y_OFFSET, SCREEN_HEIGHT*first);

    first = 1-first;

    return UWRM_OK;
}

