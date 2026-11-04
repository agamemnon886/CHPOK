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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: characters are
 * emitted as char; the header guard is renamed.
 */

/* Emits characters, produced by printf-like format, in a generic way.*/

#ifndef LIBJET_STDIO_PRINTF_EMITTER_H
#define LIBJET_STDIO_PRINTF_EMITTER_H

/*
 * MISRA C:2025 Rule 17.1 deviation: the emitter formats the variable
 * arguments of the printf() family of libpok's C library.
 */
#include <stdarg.h>

/* Emit single character. */
typedef void (*emit_character_t)(char c, void *private_data);

/* For each character, determined by format and args, call emit_character().*/
void printf_emitter(emit_character_t emit_character, void *private_data,
    const char* format, va_list arg);

#endif /* LIBJET_STDIO_PRINTF_EMITTER_H */
