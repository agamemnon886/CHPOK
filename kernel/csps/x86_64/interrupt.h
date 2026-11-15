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
 * kernel/arch/x86/interrupt.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants.
 * Modified by Kirill Brazhnikov, 2026-11-04: vectors of the debug and
 * breakpoint exceptions; ja_gdb_trap() for the GDB server.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; Doxygen comments for the file and its macros.
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, the types and the
 * declarations of kernel/arch/x86_64/interrupt.c are moved here; the Doxygen
 * comment of the file is removed (header files are not documented).
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/x86_64/interrupt.h (the CPU support packages are in
 * kernel/csps); the comments name the new paths.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_INTERRUPT_H
#define POK_INTERRUPT_H

/**
 * @macroname `EXCEPTION_SYSCALL`
 *
 * @description
 * Vector of the syscall gate (int $0x80), see libpok/arch/x86_64/syscall.c.
 */
#define EXCEPTION_SYSCALL 0x80U

/**
 * @macroname `EXCEPTION_DEBUG`
 *
 * @description
 * The debug exception (single step, debug registers).
 */
#define EXCEPTION_DEBUG 1U

/**
 * @macroname `EXCEPTION_BREAKPOINT`
 *
 * @description
 * The breakpoint exception (int3).
 */
#define EXCEPTION_BREAKPOINT 3U

/**
 * @macroname `EXCEPTION_COUNT`
 *
 * @description
 * The number of exception vectors (the IRQs follow them, remapped by the PIC,
 * see kernel/bsps/).
 */
#define EXCEPTION_COUNT 32

/**
 * @macroname `IRQ_VECTOR_FIRST`
 *
 * @description
 * The vector of IRQ 0.
 */
#define IRQ_VECTOR_FIRST 32U

/**
 * @macroname `IRQ_VECTOR_LAST`
 *
 * @description
 * The vector of IRQ 15.
 */
#define IRQ_VECTOR_LAST  47U

#ifndef __ASSEMBLER__

#include <config.h>
#include <types.h>
#include <arch/interrupt_frame.h>
#include <stdint.h>
#include <core/error.h>

/* Fill the IDT and load it. */
/** \cond */
void ja_idt_init(void);

/* Called from interrupt_entries.S for every interrupt. */
void ja_interrupt_handler(interrupt_frame* frame);

void process_syscall(interrupt_frame* frame);

/** \endcond */

#ifdef POK_NEEDS_GDB
/*
 * Debug or breakpoint exception: pass it to the GDB server (gdb.c).
 *
 * Return FALSE if it isn't for the GDB server, so it is handled as an
 * ordinary exception.
 */
/** \cond */
pok_bool_t ja_gdb_trap(interrupt_frame* frame, uint32_t vector,
    pok_bool_t from_user);

/** \endcond */
#endif /* POK_NEEDS_GDB */

/**
 * @typename `idt_entry_t`
 *
 * @description
 * An entry (gate descriptor) of the IDT: the address of the handler of an
 * interrupt vector, its code segment, its stack (IST) and its type.
 */
typedef struct
{
  uint16_t offset_low;  /**< Bits 0 to 15 of the handler's address. */
  uint16_t selector;    /**< The code segment selector of the handler. */
  uint8_t  ist;         /**< The interrupt stack table entry (0: none). */
  uint8_t  type_attr;   /**< The type and attributes (IDT_INTERRUPT_GATE). */
  uint16_t offset_mid;  /**< Bits 16 to 31 of the handler's address. */
  uint32_t offset_high; /**< Bits 32 to 63 of the handler's address. */
  uint32_t reserved;    /**< Reserved (0). */
} __attribute__((packed)) idt_entry_t;

/**
 * @macroname `IDT_SIZE`
 *
 * @description
 * The number of entries of the IDT (every interrupt vector).
 */
#define IDT_SIZE 256

/**
 * @macroname `IDT_INTERRUPT_GATE`
 *
 * @description
 * Present, 64-bit interrupt gate (interrupts are disabled on entry): the
 * type_attr of an IDT entry.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `dpl` | The privilege level that may use the gate with an INT instruction (0: the kernel only, 3: partitions too). |
 */
#define IDT_INTERRUPT_GATE(dpl) ((uint8_t) (0x8EU | ((dpl) << 5)))

/**
 * @typename `exception_info_t`
 *
 * @description
 * What the kernel does for a CPU exception (an element of the table of
 * process_exception()): its name, for the messages, and the error raised for
 * it.
 */
typedef struct
{
   const char* name;         /**< The name of the exception. */
   pok_error_id_t error_id;  /**< The error raised for it. */
} exception_info_t;

/** \cond */
/* Defined in interrupt_entries.S. */
extern void (* const ja_interrupt_stubs[IRQ_VECTOR_LAST + 1U])(void);
void ja_interrupt_stub_syscall(void);
/** \endcond */

#endif /* __ASSEMBLER__ */

#endif /* !POK_INTERRUPT_H */
