/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Linker script of the arm64 hypervisor. QEMU's ELF loader (-kernel)
 * copies the single segment, which starts with the ELF headers, to
 * 0x40100000; QEMU puts its device tree at 0x40000000, the start of the
 * RAM, below it. The guest images are in .rodata; the guests' RAM is
 * allocated after __hv_end.
 *
 * The image check (hypervisor/src/crc.c): hvcrc.py writes the CRC-32 of
 * the loaded bytes from hv_image_begin up to hv_image_crc (the ELF
 * headers, .text and .rodata with the VM table, the guest images and
 * device trees) into the word at hv_image_crc after linking. .data comes
 * after that word and isn't covered.
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH("aarch64")
ENTRY(hv_entry)

PHDRS
{
	hv PT_LOAD FILEHDR PHDRS;
}

SECTIONS
{
	__hv_load_begin = 0x40100000;
	hv_image_begin = __hv_load_begin;
	. = __hv_load_begin + SIZEOF_HEADERS;

	.text :
	{
		*(.text.boot)
		*(.text .text.*)
	} :hv

	.rodata :
	{
		*(.rodata .rodata.*)
	} :hv

	/* CRC-32 of the image up to here (hvcrc.py). */
	.hv_crc ALIGN(4) :
	{
		hv_image_crc = .;
		LONG(0)
	} :hv

	.data :
	{
		*(.data .data.*)
	} :hv

	/* Cleared by entry.S (8-byte stores). */
	.bss ALIGN(16) :
	{
		__bss_start = .;
		*(.bss .bss.*) *(COMMON)
		. = ALIGN(16);
		__bss_end = .;
	} :hv

	. = ALIGN(0x1000);
	__hv_end = .;

	/DISCARD/ : { *(.comment) *(.note .note.*) *(.eh_frame*) }
}
