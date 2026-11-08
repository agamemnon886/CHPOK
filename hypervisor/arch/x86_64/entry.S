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
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from hypervisor/src/ to
 * arch/x86_64/.
 */

/*
 * Entry point of the hypervisor, and the start-up code of the other CPUs.
 *
 * Boot CPU: QEMU's Multiboot loader (-kernel) jumps to hv_entry32 in
 * 32-bit protected mode with paging off and EBX holding the address of
 * the Multiboot information. QEMU loads ELF64 files only by the Multiboot
 * header's load addresses (the "a.out kludge"), so hv.lds makes the image
 * a single segment that starts at the beginning of the file. The code
 * clears the BSS, builds page tables that map the first 4 GiB to
 * themselves with 2 MiB pages (from 3 GiB on uncached, for the local APIC
 * and other MMIO), switches to long mode and calls hv_main(mbi).
 *
 * Other CPUs (APs): smp.c copies hv_ap_trampoline to AP_BASE and sends
 * them a start-up IPI with vector AP_BASE >> 12. They begin in real mode
 * there, switch to protected mode and to long mode with the boot CPU's
 * page tables, and call hv_ap_main(hv_ap_cpu_index) on the stack
 * hv_ap_stack_top, both set by smp.c for each AP in turn.
 */

#define MULTIBOOT_MAGIC         0x1BADB002
#define MULTIBOOT_LOAD_ADDRS    0x00010000      // a.out kludge

// GDT selectors.
#define SEL_CODE64              0x08
#define SEL_DATA                0x10
#define SEL_CODE32              0x18

#define BOOT_STACK_SIZE         0x4000
#define AP_BASE                 0x8000

// Page tables.
#define PT_PRESENT_WRITABLE     0x003
#define PD_LARGE_PAGE           0x083           // + 2 MiB page
#define PD_UNCACHED             0x018           // PWT, PCD
#define UNCACHED_FROM           0xC0000000
#define LARGE_PAGE_SIZE         0x200000
#define PD_TABLES               4               // one per GiB
#define PD_ENTRIES_TOTAL        2048            // 4 GiB / 2 MiB
#define TABLE_SIZE              0x1000

#define CR0_PE                  0x00000001
#define CR0_WP                  0x00010000
#define CR0_PG                  0x80000000
#define CR4_PAE                 0x00000020
#define CR4_PGE                 0x00000080
#define MSR_EFER                0xC0000080
#define EFER_LME                0x00000100
#define EFER_NXE                0x00000800

// Address of a trampoline label in the copy at AP_BASE.
#define AP_ADDR(label)          (AP_BASE + ((label) - hv_ap_trampoline))

.macro LOAD_DATA_SEGMENTS
  movw $SEL_DATA, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %ss
  xorl %eax, %eax
  movw %ax, %fs
  movw %ax, %gs
.endm

  .section .multiboot, "a"
  .balign 4
multiboot_header:
  .long MULTIBOOT_MAGIC
  .long MULTIBOOT_LOAD_ADDRS
  .long -(MULTIBOOT_MAGIC + MULTIBOOT_LOAD_ADDRS)
  .long multiboot_header        // header address
  .long __hv_load_begin         // start of the loaded part
  .long __hv_load_end           // end of the loaded part
  .long __hv_end                // end of the BSS
  .long hv_entry32              // entry point

  .text

// ---- Boot CPU, 32-bit protected mode ----
  .code32
  .globl hv_entry32
hv_entry32:
  cli
  cld
  movl %ebx, multiboot_info     // in .data, not cleared below

  movl $__hv_load_end, %edi
  movl $__hv_end, %ecx
  subl %edi, %ecx
  xorl %eax, %eax
  rep stosb

  movl $(boot_stack + BOOT_STACK_SIZE), %esp
  call build_page_tables
  call enter_long_mode
  ljmp $SEL_CODE64, $boot_entry64

/*
 * Fill the (cleared) page tables: PML4 entry 0 -> PDPT, PDPT entries 0-3
 * -> the four page directories, which map 4 GiB with 2 MiB pages.
 */
build_page_tables:
  movl $(pdpt + PT_PRESENT_WRITABLE), pml4

  movl $page_directories, %eax
  xorl %ecx, %ecx
