/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Formatted output shared by the kernel and the hypervisor
 * (common/format.c): jet_format() turns a printf format and its
 * arguments into characters, which it gives to a function of its caller.
 * printf() and snprintf() of the kernel and hv_printf() of the hypervisor
 * are built on it.
 *
 * The types were conversion_t and NUMBER_DIGITS of the hypervisor
 * (hypervisor/core/include/hv/hv.h); the kernel had its own formatter in
 * kernel/core/printf.c.
 */

#ifndef JET_COMMON_FORMAT_H
#define JET_COMMON_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * MISRA C:2025 Rule 17.1 deviation: jet_format() reads the variable
 * arguments of its caller with va_arg().
 */
#include <stdarg.h>

/**
 * @macroname `FORMAT_NUMBER_DIGITS`
 *
 * @description
 * The size of the buffer for the digits of a number (2^64 has 20 decimal
 * digits).
 */
#define FORMAT_NUMBER_DIGITS    24U

/**
 * @typename `format_emit_fn_t`
 *
 * @description
 * The function that takes the characters of a formatted output, one at a
 * time: it gets the character and the pointer that the caller of
 * jet_format() gave with it (its buffer, for instance), and returns
 * nothing.
 */
typedef void (*format_emit_fn_t)(char c, void *data);

/**
 * @typename `format_out_t`
 *
 * @description
 * Where the characters of a formatted output go: a function and its data.
 */
typedef struct {
    format_emit_fn_t emit; /**< The function that takes the characters. */
    void *data; /**< The data of the caller, passed to emit. */
} format_out_t;

/**
 * @typename `format_conv_t`
 *
 * @description
 * A conversion specification of jet_format(), without its conversion
 * character.
 */
typedef struct {
    bool zero_pad; /**< Whether the number is padded with zeros. */
    uint32_t width; /**< The minimum width of the field. */
    /** The precision (0 if none is given; used only for strings). */
    uint32_t precision;
    uint32_t longs; /**< Number of l modifiers (z counts as one). */
} format_conv_t;

/** \cond */
void jet_format(format_emit_fn_t emit, void *data, const char *format,
                va_list *args);
/** \endcond */

#endif /* JET_COMMON_FORMAT_H */
