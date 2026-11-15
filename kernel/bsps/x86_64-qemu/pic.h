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
 * boards/x86-qemu/pic.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; pok_pic_init() returns void.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; Doxygen comments for the file and its macros.
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/x86_64-qemu/pic.h (the board support packages are in kernel/bsps).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_X86_PIC_H
#define POK_X86_PIC_H
#include <stdint.h>

/**
 * @name 8259 PIC ports
 * The I/O ports of the two 8259 interrupt controllers (command
 * port; the data port is the next one).
 * @{
 */

/**
 * @macroname `PIC_MASTER_BASE`
 *
 * @description
 * The command port of the master PIC (IRQs 0 to 7).
 */
#define PIC_MASTER_BASE 0x20

/**
 * @macroname `PIC_SLAVE_BASE`
 *
 * @description
 * The command port of the slave PIC (IRQs 8 to 15).
 */
#define PIC_SLAVE_BASE 0xa0

/** @} */

/**
 * @name 8259 PIC initialization
 * The initialization command words (ICW1 to ICW4) that pok_pic_init()
 * writes to the master and the slave PIC.
 * @{
 */

/**
 * @macroname `PIC_MASTER_ICW1`
 *
 * @description
 * Master ICW1: initialization, edge triggered, cascaded, ICW4 follows.
 */
#define PIC_MASTER_ICW1 0x11

/**
 * @macroname `PIC_MASTER_ICW2`
 *
 * @description
 * Master ICW2: the vector of IRQ 0 (32; IRQs 0 to 7 are vectors 32 to
 * 39).
 */
#define PIC_MASTER_ICW2 0x20

/**
 * @macroname `PIC_MASTER_ICW3`
 *
 * @description
 * Master ICW3: the slave is connected to IRQ 2 (bit 2).
 */
#define PIC_MASTER_ICW3 0x04

/**
 * @macroname `PIC_MASTER_ICW4`
 *
 * @description
 * Master ICW4: 8086 mode.
 */
#define PIC_MASTER_ICW4 0x01

/**
 * @macroname `PIC_SLAVE_ICW1`
 *
 * @description
 * Slave ICW1: initialization, edge triggered, cascaded, ICW4 follows.
 */
#define PIC_SLAVE_ICW1 0x11

/**
 * @macroname `PIC_SLAVE_ICW2`
 *
 * @description
 * Slave ICW2: the vector of IRQ 8 (40; IRQs 8 to 15 are vectors 40 to
 * 47).
 */
#define PIC_SLAVE_ICW2 0x28

/**
 * @macroname `PIC_SLAVE_ICW3`
 *
 * @description
 * Slave ICW3: its cascade identity (it is on IRQ 2 of the master).
 */
#define PIC_SLAVE_ICW3 0x02

/**
 * @macroname `PIC_SLAVE_ICW4`
 *
 * @description
 * Slave ICW4: 8086 mode.
 */
#define PIC_SLAVE_ICW4 0x01

/** @} */

/** \cond */
void		pok_pic_init(void);
int32_t		pok_pic_mask(uint8_t irq);
int32_t		pok_pic_unmask(uint8_t irq);
void		pok_pic_eoi(uint8_t irq);
/** \endcond */

#endif /* __POK_X86_PIC_H_ */
