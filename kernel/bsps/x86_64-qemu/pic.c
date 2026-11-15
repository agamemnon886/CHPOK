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
 * boards/x86-qemu/pic.c to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; U suffixes for unsigned constants;
 * pok_pic_init() returns void (it always succeeded); the new masks are
 * computed as uint8_t.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the
 * file and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/x86_64-qemu/pic.c (the board support packages are in kernel/bsps);
 * the function headers follow outb() and inb(), which call x86_outb() and
 * x86_inb().
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * The 8259 programmable interrupt controllers (master and slave) of the
 * x86_64 QEMU board.
 */


#include <types.h>
#include <errno.h>

#include <ioports.h>

#include "pic.h"

/**
 * @funcname `pok_pic_init`
 *
 * @description
 * Initializes the master and slave 8259 interrupt controllers: IRQs 0-7 are
 * delivered as vectors 0x20-0x27 and IRQs 8-15 as 0x28-0x2f, the slave is
 * cascaded on IRQ 2, and every IRQ except the cascade is masked. Called once
 * by ja_bsp_init().
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
 * 1. Use the macro `outb (PIC_MASTER_BASE, PIC_MASTER_ICW1)`.
 * 2. Use the macro `outb (PIC_SLAVE_BASE, PIC_SLAVE_ICW1)`.
 * 3. Use the macro `outb (PIC_MASTER_BASE + 1, PIC_MASTER_ICW2)`.
 * 4. Use the macro `outb (PIC_SLAVE_BASE + 1, PIC_SLAVE_ICW2)`.
 * 5. Use the macro `outb (PIC_MASTER_BASE + 1, PIC_MASTER_ICW3)`.
 * 6. Use the macro `outb (PIC_SLAVE_BASE + 1, PIC_SLAVE_ICW3)`.
 * 7. Use the macro `outb (PIC_MASTER_BASE + 1, PIC_MASTER_ICW4)`.
 * 8. Use the macro `outb (PIC_SLAVE_BASE + 1, PIC_SLAVE_ICW4)`.
 * 9. Use the macro `outb (PIC_MASTER_BASE + 1, 0xfb)`.
 * 10. Use the macro `outb (PIC_SLAVE_BASE + 1, 0xff)`.
 *
 * @calls
 * - `x86_outb()`
 */
void pok_pic_init (void)
{
   outb (PIC_MASTER_BASE, PIC_MASTER_ICW1);
   outb (PIC_SLAVE_BASE, PIC_SLAVE_ICW1);

   outb (PIC_MASTER_BASE + 1, PIC_MASTER_ICW2);
   outb (PIC_SLAVE_BASE + 1, PIC_SLAVE_ICW2);

   outb (PIC_MASTER_BASE + 1, PIC_MASTER_ICW3);
   outb (PIC_SLAVE_BASE + 1, PIC_SLAVE_ICW3);

   outb (PIC_MASTER_BASE + 1, PIC_MASTER_ICW4);
   outb (PIC_SLAVE_BASE + 1, PIC_SLAVE_ICW4);

   /* Mask everything */
   outb (PIC_MASTER_BASE + 1, 0xfb);
   outb (PIC_SLAVE_BASE + 1, 0xff);
}

/**
 * @funcname `pok_pic_mask`
 *
 * @description
 * Masks (disables) one IRQ line of the interrupt controllers.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `irq` | `uint8_t` | [in] The IRQ line, 0-15. |
 *
 * @return POK_ERRNO_OK, or POK_ERRNO_EINVAL if irq is above 15.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mask` | `uint8_t` | The interrupt mask read from the controller |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `irq` is greater than `15U`:
 *    1. Return `(POK_ERRNO_EINVAL)`.
 * 2. If `irq` is less than `8U`:
 *    1. Set `mask` to `inb (PIC_MASTER_BASE + 1)`.
 *    2. Use the macro
 *       `outb (PIC_MASTER_BASE + 1, (uint8_t) (mask | (1U << irq)))`.
 * 3. Otherwise:
 *    1. Set `mask` to `inb (PIC_SLAVE_BASE + 1)`.
 *    2. Use the macro
 *       `outb (PIC_SLAVE_BASE + 1, (uint8_t) (mask | (1U << (irq - 8U))))`.
 * 4. Return `(POK_ERRNO_OK)`.
 *
 * @calls
 * - `x86_inb()`
 * - `x86_outb()`
 */
int32_t pok_pic_mask (uint8_t irq)
{
   uint8_t mask;

   if (irq > 15U)
   {
      return (POK_ERRNO_EINVAL);
   }

   if (irq < 8U)
   {
      mask = inb (PIC_MASTER_BASE + 1);
      outb (PIC_MASTER_BASE + 1, (uint8_t) (mask | (1U << irq)));
   }
   else
   {
      mask = inb (PIC_SLAVE_BASE + 1);
      outb (PIC_SLAVE_BASE + 1, (uint8_t) (mask | (1U << (irq - 8U))));
   }

   return (POK_ERRNO_OK);
}

/**
 * @funcname `pok_pic_unmask`
 *
 * @description
 * Unmasks (enables) one IRQ line of the interrupt controllers.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `irq` | `uint8_t` | [in] The IRQ line, 0-15. |
 *
 * @return POK_ERRNO_OK, or POK_ERRNO_EINVAL if irq is above 15.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `mask` | `uint8_t` | The interrupt mask read from the controller |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `irq` is greater than `15U`:
 *    1. Return `(POK_ERRNO_EINVAL)`.
 * 2. If `irq` is less than `8U`:
 *    1. Set `mask` to `inb(PIC_MASTER_BASE + 1)`.
 *    2. Use the macro
 *       `outb(PIC_MASTER_BASE + 1, (uint8_t) (mask & ~(1U << irq)))`.
 * 3. Otherwise:
 *    1. Set `mask` to `inb(PIC_SLAVE_BASE + 1)`.
 *    2. Use the macro
 *       `outb(PIC_SLAVE_BASE + 1, (uint8_t) (mask & ~(1U << (irq - 8U))))`.
 * 4. Return `(POK_ERRNO_OK)`.
 *
 * @calls
 * - `x86_inb()`
 * - `x86_outb()`
 */
int32_t pok_pic_unmask(uint8_t irq)
{
   uint8_t mask;

   if (irq > 15U) {
      return (POK_ERRNO_EINVAL);
   }

   if (irq < 8U)
   {
      mask = inb(PIC_MASTER_BASE + 1);
      outb(PIC_MASTER_BASE + 1, (uint8_t) (mask & ~(1U << irq)));
   }
   else
   {
      mask = inb(PIC_SLAVE_BASE + 1);
      outb(PIC_SLAVE_BASE + 1, (uint8_t) (mask & ~(1U << (irq - 8U))));
   }

   return (POK_ERRNO_OK);
}

/**
 * @funcname `pok_pic_eoi`
 *
 * @description
 * Signals the end of an interrupt to the interrupt controllers, so that they
 * deliver the next one. Called by the interrupt handlers of the board.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `irq` | `uint8_t` | [in] The IRQ line that was handled, 0-15. |
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
 * 1. If `irq` is greater than or equal to `8U`:
 *    1. Use the macro `outb (PIC_SLAVE_BASE, 0x20)`.
 * 2. Use the macro `outb (PIC_MASTER_BASE, 0x20)`.
 *
 * @calls
 * - `x86_outb()`
 */
void pok_pic_eoi (uint8_t irq)
{
   if (irq >= 8U)
   {
      outb (PIC_SLAVE_BASE, 0x20);
   }

   outb (PIC_MASTER_BASE, 0x20);
}

