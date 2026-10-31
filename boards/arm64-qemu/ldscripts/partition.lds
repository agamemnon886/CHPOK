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
 * boards/x86-qemu/ldscripts/partition.lds for arm64.
 */

OUTPUT_FORMAT("elf64-littleaarch64");
OUTPUT_ARCH("aarch64")
ENTRY(__pok_partition_start)

SECTIONS
{
	/* This should coincide with ja_space_shared_data() and POK_PARTITION_MEMORY_BASE in the kernel. */
	kshd = 0x100000000;

	. = 0x100001000;

	__partition_begin = . ;

	.text :
	{
		*(.text .text.*)
	}

	.rodata :
	{
		*(.rodata .rodata.*)
	}

	.data :
	{
		*(.data .data.*) *(.got .got.*) *(.bss .bss.*) *(COMMON)
	}

	__partition_end = . ;

	/DISCARD/ : { *(.comment) *(.note .note.*) *(.eh_frame*) }
}
