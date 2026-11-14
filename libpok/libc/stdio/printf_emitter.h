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
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the types of printf_emitter.c and
 * sprintf.c are moved here.
 */

/* Emits characters, produced by printf-like format, in a generic way.*/

#ifndef LIBJET_STDIO_PRINTF_EMITTER_H
#define LIBJET_STDIO_PRINTF_EMITTER_H

/*
 * MISRA C:2025 Rule 17.1 deviation: the emitter formats the variable
 * arguments of the printf() family of libpok's C library.
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @typename `emit_character_t`
 *
 * @description
 * Emits one character of a printf() variant: its parameters are the character
 * and the private data of the output; it returns nothing.
 */
typedef void (*emit_character_t)(char c, void *private_data);

/* For each character, determined by format and args, call emit_character().*/
/** \cond */
void printf_emitter(emit_character_t emit_character, void *private_data,
    const char* format, va_list arg);
/** \endcond */

/**
 * @typename `emitter_t`
 *
 * @description
 * Where the characters go: the output function of a printf() variant and its
 * private data.
 */
typedef struct {
    emit_character_t emit_character; /**< The output function. */
    void *private_data; /**< Its private data. */
} emitter_t;

/**
 * @typename `format_spec_t`
 *
 * @description
 * Conversion specification: flags, field width, precision and length.
 */
typedef struct {
    uint32_t length; /**< Number of 'l' modifiers; 'z' counts as one. */
    int32_t pad; /**< Field width. */
    int32_t precision; /**< Precision. */
    /** The '0' flag: pad with zeros instead of spaces. */
    bool pad_with_zero;
} format_spec_t;

/**
 * @typename `string_stream_t`
 *
 * @description
 * The output of vsnprintf(): the buffer, the number of characters emitted and
 * the size of the buffer.
 */
typedef struct {
    char* str; /**< The buffer. */
    size_t bytes_written; /**< The number of characters emitted. */
    size_t bytes_max; /**< The size of the buffer, with the NUL. */
} string_stream_t;

#endif /* LIBJET_STDIO_PRINTF_EMITTER_H */
