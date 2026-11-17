/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * kernel/arch/x86/interrupt.c to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; discarded results are cast to void; loading the IDT
 * and reading CR2 are small inline-assembly functions; the gate fields
 * are assigned with casts to their widths; deviations are documented
 * where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the exception
 * table is defined in the function that uses it; operands of mixed
 * precedence are parenthesized; deviations are documented where a
 * guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-04: with the GDB server, every
 * interrupt frame is saved in global_thread_stack (and the previous one is
 * restored when an interrupt of kernel code returns), user space may
 * execute int3, and debug and breakpoint exceptions go to the GDB server
 * first.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-13: interrupt_frame is used by its
 * typedef name; the macros, the types and the declarations of the entry stubs
 * are moved to kernel/arch/x86_64/interrupt.h; the type of the table of the
 * exceptions is the typedef exception_info_t.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/x86_64/interrupt.c (the CPU support packages are in
 * kernel/csps).
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
 */

/**
 * @file
 * Interrupts and exceptions on x86_64: the IDT and the common interrupt
 * handler (syscalls, the timer, GDB traps, exceptions).
 */

#include <libc.h>
#include <core/debug.h>
#include <core/error.h>
#include <core/sched.h>
#include <bsp/bsp.h>
#include <gdb.h>

#include "interrupt.h"
#include "gdt.h"
#include "sysdesc.h"

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): the kernel stack of a thread
 * starts right after its interrupt frame, at frame + 1.
 */

static idt_entry_t pok_idt[IDT_SIZE] __attribute__((aligned(16)));


/**
 * @funcname `cpu_load_idt`
 *
 * @description
 * Loads the IDT register (lidt).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `desc` | `const sysdesc_t *` | [in] The IDT's limit and base. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_INTERRUPT_CPU_LOAD_IDT_01}: `cpu_load_idt()` shall load the
 *   interrupt descriptor table register.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_001}
 *
 * @llr
 * - \llrid{LLR_INTERRUPT_CPU_LOAD_IDT_01}: `cpu_load_idt()` shall execute the
 *   instruction `lidt` once with the descriptor at `desc`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_CPU_LOAD_IDT_01}
 *
 * @design
 * 1. Execute the assembly code `lidt %0`.
 *    \llrref{LLR_INTERRUPT_CPU_LOAD_IDT_01}
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Rule 11.1 and Rule 11.4 deviation (this file): the IDT and
 * its gates are described to the CPU by addresses, as integers: the
 * address of the table and of each interrupt stub, and the kernel stack
 * pointer of the interrupted thread.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (the functions below):
 * privileged instructions are written as GNU inline assembly, each isolated
 * in a small function.
 */
static void cpu_load_idt(const sysdesc_t* desc)
{
   asm volatile ("lidt %0" : : "m" (*desc));
}

/**
 * @funcname `cpu_read_cr2`
 *
 * @description
 * Reads CR2: the address of the last page fault.
 *
 * @return CR2.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cr2` | `uint64_t` | CR2 |
 *
 * @hlr
 * - \hlrid{HLR_INTERRUPT_CPU_READ_CR2_01}: `cpu_read_cr2()` shall give the
 *   address of the last page fault.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_003}
 *
 * @llr
 * - \llrid{LLR_INTERRUPT_CPU_READ_CR2_01}: `cpu_read_cr2()` shall return the
 *   value of CR2.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_CPU_READ_CR2_01}
 *
 * @design
 * 1. Execute the assembly code `mov %%%cr2, %0`.
 *    \llrref{LLR_INTERRUPT_CPU_READ_CR2_01}
 * 2. Return `cr2`. \llrref{LLR_INTERRUPT_CPU_READ_CR2_01}
 *
 * @calls
 * None.
 */
static uint64_t cpu_read_cr2(void)
{
   uint64_t cr2;

   asm volatile ("mov %%cr2, %0" : "=r" (cr2));
   return cr2;
}

