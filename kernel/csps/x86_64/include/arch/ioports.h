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
 * kernel/arch/x86/include/arch/ioports.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; deviations are documented where a
 * guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its macros; the unused macro JET_ARCH_DECLARE_IO_PORT is removed.
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/x86_64/include/arch/ioports.h (the CPU support packages are in
 * kernel/csps); the macros call the I/O port functions shared with the
 * hypervisor (include/csps/x86_64/cpu/io.h).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */


#ifndef POK_X86_64_IOPORTS_H
#define POK_X86_64_IOPORTS_H
#include <stdint.h>

/*
 * The instructions are in the functions that the kernel shares with the
 * hypervisor; the macros below give them the names of the kernel's
 * architecture interface and convert their arguments.
 */
#include <cpu/io.h>

/**
 * @macroname `ja_outb`
 *
 * @description
 * Writes a byte to an I/O port.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 * | `data` | The value written. |
 */
#define ja_outb(port, data) x86_outb((uint16_t)(port), (uint8_t)(data))

/**
 * @macroname `ja_inb`
 *
 * @description
 * Reads a byte from an I/O port: the byte read.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 */
#define ja_inb(port) x86_inb((uint16_t)(port))

/**
 * @macroname `ja_outw`
 *
 * @description
 * Writes a 16-bit word to an I/O port.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 * | `data` | The value written. |
 */
#define ja_outw(port, data) \
    x86_outw((uint16_t)(port), (uint16_t)(data))

/**
 * @macroname `ja_inw`
 *
 * @description
 * Reads a 16-bit word from an I/O port: the word read.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 */
#define ja_inw(port) x86_inw((uint16_t)(port))

/**
 * @macroname `ja_outl`
 *
 * @description
 * Writes a 32-bit word to an I/O port.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 * | `data` | The value written. |
 */
#define ja_outl(port, data) \
    x86_outl((uint16_t)(port), (uint32_t)(data))

/**
 * @macroname `ja_inl`
 *
 * @description
 * Reads a 32-bit word from an I/O port: the word read.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `port` | The I/O port. |
 */
#define ja_inl(port) x86_inl((uint16_t)(port))

#endif /* POK_X86_64_IOPORTS_H */
