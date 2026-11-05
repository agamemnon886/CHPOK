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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
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

typedef struct
{
  uint16_t offset_low;
  uint16_t selector;
  uint8_t  ist;
  uint8_t  type_attr;
  uint16_t offset_mid;
  uint32_t offset_high;
  uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

#define IDT_SIZE 256
/* Present, 64-bit interrupt gate (interrupts are disabled on entry). */
#define IDT_INTERRUPT_GATE(dpl) ((uint8_t) (0x8EU | ((dpl) << 5)))

static idt_entry_t pok_idt[IDT_SIZE] __attribute__((aligned(16)));

/* Defined in interrupt_entries.S. */
extern void (* const ja_interrupt_stubs[IRQ_VECTOR_LAST + 1U])(void);
void ja_interrupt_stub_syscall(void);

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

/* Address of the last page fault. */
static uint64_t cpu_read_cr2(void)
{
   uint64_t cr2;

   asm volatile ("mov %%cr2, %0" : "=r" (cr2));
   return cr2;
}

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

static void process_exception(interrupt_frame* frame, pok_bool_t from_user)
{
   /* Same mapping as for (32-bit) x86. */
   static const struct
   {
      const char* name;
      pok_error_id_t error_id;
   } exceptions[EXCEPTION_COUNT] =
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

void ja_interrupt_handler(interrupt_frame* frame)
{
   pok_bool_t from_user = (frame->cs & 3U) == 3U;
   uint64_t vector = frame->vector;

#ifdef POK_NEEDS_GDB
   // Registers of the interrupted thread, for the GDB server.
   struct jet_interrupt_context* previous_frame = global_thread_stack;
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
