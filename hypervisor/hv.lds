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
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: a CRC-32 word after .rodata
 * (.hv_crc), which the boot code checks against the image before it.
 */

/*
 * Linker script of the hypervisor. QEMU's multiboot loader loads the file
 * as a flat image (see src/entry.S), so the whole hypervisor is a single
 * segment which starts at the beginning of the file, at 1 MiB. The guest
 * images are in .rodata; the guests' RAM is allocated after __hv_end.
 *
 * The image check (src/crc.c): hvcrc.py writes the CRC-32 of the loaded
 * bytes from hv_image_begin up to hv_image_crc (the ELF headers, .text
 * and .rodata with the VM table and the guest images) into the word at
 * hv_image_crc after linking. .data comes after that word and isn't
 * covered: it is written before the check (multiboot_info, and the CPU
 * sets the accessed bits of the GDT descriptors).
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH("i386:x86-64")
ENTRY(hv_entry32)

PHDRS
{
	hv PT_LOAD FILEHDR PHDRS;
}

SECTIONS
{
	__hv_load_begin = 0x100000;
	hv_image_begin = __hv_load_begin;
	. = __hv_load_begin + SIZEOF_HEADERS;

	.text :
	{
		*(.multiboot)
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

	/* End of the data loaded from the file. */
	__hv_load_end = .;

	.bss :
	{
		*(.bss .bss.*) *(COMMON)
	} :hv

	. = ALIGN(0x1000);
	__hv_end = .;

	/DISCARD/ : { *(.comment) *(.note .note.*) *(.eh_frame*) }
}
