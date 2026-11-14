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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; discarded results are cast to void;
 * pok_fatal() is _Noreturn; printf_debug() discards the result of
 * printf() itself.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; Doxygen comments for the file and its macros; the unused macro
 * POK_FATAL is removed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the header of the object-like
 * printf_debug has no parameter table.
 */

#include <config.h>

#ifndef POK_KERNEL_DEBUG_H
#define POK_KERNEL_DEBUG_H

#ifdef POK_NEEDS_DEBUG

#include <errno.h>
#include <types.h>
#include <libc.h>

/** \cond */
__attribute__((__noreturn__))
_Noreturn void pok_fatal(const char* message);

int32_t  debug_strlen (const char* str);

void hexdump (const void *addr, int32_t len);

/** \endcond */

/**
 * @macroname `printf_debug`
 *
 * @description
 * Print formatted message if debugging enabled: expands to `(void) printf`, so
 * that printf_debug(format, ...) calls printf() and discards its result.
 * Otherwise do nothing (the other definition).
 */
#define printf_debug (void) printf

/** \cond */
void pok_debug_print_current_state(void);
/** \endcond */

/**
 * @macroname `POK_DEBUG_PRINT_CURRENT_STATE`
 *
 * @description
 * Prints the state of the kernel (pok_debug_print_current_state()); without
 * POK_NEEDS_DEBUG, nothing.
 */
#define POK_DEBUG_PRINT_CURRENT_STATE pok_debug_print_current_state();

#else /* POK_NEEDS_DEBUG */

/**
 * @macroname `printf_debug`
 *
 * @description
 * Print formatted message if debugging enabled (printf()). Otherwise do nothing
 * (the other definition).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `...` | The format and its arguments (ignored). |
 */
#define printf_debug(...) ((void) 0)

/**
 * @macroname `POK_DEBUG_PRINT_CURRENT_STATE`
 *
 * @description
 * Prints the state of the kernel (pok_debug_print_current_state()); without
 * POK_NEEDS_DEBUG, nothing.
 */
#define POK_DEBUG_PRINT_CURRENT_STATE

#endif /* POK_NEEDS_DEBUG */

#endif /* __POK_KERNELDEBUG_H__ */