/**
 * @funcname `idt_set_gate`
 *
 * @description
 * Writes an interrupt gate of the IDT (interrupts disabled on entry).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `index` | `uint32_t` | [in] The vector. |
 * | `entry` | `void (*)(void)` | [in] The entry stub. |
 * | `dpl` | `uint32_t` | [in] The privilege level that may use it with int (3: user space). |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pok_idt` | `idt_entry_t[256]` | The IDT |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `offset` | `uint64_t` | The stub's address |
 *
 * @hlr
 * - \hlrid{HLR_INTERRUPT_IDT_SET_GATE_01}: `idt_set_gate()` shall make a vector
 *   of the interrupt table enter the kernel at a handler, with interrupts
 *   disabled, from the privilege levels that the gate allows.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_001}
 *   - \srqref{SRQ_JET_SYS_002}
 *
 * @llr
 * - \llrid{LLR_INTERRUPT_IDT_SET_GATE_01}: `idt_set_gate()` shall set, in
 *   `pok_idt[index]`, `offset_low` to the bits 15 to 0 of the address `entry`,
 *   `offset_mid` to its bits 31 to 16 and `offset_high` to its bits 63 to 32,
 *   `selector` to `GDT_KERNEL_CODE`, `ist` to 0, `type_attr` to
 *   `IDT_INTERRUPT_GATE(dpl)` and `reserved` to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_IDT_SET_GATE_01}
 *
 * @design
 * 1. Set `offset` to `(uint64_t)entry`. \llrref{LLR_INTERRUPT_IDT_SET_GATE_01}
 * 2. Set `pok_idt[index].offset_low` to `(uint16_t) (offset & 0xFFFFU)`.
 *    \llrref{LLR_INTERRUPT_IDT_SET_GATE_01}
 * 3. Set `pok_idt[index].selector` to `GDT_KERNEL_CODE`.
 *    \llrref{LLR_INTERRUPT_IDT_SET_GATE_01}
 * 4. Set `pok_idt[index].ist` to `0`. \llrref{LLR_INTERRUPT_IDT_SET_GATE_01}
 * 5. Set `pok_idt[index].type_attr` to `IDT_INTERRUPT_GATE(dpl)`.
 *    \llrref{LLR_INTERRUPT_IDT_SET_GATE_01}
 * 6. Set `pok_idt[index].offset_mid` to
 *    `(uint16_t) ((offset >> 16) & 0xFFFFU)`.
 *    \llrref{LLR_INTERRUPT_IDT_SET_GATE_01}
 * 7. Set `pok_idt[index].offset_high` to `(uint32_t) (offset >> 32)`.
 *    \llrref{LLR_INTERRUPT_IDT_SET_GATE_01}
 * 8. Set `pok_idt[index].reserved` to `0`.
 *    \llrref{LLR_INTERRUPT_IDT_SET_GATE_01}
 *
 * @calls
 * None.
 */
static void idt_set_gate(uint32_t index, void (*entry)(void), uint32_t dpl)
{
   uint64_t offset = (uint64_t)entry;

   pok_idt[index].offset_low  = (uint16_t) (offset & 0xFFFFU);
   pok_idt[index].selector    = GDT_KERNEL_CODE;
   pok_idt[index].ist         = 0;
   pok_idt[index].type_attr   = IDT_INTERRUPT_GATE(dpl);
   pok_idt[index].offset_mid  = (uint16_t) ((offset >> 16) & 0xFFFFU);
   pok_idt[index].offset_high = (uint32_t) (offset >> 32);
   pok_idt[index].reserved    = 0;
}

