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
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * boards/x86-qemu/ldscripts/kernel.lds to x86_64 (64-bit).
 */

OUTPUT_FORMAT("elf64-x86-64");
OUTPUT_ARCH("i386:x86-64")
ENTRY(_core_entry)

/*
 * QEMU's multiboot loader can't load ELF64 images, so it loads this file
 * as a flat image (see entry.S): everything is placed in a single segment,
 * which starts with the ELF headers at the beginning of the file. Thus
 * file offsets coincide with offsets from __pok_load_begin.
 */
PHDRS
{
	kernel PT_LOAD FILEHDR PHDRS;
}

SECTIONS
{
	__pok_load_begin = 0x100000;
	. = __pok_load_begin + SIZEOF_HEADERS;

	__pok_begin = . ;

	.text :
	{
		*(.multiboot)
		*(.text .text.*)
	} :kernel

	.rodata :
	{
		*(.rodata .rodata.*)
	} :kernel

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
	} :kernel

	/* End of the data loaded from the file. */
	__pok_load_end = . ;

	. = ALIGN(16);
	__pok_end = . ;

	/* Kernel heap starts at __pok_end, so nothing may be placed after it. */
	/DISCARD/ : { *(.comment) *(.note .note.*) *(.eh_frame*) }
}