1:
  leal PT_PRESENT_WRITABLE(%eax), %edx
  movl %edx, pdpt(, %ecx, 8)
  addl $TABLE_SIZE, %eax
  incl %ecx
  cmpl $PD_TABLES, %ecx
  jb 1b

  xorl %eax, %eax               // physical address of the page
  xorl %ecx, %ecx               // index over all page directories
2:
  movl %eax, %edx
  orl $PD_LARGE_PAGE, %edx
  cmpl $UNCACHED_FROM, %eax
  jb 3f
  orl $PD_UNCACHED, %edx
3:
  movl %edx, page_directories(, %ecx, 8)
  addl $LARGE_PAGE_SIZE, %eax
  incl %ecx
  cmpl $PD_ENTRIES_TOTAL, %ecx
  jb 2b
  ret

/*
 * Switch on PAE paging with the tables above and long mode, and load the
 * GDT (protected mode, any CPU). Returns in compatibility mode: the
 * caller jumps to 64-bit code.
 */
enter_long_mode:
  movl %cr4, %eax
  orl $(CR4_PAE | CR4_PGE), %eax
  movl %eax, %cr4
  movl $pml4, %eax
  movl %eax, %cr3

  movl $MSR_EFER, %ecx
  rdmsr
  orl $(EFER_LME | EFER_NXE), %eax
  wrmsr

  movl %cr0, %eax
  orl $(CR0_PE | CR0_WP | CR0_PG), %eax
  movl %eax, %cr0

  lgdt gdt_descriptor
  ret

// ---- 64-bit mode ----
  .code64
boot_entry64:
  LOAD_DATA_SEGMENTS
  movq $(boot_stack + BOOT_STACK_SIZE), %rsp
  xorl %ebp, %ebp
  movl multiboot_info(%rip), %edi
  call hv_main
  jmp halt

ap_entry64:
  LOAD_DATA_SEGMENTS
  movq hv_ap_stack_top(%rip), %rsp
  xorl %ebp, %ebp
  movl hv_ap_cpu_index(%rip), %edi
  call hv_ap_main
halt:
  cli
  hlt
  jmp halt

// ---- AP start-up, copied to AP_BASE; refers to itself by AP_ADDR ----
  .code16
  .globl hv_ap_trampoline
hv_ap_trampoline:
  cli
  cld
  xorw %ax, %ax
  movw %ax, %ds
  lgdtl AP_ADDR(ap_gdt_descriptor)
  movl %cr0, %eax
  orl $CR0_PE, %eax
  movl %eax, %cr0
  ljmpl $SEL_CODE32, $AP_ADDR(ap_protected_mode)

  .code32
ap_protected_mode:
  movw $SEL_DATA, %ax
  movw %ax, %ds
  movw %ax, %es
  movw %ax, %ss
  movl $AP_ADDR(ap_stack_top), %esp
  movl $enter_long_mode, %eax   // absolute: the copy runs elsewhere
  call *%eax
  ljmp $SEL_CODE64, $ap_entry64

  .balign 8
ap_gdt_descriptor:
  .word gdt_end - gdt - 1
  .long gdt

  // Stack for the call of enter_long_mode.
  .balign 16
  .space 64
ap_stack_top:
  .globl hv_ap_trampoline_end
hv_ap_trampoline_end:

  .data

multiboot_info:
  .long 0

  .balign 8
  .globl hv_ap_stack_top
hv_ap_stack_top:
  .quad 0
  .globl hv_ap_cpu_index
hv_ap_cpu_index:
  .long 0

  .balign 16
gdt:
  .quad 0                       // null descriptor
  .quad 0x00AF9A000000FFFF      // SEL_CODE64: code, 64-bit, ring 0
  .quad 0x00CF92000000FFFF      // SEL_DATA: data, 4 GiB, ring 0
  .quad 0x00CF9A000000FFFF      // SEL_CODE32: code, 32-bit, ring 0
gdt_end:

  .balign 4
gdt_descriptor:
  .word gdt_end - gdt - 1
  .long gdt

  .bss

  .balign TABLE_SIZE
pml4:
  .space TABLE_SIZE
pdpt:
  .space TABLE_SIZE
page_directories:
  .space TABLE_SIZE * PD_TABLES

  .balign 16
boot_stack:
  .space BOOT_STACK_SIZE