/**
 * @funcname `ja_idt_init`
 *
 * @description
 * Fills the IDT with the stubs of interrupt_entries.S, the syscall gate open to
 * user space (and int3 with GDB), and loads it.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ja_interrupt_stubs` | `void (*const[48])(void)` | The entry stubs of the vectors (interrupt_entries.S) |
 * | `pok_idt` | `idt_entry_t[256]` | The IDT |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The vector |
 * | `sysdesc` | `sysdesc_t` | The IDT's limit and base |
 *
 * @hlr
 * - \hlrid{HLR_INTERRUPT_JA_IDT_INIT_01}: `ja_idt_init()` shall set up the
 *   interrupt table: every exception and interrupt vector enters the common
 *   handler of the kernel, and the system call vector and the breakpoint vector
 *   may be raised from user mode.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_001}
 *   - \srqref{SRQ_JET_SYS_002}
 *
 * @llr
 * - \llrid{LLR_INTERRUPT_JA_IDT_INIT_01}: `ja_idt_init()` shall set every byte
 *   of `pok_idt` to 0 with `memset()`, call `idt_set_gate()` with `i`,
 *   `ja_interrupt_stubs[i]` and 0 for each `i` from 0 to `IRQ_VECTOR_LAST`,
 *   then with `EXCEPTION_SYSCALL`, `ja_interrupt_stub_syscall` and 3, and with
 *   `EXCEPTION_BREAKPOINT`, `ja_interrupt_stubs[EXCEPTION_BREAKPOINT]` and 3,
 *   and then call `cpu_load_idt()` with a descriptor whose limit is the size of
 *   `pok_idt` minus 1 and whose base is the address of `pok_idt`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_IDT_INIT_01}
 *
 * @design
 * 1. Call `memset()` with the arguments `pok_idt`, `0` and `sizeof(pok_idt)`.
 *    \llrref{LLR_INTERRUPT_JA_IDT_INIT_01}
 * 2. Set `i` to `0`; then while `i` is less than or equal to `IRQ_VECTOR_LAST`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_INTERRUPT_JA_IDT_INIT_01}
 *    1. Call `idt_set_gate()` with the arguments `i`, `ja_interrupt_stubs[i]`
 *       and `0`.
 * 3. Call `idt_set_gate()` with the arguments `EXCEPTION_SYSCALL`,
 *    `ja_interrupt_stub_syscall` and `3`. \llrref{LLR_INTERRUPT_JA_IDT_INIT_01}
 * 4. Call `idt_set_gate()` with the arguments `EXCEPTION_BREAKPOINT`,
 *    `ja_interrupt_stubs[EXCEPTION_BREAKPOINT]` and `3`.
 *    \llrref{LLR_INTERRUPT_JA_IDT_INIT_01}
 * 5. Set `sysdesc.limit` to `sizeof(pok_idt) - 1U`.
 *    \llrref{LLR_INTERRUPT_JA_IDT_INIT_01}
 * 6. Set `sysdesc.base` to `(uint64_t)pok_idt`.
 *    \llrref{LLR_INTERRUPT_JA_IDT_INIT_01}
 * 7. Call `cpu_load_idt()` with the address of `sysdesc`.
 *    \llrref{LLR_INTERRUPT_JA_IDT_INIT_01}
 *
 * @calls
 * - `memset()`
 * - `idt_set_gate()`
 * - `cpu_load_idt()`
 */
void ja_idt_init(void)
{
   sysdesc_t sysdesc;

   (void) memset(pok_idt, 0, sizeof(pok_idt));

   for (uint32_t i = 0; i <= IRQ_VECTOR_LAST; i++) {
      idt_set_gate(i, ja_interrupt_stubs[i], 0);
   }

   // The only gate user space may use.
   idt_set_gate(EXCEPTION_SYSCALL, ja_interrupt_stub_syscall, 3);
#ifdef POK_NEEDS_GDB
   // Breakpoints inserted by GDB into partitions: int3 checks the DPL.
   idt_set_gate(EXCEPTION_BREAKPOINT,
      ja_interrupt_stubs[EXCEPTION_BREAKPOINT], 3);
#endif /* POK_NEEDS_GDB */

   sysdesc.limit = sizeof(pok_idt) - 1U;
   sysdesc.base = (uint64_t)pok_idt;
   cpu_load_idt(&sysdesc);
}


