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
 * kernel/arch/x86/entry.S to x86_64 (64-bit).
 */

/*
 * Kernel entry point.
 *
 * QEMU's multiboot loader starts the kernel in 32-bit protected mode
 * with paging off. It can't load ELF64 images, so the multiboot header
 * provides load addresses ("a.out kludge") and QEMU loads the file as a
 * flat image: kernel.lds keeps the whole kernel in a single segment
 * which starts at the beginning of the file.
 *
 * The code below switches to 64-bit (long) mode with the first GiB
 * identity-mapped, then calls the kernel.
 */

#include "gdt.h"

#define STACK_SIZE 0x4000

#define MULTIBOOT_MAGIC     0x1BADB002
#define MULTIBOOT_FLAGS     0x00010000 /* Load addresses are in the header. */

#define CR0_PE  (1 << 0)
#define CR0_MP  (1 << 1)
#define CR0_EM  (1 << 2)
#define CR0_WP  (1 << 16)
#define CR0_PG  (1 << 31)
#define CR4_PAE (1 << 5)
#define CR4_PGE (1 << 7)
#define CR4_OSFXSR (1 << 9)
#define CR4_OSXMMEXCPT (1 << 10)
#define MSR_EFER 0xC0000080
#define EFER_LME (1 << 8)
#define EFER_NXE (1 << 11)

.section .multiboot, "a"
  .align 4
multiboot_header:
  .long   MULTIBOOT_MAGIC
  .long   MULTIBOOT_FLAGS
  .long   -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)
  .long   multiboot_header      /* header_addr */
  .long   __pok_load_begin      /* load_addr */
  .long   __pok_load_end        /* load_end_addr */
  .long   __pok_end             /* bss_end_addr */
  .long   _core_entry           /* entry_addr */

.section .text

.code32
.globl _core_entry
_core_entry:
  cli
  movl $(boot_stack + STACK_SIZE), %esp

  /* Enable PAE and global pages, then load boot page tables. */
  movl %cr4, %eax
  orl $(CR4_PAE | CR4_PGE), %eax
  movl %eax, %cr4
  movl $boot_pml4, %eax
  movl %eax, %cr3

  /* Enable long mode and no-execute pages. */
  movl $MSR_EFER, %ecx
  rdmsr
  orl $(EFER_LME | EFER_NXE), %eax
  wrmsr

  /* Enable paging: this activates long mode (in compatibility mode). */
  movl %cr0, %eax
  orl $(CR0_PG | CR0_WP | CR0_PE), %eax
  movl %eax, %cr0

  lgdt boot_gdt_desc
  ljmp $GDT_KERNEL_CODE, $long_mode_entry

.code64
long_mode_entry:
  movw $GDT_KERNEL_DATA, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %ss
  xorw %ax, %ax
  movw %ax, %fs
  movw %ax, %gs

  movq $(boot_stack + STACK_SIZE), %rsp
  xorq %rbp, %rbp

  /*
   * Enable SSE for partitions. The kernel itself never uses SSE
   * registers (-mgeneral-regs-only).
   */
  movq %cr0, %rax
  andq $~CR0_EM, %rax
  orq $CR0_MP, %rax
  movq %rax, %cr0
  movq %cr4, %rax
  orq $(CR4_OSFXSR | CR4_OSXMMEXCPT), %rax
  movq %rax, %cr4

  call pok_arch_init
  call jet_boot
loop:
  hlt
  jmp loop

  .globl pok_trap
pok_trap:
  int3
  ret

.section .data

/*
 * The only GDT. The TSS descriptor is filled by gdt.c.
 * Layout must correspond to GDT_* selectors in gdt.h.
 */
  .align 16
  .globl ja_gdt
ja_gdt:
  .quad 0                       /* null */
  .quad 0x00AF9A000000FFFF      /* GDT_KERNEL_CODE: 64-bit, DPL 0 */
  .quad 0x00CF92000000FFFF      /* GDT_KERNEL_DATA: DPL 0 */
  .quad 0x00CFF2000000FFFF      /* GDT_USER_DATA: DPL 3 */
  .quad 0x00AFFA000000FFFF      /* GDT_USER_CODE: 64-bit, DPL 3 */
  .quad 0, 0                    /* GDT_TSS: 16-byte system descriptor */
ja_gdt_end:

  .align 4
boot_gdt_desc:
  .word ja_gdt_end - ja_gdt - 1
  .long ja_gdt

/* Boot page tables: the first GiB is identity-mapped with 2 MiB pages. */
  .align 4096
boot_pml4:
  .quad boot_pdpt + 0x3         /* present, writable */
  .fill 511, 8, 0
boot_pdpt:
  .quad boot_pd + 0x3
  .fill 511, 8, 0
boot_pd:
  .set addr, 0
  .rept 512
  .quad addr + 0x83             /* present, writable, 2 MiB page */
  .set addr, addr + 0x200000
  .endr

.section .bss
  .align 16
boot_stack:
  .space STACK_SIZE
