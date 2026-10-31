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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <libc.h>
#include <core/debug.h>
#include <core/error.h>
#include <core/sched.h>
#include <bsp/bsp.h>

#include "interrupt.h"
#include "gdt.h"
#include "sysdesc.h"

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
#define IDT_INTERRUPT_GATE(dpl) (0x8E | ((dpl) << 5))

static idt_entry_t pok_idt[IDT_SIZE] __attribute__((aligned(16)));

/* Defined in interrupt_entries.S. */
extern void (* const ja_interrupt_stubs[IRQ_VECTOR_LAST + 1])(void);
void ja_interrupt_stub_syscall(void);

static void idt_set_gate(unsigned index, void (*entry)(void), int dpl)
{
   uint64_t offset = (uint64_t)entry;

   pok_idt[index].offset_low  = offset & 0xFFFF;
   pok_idt[index].selector    = GDT_KERNEL_CODE;
   pok_idt[index].ist         = 0;
   pok_idt[index].type_attr   = IDT_INTERRUPT_GATE(dpl);
   pok_idt[index].offset_mid  = (offset >> 16) & 0xFFFF;
   pok_idt[index].offset_high = offset >> 32;
   pok_idt[index].reserved    = 0;
}

void ja_idt_init(void)
{
   sysdesc_t sysdesc;

   memset(pok_idt, 0, sizeof(pok_idt));

   for (unsigned i = 0; i <= IRQ_VECTOR_LAST; i++)
      idt_set_gate(i, ja_interrupt_stubs[i], 0);

   // The only gate user space may use.
   idt_set_gate(EXCEPTION_SYSCALL, ja_interrupt_stub_syscall, 3);

   sysdesc.limit = sizeof(pok_idt) - 1;
   sysdesc.base = (uint64_t)pok_idt;
   asm volatile ("lidt %0" : : "m" (sysdesc));
}

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
   unsigned vector = frame->vector;
   const char* name = exceptions[vector].name ? exceptions[vector].name : "reserved";
   pok_error_id_t error_id = exceptions[vector].name ?
      exceptions[vector].error_id : POK_ERROR_ID_UNHANDLED_INT;

   dump_registers(frame);
   if (vector == 14)
   {
      uint64_t cr2;
      asm volatile ("mov %%cr2, %0" : "=r" (cr2));
      printf_debug("[KERNEL] Raise exception %s at 0x%lx. RIP=0x%lx\n",
         name, (unsigned long)cr2, (unsigned long)frame->rip);
   }
   else
   {
      printf_debug("[KERNEL] Raise exception %s. RIP=0x%lx\n",
         name, (unsigned long)frame->rip);
   }

   pok_raise_error(error_id, from_user, (void*)frame->rip);

   if (!from_user)
   {
      // Returning would re-execute the faulting instruction.
      pok_fatal("Unrecoverable exception in kernel");
   }
}

void ja_interrupt_handler(interrupt_frame* frame)
{
   pok_bool_t from_user = (frame->cs & 3) == 3;
   uint64_t vector = frame->vector;

   if (vector == EXCEPTION_SYSCALL)
   {
      process_syscall(frame);
   }
   else if (vector == EXCEPTION_TIMER)
   {
      ja_bsp_process_timer(frame);
   }
   else if (vector >= IRQ_VECTOR_FIRST && vector <= IRQ_VECTOR_LAST)
   {
      // Other IRQs are masked; this may be a spurious IRQ7/IRQ15 from the PIC.
   }
   else
   {
      process_exception(frame, from_user);
   }

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