/**
 * @funcname `dump_registers`
 *
 * @description
 * Prints the registers of an interrupt frame (debug output).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `const interrupt_frame *` | [in] The frame. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_INTERRUPT_DUMP_REGISTERS_01}: `dump_registers()` shall print the
 *   registers of an interrupt frame when the kernel is built with debug output.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_004}
 *
 * @llr
 * - \llrid{LLR_INTERRUPT_DUMP_REGISTERS_01}: `dump_registers()` shall call
 *   `printf_debug()` four times: with `"CS: %%lx, SS: %%lx, RFLAGS: %%lx\n"`
 *   and `cs`, `ss` and `rflags` of `frame`; with `"RIP: %%lx, RSP: %%lx, RBP:
 *   %%lx, ErrorCode: %%lx\n"` and `rip`, `rsp`, `rbp` and `error` of `frame`;
 *   with `"RAX: %%lx, RBX: %%lx, RCX: %%lx, RDX: %%lx\n"` and `rax`, `rbx`,
 *   `rcx` and `rdx` of `frame`; and with `"RSI: %%lx, RDI: %%lx\n"` and `rsi`
 *   and `rdi` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_DUMP_REGISTERS_01}
 *
 * @design
 * 1. Evaluate `printf_debug("CS: %%lx, SS: %%lx, RFLAGS: %%lx\n", (unsigned`
 *    `long)frame->cs, (unsigned long)frame->ss, (unsigned long)frame->rflags)`.
 *    \llrref{LLR_INTERRUPT_DUMP_REGISTERS_01}
 * 2. Evaluate
 *    `printf_debug("RIP: %%lx, RSP: %%lx, RBP: %%lx, ErrorCode: %%lx\n",`
 *    `(unsigned`
 *    `long)frame->rip, (unsigned long)frame->rsp, (unsigned long)frame->rbp,`
 *    `(unsigned long)frame->error)`. \llrref{LLR_INTERRUPT_DUMP_REGISTERS_01}
 * 3. Evaluate
 *    `printf_debug("RAX: %%lx, RBX: %%lx, RCX: %%lx, RDX: %%lx\n", (unsigned`
 *    `long)frame->rax, (unsigned long)frame->rbx, (unsigned long)frame->rcx,`
 *    `(unsigned long)frame->rdx)`. \llrref{LLR_INTERRUPT_DUMP_REGISTERS_01}
 * 4. Evaluate
 *    `printf_debug("RSI: %%lx, RDI: %%lx\n", (unsigned long)frame->rsi,`
 *    `(unsigned long)frame->rdi)`. \llrref{LLR_INTERRUPT_DUMP_REGISTERS_01}
 *
 * @calls
 * - `printf()`
 */
static void dump_registers(const interrupt_frame* frame)
{
   printf_debug("CS: %lx, SS: %lx, RFLAGS: %lx\n",
      (unsigned long)frame->cs, (unsigned long)frame->ss, (unsigned long)frame->rflags);
   printf_debug("RIP: %lx, RSP: %lx, RBP: %lx, ErrorCode: %lx\n",
      (unsigned long)frame->rip, (unsigned long)frame->rsp,
      (unsigned long)frame->rbp, (unsigned long)frame->error);
   printf_debug("RAX: %lx, RBX: %lx, RCX: %lx, RDX: %lx\n",
      (unsigned long)frame->rax, (unsigned long)frame->rbx,
      (unsigned long)frame->rcx, (unsigned long)frame->rdx);
   printf_debug("RSI: %lx, RDI: %lx\n",
      (unsigned long)frame->rsi, (unsigned long)frame->rdi);
}

