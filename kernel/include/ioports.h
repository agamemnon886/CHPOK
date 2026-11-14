/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the descriptions that start with a
 * function name write it in lower case, as in the code.
 */

#ifndef POK_IOPORTS_H
#define POK_IOPORTS_H

#include <arch/ioports.h>

/**
 * @name I/O port accessors
 * The generic names of the I/O port accessors of the architecture.
 * @{
 */

/**
 * @macroname `outb`
 *
 * @description
 * ja_outb(): writes a byte to an I/O port.
 */
#define outb ja_outb

/**
 * @macroname `inb`
 *
 * @description
 * ja_inb(): reads a byte from an I/O port.
 */
#define inb ja_inb

/**
 * @macroname `outw`
 *
 * @description
 * ja_outw(): writes a 16-bit word to an I/O port.
 */
#define outw ja_outw

/**
 * @macroname `inw`
 *
 * @description
 * ja_inw(): reads a 16-bit word from an I/O port.
 */
#define inw ja_inw

/**
 * @macroname `outl`
 *
 * @description
 * ja_outl(): writes a 32-bit word to an I/O port.
 */
#define outl ja_outl

/**
 * @macroname `inl`
 *
 * @description
 * ja_inl(): reads a 32-bit word from an I/O port.
 */
#define inl ja_inl

/** @} */


#endif /* POK_IOPORTS_H */
