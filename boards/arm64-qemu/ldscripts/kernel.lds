/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * boards/x86-qemu/ldscripts/kernel.lds for arm64.
 */

OUTPUT_FORMAT("elf64-littleaarch64");
OUTPUT_ARCH("aarch64")
ENTRY(_core_entry)

SECTIONS
{
	/* QEMU 'virt': RAM starts at 0x40000000, the DTB is placed at its beginning. */
	. = 0x40100000;

	__pok_begin = . ;

	.text :
	{
		*(.text.boot)
		*(.text .text.*)
	}

	.rodata :
	{
		*(.rodata .rodata.*)
	}

	.data :
	{
		*(.data .data.*) *(.got .got.*) *(.bss .bss.*) *(COMMON)

		__archive_begin = .;
		*(.archive)
		__archive_end = .;
		. = ALIGN(16);
		__archive2_begin = .;
		*(.archive2)
		__archive2_end = .;
	}

	. = ALIGN(16);
	__pok_end = . ;

	/* Kernel heap starts at __pok_end, so nothing may be placed after it. */
	/DISCARD/ : { *(.comment) *(.note .note.*) *(.eh_frame*) }
}