/**
 * @funcname `process_exception`
 *
 * @description
 * Handles an exception: prints it, raises its health monitoring error; an
 * exception in the kernel is fatal.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in] The frame. |
 * | `from_user` | `pok_bool_t` | [in] Whether it came from user space. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `cr2` | `uint64_t` | The page fault address |
 * | `error_id` | `pok_error_id_t` | Its error |
 * | `exceptions` | `static const exception_info_t[32]` | The name and error of each exception |
 * | `name` | `const char *` | The exception's name |
 * | `vector` | `uint32_t` | The vector |
 *
 * @hlr
 * - \hlrid{HLR_INTERRUPT_PROCESS_EXCEPTION_01}: `process_exception()` shall
 *   handle an exception of the processor as an error of the partition: a memory
 *   fault as a memory violation, an arithmetic fault as a numeric error, others
 *   as an illegal request, a stack overflow or an unhandled interrupt, as the
 *   table of the exceptions says.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_003}
 *   - \srqref{SRQ_JET_HM_001}
 *   - \srqref{SRQ_JET_MEM_013}
 * - \hlrid{HLR_INTERRUPT_PROCESS_EXCEPTION_02}: `process_exception()` shall
 *   stop the kernel after an exception in the kernel itself.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_004}
 *
 * @llr
 * - \llrid{LLR_INTERRUPT_PROCESS_EXCEPTION_01}: If the vector of `frame` has an
 *   entry in `exceptions`, `process_exception()` shall take the name and the
 *   error of the entry, and call `pok_raise_error()` once with the error,
 *   `from_user` and `rip` of `frame` as the address. The entries are: 0
 *   `"divide by zero error"`, `POK_ERROR_ID_NUMERIC_ERROR`; 1 `"debug"`, 2
 *   `"NMI fault"`, 3 `"breakpoint"`, 6 `"invalid opcode fault"`, 7 `"no math
 *   coprocessor fault"` and 13 `"general protection fault"`,
 *   `POK_ERROR_ID_ILLEGAL_REQUEST`; 4 `"overflow"`,
 *   `POK_ERROR_ID_STACK_OVERFLOW`; 5 `"bound range fault"`, 9 `"copseg overrun
 *   fault"`, 10 `"invalid TSS fault"`, 11 `"segment not present fault"`, 12
 *   `"stack segment fault"` and 14 `"pagefault"`,
 *   `POK_ERROR_ID_MEMORY_VIOLATION`; 8 `"double fault"`,
 *   `POK_ERROR_ID_UNHANDLED_INT`; 16 `"FPU fault"`, 17 `"alignement check
 *   fault"`, 18 `"machine check fault"` and 19 `"SIMD fault"`,
 *   `POK_ERROR_ID_HARDWARE_FAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_PROCESS_EXCEPTION_01}
 * - \llrid{LLR_INTERRUPT_PROCESS_EXCEPTION_02}: If the vector of `frame` has no
 *   entry in `exceptions` (the name of its element is NULL),
 *   `process_exception()` shall take the name `"reserved"` and call
 *   `pok_raise_error()` once with `POK_ERROR_ID_UNHANDLED_INT`, `from_user` and
 *   `rip` of `frame` as the address.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_PROCESS_EXCEPTION_01}
 * - \llrid{LLR_INTERRUPT_PROCESS_EXCEPTION_03}: If the vector of `frame` is 14,
 *   `process_exception()` shall call, before `pok_raise_error()`,
 *   `dump_registers()` with `frame` and then `printf_debug()` with `"[KERNEL]
 *   Raise exception %%s at 0x%%lx. RIP=0x%%lx\n"`, the name, the result of
 *   `cpu_read_cr2()` and `rip` of `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_PROCESS_EXCEPTION_01}
 * - \llrid{LLR_INTERRUPT_PROCESS_EXCEPTION_04}: If the vector of `frame` is not
 *   14, `process_exception()` shall call, before `pok_raise_error()`,
 *   `dump_registers()` with `frame` and then `printf_debug()` with `"[KERNEL]
 *   Raise exception %%s. RIP=0x%%lx\n"`, the name and `rip` of `frame`, without
 *   calling `cpu_read_cr2()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_PROCESS_EXCEPTION_01}
 * - \llrid{LLR_INTERRUPT_PROCESS_EXCEPTION_05}: If and only if `from_user` is
 *   false, `process_exception()` shall call `pok_fatal()` with `"Unrecoverable
 *   exception in kernel"` after `pok_raise_error()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_PROCESS_EXCEPTION_02}
 *
 * @design
 * 1. Set `vector` to `frame->vector`.
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_01},
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_02},
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_03},
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_04}
 * 2. Set `name` to `exceptions[vector].name` if `exceptions[vector].name` is
 *    not NULL, otherwise `"reserved"`.
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_01},
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_02}
 * 3. Set `error_id` to `exceptions[vector].error_id` if
 *    `exceptions[vector].name` is not NULL, otherwise
 *    `POK_ERROR_ID_UNHANDLED_INT`. \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_01},
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_02}
 * 4. Call `dump_registers()` with the argument `frame`.
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_03},
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_04}
 * 5. If `vector` is equal to `14U`: \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_03}
 *    1. Call `cpu_read_cr2()`, and store the result in `cr2`.
 *    2. Evaluate
 *       `printf_debug("[KERNEL] Raise exception %%s at 0x%%lx. RIP=0x%%lx\n",`
 *       `name, (unsigned long)cr2, (unsigned long)frame->rip)`.
 * 6. Otherwise: \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_04}
 *    1. Evaluate
 *       `printf_debug("[KERNEL] Raise exception %%s. RIP=0x%%lx\n", name,`
 *       `(unsigned long)frame->rip)`.
 * 7. Call `pok_raise_error()` with the arguments `error_id`, `from_user` and
 *    `(void*)frame->rip`. \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_01},
 *    \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_02}
 * 8. If `from_user` is false: \llrref{LLR_INTERRUPT_PROCESS_EXCEPTION_05}
 *    1. Call `pok_fatal()` with the argument
 *       `"Unrecoverable exception in kernel"`.
 *
 * @calls
 * - `dump_registers()`
 * - `cpu_read_cr2()`
 * - `printf()`
 * - `pok_raise_error()`
 * - `pok_fatal()`
 */
