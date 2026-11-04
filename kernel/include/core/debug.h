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
 */

#include <config.h>

#ifndef POK_KERNEL_DEBUG_H
#define POK_KERNEL_DEBUG_H

#ifdef POK_NEEDS_DEBUG

#include <errno.h>
#include <types.h>
#include <libc.h>

__attribute__((__noreturn__))
_Noreturn void pok_fatal(const char* message);

int32_t  debug_strlen (const char* str);

void hexdump (const void *addr, int32_t len);

/* Print formatted message if debugging enabled. Otherwise do nothing.*/
#define printf_debug (void) printf

#define POK_FATAL(arg) pok_fatal(arg)

void pok_debug_print_current_state(void);
#define POK_DEBUG_PRINT_CURRENT_STATE pok_debug_print_current_state();

#else /* POK_NEEDS_DEBUG */
#define printf_debug(...) ((void) 0)
#define POK_DEBUG_PRINT_CURRENT_STATE
#define POK_FATAL(arg)
#endif /* POK_NEEDS_DEBUG */

#endif /* __POK_KERNELDEBUG_H__ */

