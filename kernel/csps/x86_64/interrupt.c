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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `lidt %0`.
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
 *
 * @llr
 *
 * @design
 * 1. Execute the assembly code `mov %%%cr2, %0`.
 * 2. Return `cr2`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `offset` to `(uint64_t)entry`.
 * 2. Set `pok_idt[index].offset_low` to `(uint16_t) (offset & 0xFFFFU)`.
 * 3. Set `pok_idt[index].selector` to `GDT_KERNEL_CODE`.
 * 4. Set `pok_idt[index].ist` to `0`.
 * 5. Set `pok_idt[index].type_attr` to `IDT_INTERRUPT_GATE(dpl)`.
 * 6. Set `pok_idt[index].offset_mid` to
 *    `(uint16_t) ((offset >> 16) & 0xFFFFU)`.
 * 7. Set `pok_idt[index].offset_high` to `(uint32_t) (offset >> 32)`.
 * 8. Set `pok_idt[index].reserved` to `0`.
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
 *
 * @llr
 *
 * @design
 * 1. Call `memset()` with the arguments `pok_idt`, `0` and `sizeof(pok_idt)`.
 * 2. Set `i` to `0`; then while `i` is less than or equal to `IRQ_VECTOR_LAST`,
 *    repeat (after each pass: increment `i`):
 *    1. Call `idt_set_gate()` with the arguments `i`, `ja_interrupt_stubs[i]`
 *       and `0`.
 * 3. Call `idt_set_gate()` with the arguments `EXCEPTION_SYSCALL`,
 *    `ja_interrupt_stub_syscall` and `3`.
 * 4. Call `idt_set_gate()` with the arguments `EXCEPTION_BREAKPOINT`,
 *    `ja_interrupt_stubs[EXCEPTION_BREAKPOINT]` and `3`.
 * 5. Set `sysdesc.limit` to `sizeof(pok_idt) - 1U`.
 * 6. Set `sysdesc.base` to `(uint64_t)pok_idt`.
 * 7. Call `cpu_load_idt()` with the address of `sysdesc`.
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
 *
 * @llr
 *
 * @design
 * 1. Evaluate `printf_debug("CS: %%lx, SS: %%lx, RFLAGS: %%lx\n", (unsigned`
 *    `long)frame->cs, (unsigned long)frame->ss, (unsigned long)frame->rflags)`.
 * 2. Evaluate
 *    `printf_debug("RIP: %%lx, RSP: %%lx, RBP: %%lx, ErrorCode: %%lx\n",`
 *    `(unsigned`
 *    `long)frame->rip, (unsigned long)frame->rsp, (unsigned long)frame->rbp,`
 *    `(unsigned long)frame->error)`.
 * 3. Evaluate
 *    `printf_debug("RAX: %%lx, RBX: %%lx, RCX: %%lx, RDX: %%lx\n", (unsigned`
 *    `long)frame->rax, (unsigned long)frame->rbx, (unsigned long)frame->rcx,`
 *    `(unsigned long)frame->rdx)`.
 * 4. Evaluate
 *    `printf_debug("RSI: %%lx, RDI: %%lx\n", (unsigned long)frame->rsi,`
 *    `(unsigned long)frame->rdi)`.
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
 *
 * @llr
 *
 * @design
 * 1. Set `vector` to `frame->vector`.
 * 2. Set `name` to `exceptions[vector].name` if `exceptions[vector].name` is
 *    not NULL, otherwise `"reserved"`.
 * 3. Set `error_id` to `exceptions[vector].error_id` if
 *    `exceptions[vector].name` is not NULL, otherwise
 *    `POK_ERROR_ID_UNHANDLED_INT`.
 * 4. Call `dump_registers()` with the argument `frame`.
 * 5. If `vector` is equal to `14U`:
 *    1. Call `cpu_read_cr2()`, and store the result in `cr2`.
 *    2. Evaluate
 *       `printf_debug("[KERNEL] Raise exception %%s at 0x%%lx. RIP=0x%%lx\n",`
 *       `name, (unsigned long)cr2, (unsigned long)frame->rip)`.
 * 6. Otherwise:
 *    1. Evaluate
 *       `printf_debug("[KERNEL] Raise exception %%s. RIP=0x%%lx\n", name,`
 *       `(unsigned long)frame->rip)`.
 * 7. Call `pok_raise_error()` with the arguments `error_id`, `from_user` and
 *    `(void*)frame->rip`.
 * 8. If `from_user` is false:
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
 *
 * @llr
 *
 * @design
 * 1. Set `from_user` to `(frame->cs & 3U) == 3U`.
 * 2. Set `vector` to `frame->vector`.
 * 3. Set `previous_frame` to `global_thread_stack`.
 * 4. Set `global_thread_stack` to `frame`.
 * 5. If `vector` is equal to `EXCEPTION_SYSCALL`:
 *    1. Call `process_syscall()` with the argument `frame`.
 * 6. Otherwise, if `vector` is equal to `EXCEPTION_TIMER`:
 *    1. Call `ja_bsp_process_timer()` with the argument `frame`.
 * 7. Otherwise, if `vector` is greater than or equal to `IRQ_VECTOR_FIRST` and
 *    `vector` is less than or equal to `IRQ_VECTOR_LAST`:
 *    1. Do nothing.
 * 8. Otherwise:
 *    1. Set `handled` to `FALSE`.
 *    2. If `vector` is equal to `EXCEPTION_DEBUG` or `vector` is equal to
 *       `EXCEPTION_BREAKPOINT`:
 *       1. Call `ja_gdb_trap()` with the arguments `frame`, `(uint32_t)vector`
 *          and `from_user`, and store the result in `handled`.
 *    3. If `handled` is false:
 *       1. Call `process_exception()` with the arguments `frame` and
 *          `from_user`.
 * 9. If `from_user` is false:
 *    1. Set `global_thread_stack` to `previous_frame`.
 * 10. If `from_user` is true:
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