static void process_exception(interrupt_frame* frame, pok_bool_t from_user)
{
   /* Same mapping as for (32-bit) x86. */
   static const exception_info_t exceptions[EXCEPTION_COUNT] =
   {
      [0]  = {"divide by zero error", POK_ERROR_ID_NUMERIC_ERROR},
      [1]  = {"debug", POK_ERROR_ID_ILLEGAL_REQUEST},
      [2]  = {"NMI fault", POK_ERROR_ID_ILLEGAL_REQUEST},
      [3]  = {"breakpoint", POK_ERROR_ID_ILLEGAL_REQUEST},
      [4]  = {"overflow", POK_ERROR_ID_STACK_OVERFLOW},
      [5]  = {"bound range fault", POK_ERROR_ID_MEMORY_VIOLATION},
      [6]  = {"invalid opcode fault", POK_ERROR_ID_ILLEGAL_REQUEST},
      [7]  = {"no math coprocessor fault", POK_ERROR_ID_ILLEGAL_REQUEST},
      [8]  = {"double fault", POK_ERROR_ID_UNHANDLED_INT},
      [9]  = {"copseg overrun fault", POK_ERROR_ID_MEMORY_VIOLATION},
      [10] = {"invalid TSS fault", POK_ERROR_ID_MEMORY_VIOLATION},
      [11] = {"segment not present fault", POK_ERROR_ID_MEMORY_VIOLATION},
      [12] = {"stack segment fault", POK_ERROR_ID_MEMORY_VIOLATION},
      [13] = {"general protection fault", POK_ERROR_ID_ILLEGAL_REQUEST},
      [14] = {"pagefault", POK_ERROR_ID_MEMORY_VIOLATION},
      [16] = {"FPU fault", POK_ERROR_ID_HARDWARE_FAULT},
      [17] = {"alignement check fault", POK_ERROR_ID_HARDWARE_FAULT},
      [18] = {"machine check fault", POK_ERROR_ID_HARDWARE_FAULT},
      [19] = {"SIMD fault", POK_ERROR_ID_HARDWARE_FAULT},
   };

   uint32_t vector = frame->vector;
   const char* name = (exceptions[vector].name != NULL)
       ? exceptions[vector].name : "reserved";
   pok_error_id_t error_id = (exceptions[vector].name != NULL) ?
      exceptions[vector].error_id : POK_ERROR_ID_UNHANDLED_INT;

   dump_registers(frame);
   if (vector == 14U)
   {
      uint64_t cr2 = cpu_read_cr2();
      printf_debug("[KERNEL] Raise exception %s at 0x%lx. RIP=0x%lx\n",
         name, (unsigned long)cr2, (unsigned long)frame->rip);
   }
   else
   {
      printf_debug("[KERNEL] Raise exception %s. RIP=0x%lx\n",
         name, (unsigned long)frame->rip);
   }

   /* MISRA C:2025 Rule 11.6 deviation: the faulting instruction address is
    * reported as a pointer. */
   pok_raise_error(error_id, from_user, (void*)frame->rip);

   if (!from_user)
   {
      // Returning would re-execute the faulting instruction.
      pok_fatal("Unrecoverable exception in kernel");
   }
}

/**
 * @funcname `ja_interrupt_handler`
 *
 * @description
 * The common interrupt handler (from interrupt_entries.S): a syscall, the
 * timer, a spurious IRQ, a GDB trap or an exception; on the way back to user
 * space it sets the kernel stack of the thread that returns and restores its FP
 * registers.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The interrupt frame. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `global_thread_stack` | `interrupt_frame *` | The frame of the interrupted thread, for the GDB server |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `from_user` | `pok_bool_t` | Whether it came from user space |
 * | `handled` | `pok_bool_t` | Whether GDB handled the trap |
 * | `previous_frame` | `interrupt_frame *` | The frame before |
 * | `vector` | `uint64_t` | The vector |
 *
 * @hlr
 * - \hlrid{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}: `ja_interrupt_handler()`
 *   shall dispatch an interrupt or exception: a system call, the timer tick, a
 *   spurious interrupt of the board (ignored), a trap of the debugger, or an
 *   exception.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_INT_001}
 *   - \srqref{SRQ_JET_INT_006}
 *   - \srqref{SRQ_JET_SYS_002}
 * - \hlrid{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_02}: `ja_interrupt_handler()`
 *   shall, on the way back to user mode, set the kernel stack of the next
 *   interrupt and put the floating point state of the current process into the
 *   processor.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_007}
 *   - \srqref{SRQ_JET_THR_023}
 *   - \srqref{SRQ_JET_THR_024}
 *
 * @llr
 * - \llrid{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}: `ja_interrupt_handler()`
 *   shall set `global_thread_stack` to `frame` before it handles the vector of
 *   `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}
 * - \llrid{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_02}: If and only if the low 2
 *   bits of `cs` of `frame` are not 3 (the frame is not from user mode),
 *   `ja_interrupt_handler()` shall set `global_thread_stack` back to its value
 *   at entry after it has handled the vector.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}
 * - \llrid{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_03}: If the vector of `frame` is
 *   `EXCEPTION_SYSCALL`, `ja_interrupt_handler()` shall call
 *   `process_syscall()` once with `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}
 * - \llrid{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_04}: If the vector of `frame` is
 *   `EXCEPTION_TIMER`, `ja_interrupt_handler()` shall call
 *   `ja_bsp_process_timer()` once with `frame`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}
 * - \llrid{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_05}: If the vector of `frame` is
 *   neither `EXCEPTION_SYSCALL` nor `EXCEPTION_TIMER` and is from
 *   `IRQ_VECTOR_FIRST` to `IRQ_VECTOR_LAST`, `ja_interrupt_handler()` shall
 *   call none of `process_syscall()`, `ja_bsp_process_timer()`, `ja_gdb_trap()`
 *   and `process_exception()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}
 * - \llrid{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_06}: If the vector of `frame` is
 *   `EXCEPTION_DEBUG` or `EXCEPTION_BREAKPOINT` and `ja_gdb_trap()` returns
 *   true for `frame`, the vector and whether the frame is from user mode,
 *   `ja_interrupt_handler()` shall not call `process_exception()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}
 * - \llrid{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_07}: If the vector of `frame` is
 *   `EXCEPTION_DEBUG` or `EXCEPTION_BREAKPOINT` and `ja_gdb_trap()` returns
 *   false for `frame`, the vector and whether the frame is from user mode,
 *   `ja_interrupt_handler()` shall call `process_exception()` once with `frame`
 *   and whether the frame is from user mode.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}
 * - \llrid{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_08}: If the vector of `frame` is
 *   none of `EXCEPTION_SYSCALL`, `EXCEPTION_TIMER`, `EXCEPTION_DEBUG` and
 *   `EXCEPTION_BREAKPOINT`, and is not from `IRQ_VECTOR_FIRST` to
 *   `IRQ_VECTOR_LAST`, `ja_interrupt_handler()` shall call
 *   `process_exception()` once with `frame` and whether the frame is from user
 *   mode, without calling `ja_gdb_trap()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}
 * - \llrid{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_09}: If and only if the low 2
 *   bits of `cs` of `frame` are 3 (the frame is from user mode),
 *   `ja_interrupt_handler()` shall, after it has handled the vector, call
 *   `ja_tss_set_rsp0()` with the address that follows the frame (`frame + 1`)
 *   and then `pok_partition_return_user_fp()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_INTERRUPT_JA_INTERRUPT_HANDLER_02}
 *
 * @design
 * 1. Set `from_user` to `(frame->cs & 3U) == 3U`.
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_02},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_06},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_07},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_08},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_09}
 * 2. Set `vector` to `frame->vector`.
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_03},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_04},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_05},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_06},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_07},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_08}
 * 3. Set `previous_frame` to `global_thread_stack`.
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_02}
 * 4. Set `global_thread_stack` to `frame`.
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_01}
 * 5. If `vector` is equal to `EXCEPTION_SYSCALL`:
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_03}
 *    1. Call `process_syscall()` with the argument `frame`.
 * 6. Otherwise, if `vector` is equal to `EXCEPTION_TIMER`:
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_04}
 *    1. Call `ja_bsp_process_timer()` with the argument `frame`.
 * 7. Otherwise, if `vector` is greater than or equal to `IRQ_VECTOR_FIRST` and
 *    `vector` is less than or equal to `IRQ_VECTOR_LAST`:
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_05}
 * 8. Otherwise: \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_06},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_07},
 *    \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_08}
 *    1. Set `handled` to `FALSE`.
 *    2. If `vector` is equal to `EXCEPTION_DEBUG` or `vector` is equal to
 *       `EXCEPTION_BREAKPOINT`: \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_06},
 *       \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_07}
 *       1. Call `ja_gdb_trap()` with the arguments `frame`, `(uint32_t)vector`
 *          and `from_user`, and store the result in `handled`.
 *    3. If `handled` is false: \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_06},
 *       \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_07},
 *       \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_08}
 *       1. Call `process_exception()` with the arguments `frame` and
 *          `from_user`. \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_07},
 *          \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_08}
 * 9. If `from_user` is false: \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_02}
 *    1. Set `global_thread_stack` to `previous_frame`.
 * 10. If `from_user` is true: \llrref{LLR_INTERRUPT_JA_INTERRUPT_HANDLER_09}
 *     1. Call `ja_tss_set_rsp0()` with the argument `(uint64_t)(frame + 1)`.
 *     2. Call `pok_partition_return_user_fp()`.
 *
 * @calls
 * - `process_syscall()`
 * - `ja_bsp_process_timer()`
 * - `ja_gdb_trap()`
 * - `process_exception()`
 * - `ja_tss_set_rsp0()`
 * - `pok_partition_return_user_fp()`
 */
void ja_interrupt_handler(interrupt_frame* frame)
{
   pok_bool_t from_user = (frame->cs & 3U) == 3U;
   uint64_t vector = frame->vector;

#ifdef POK_NEEDS_GDB
   // Registers of the interrupted thread, for the GDB server.
   interrupt_frame* previous_frame = global_thread_stack;
   global_thread_stack = frame;
#endif /* POK_NEEDS_GDB */

   if (vector == EXCEPTION_SYSCALL)
   {
      process_syscall(frame);
   }
   else if (vector == EXCEPTION_TIMER)
   {
      ja_bsp_process_timer(frame);
   }
   else if ((vector >= IRQ_VECTOR_FIRST) && (vector <= IRQ_VECTOR_LAST))
   {
      // Other IRQs are masked; this may be a spurious IRQ7/IRQ15 from the PIC.
   }
   else
   {
      pok_bool_t handled = FALSE;

#ifdef POK_NEEDS_GDB
      if ((vector == EXCEPTION_DEBUG) || (vector == EXCEPTION_BREAKPOINT))
      {
         handled = ja_gdb_trap(frame, (uint32_t)vector, from_user);
      }
#endif /* POK_NEEDS_GDB */

      if (!handled)
      {
         process_exception(frame, from_user);
      }
   }

#ifdef POK_NEEDS_GDB
   if (!from_user)
   {
      // Back in the kernel code which was interrupted, and to its frame.
      global_thread_stack = previous_frame;
   }
#endif /* POK_NEEDS_GDB */

   if (from_user)
   {
      /*
       * Returning to user space, possibly in another thread than the one
       * which was interrupted: the next interrupt from user space should
       * use the kernel stack of that thread, which ends where its frame
       * ends. FP registers are switched lazily, on return to user space.
       */
      ja_tss_set_rsp0((uint64_t)(frame + 1));
      pok_partition_return_user_fp();
   }
}
