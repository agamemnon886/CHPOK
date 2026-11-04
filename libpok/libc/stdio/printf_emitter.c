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
 * Modified by Kirill Brazhnikov, 2026-10-31: comment update for 64-bit
 * targets.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: rewritten with
 * fixed-width and Boolean types, a parsed conversion specification and one
 * break per switch clause (no fall-through, no GNU case ranges); characters
 * are emitted as char. %p and %z now read a pointer and a size_t (they read
 * an unsigned int, which truncated 64-bit values), and the most negative
 * %d value is printed without overflow.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: digits is
 * defined in the function that uses it; deviations are documented where a
 * guideline can't be followed. Long lines are wrapped at 80 columns.
 */

/*
 * MISRA C:2025 Rule 17.1 deviation (this file): the arguments of the printf()
 * family are read with va_arg().
 */
#include "printf_emitter.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): the format string is walked
 * with a pointer into it.
 */


/* Where the characters go. */
struct emitter {
    emit_character_t emit_character;
    void *private_data;
};

/* Conversion specification: flags, field width, precision and length. */
struct format_spec {
    uint32_t length; // Number of 'l' modifiers; 'z' counts as one.
    int32_t pad; // Field width.
    int32_t precision;
    bool pad_with_zero; // The '0' flag: pad with zeros instead of spaces.
};

static void emit(const struct emitter *e, char c)
{
    e->emit_character(c, e->private_data);
}

/* Emit characters, produced by a number with given modificators.*/
static void print_num(const struct emitter *e,
                      uint64_t value,
                      uint32_t base,
                      int32_t pad,
                      bool neg,
                      bool pad_with_zero)
{
    static const char digits[] = "0123456789abcdef";

    char digit_str[32]; //64-bit number in octal base is 22 digits long
    int32_t size = 0;
    int32_t width = pad;
    uint64_t rest = value;

    do {
        digit_str[size] = digits[rest % base];
        rest /= base;
        size++;
    } while (rest != 0U);

    if (neg) {
        width--;
    }

    if (pad_with_zero) {
        if (neg) {
            emit(e, '-');
        }
        while (size < width) {
            emit(e, '0');
            width--;
        }
    } else {
        while (size < width) {
            emit(e, ' ');
            width--;
        }
        if (neg) {
            emit(e, '-');
        }
    }

    while (size > 0) {
        size--;
        emit(e, digit_str[size]);
    }
}

/* Emit a string, cut to the precision if one is given. */
static void print_string(const struct emitter *e, const char *str,
                         const struct format_spec *spec)
{
    const char *s = (str != NULL) ? str : "(null)";
    size_t len = strlen(s);
    size_t width = (spec->pad > 0) ? (size_t) spec->pad : 0U;

    if ((spec->precision > 0) && ((size_t) spec->precision < len)) {
        len = (size_t) spec->precision;
    }

    while (width > len) {
        emit(e, ' ');
        width--;
    }
    for (size_t i = 0U; i < len; i++) {
        emit(e, s[i]);
    }
}

/* Unsigned argument of the length given by the specification. */
static uint64_t get_unsigned(const struct format_spec *spec, va_list *parg)
{
    uint64_t value;

    if (spec->length == 2U) {
        value = va_arg(*parg, unsigned long long);
    } else if (spec->length == 1U) {
        value = va_arg(*parg, unsigned long);
    } else {
        value = va_arg(*parg, unsigned int);
    }
    return value;
}

/* Signed argument of the length given by the specification. */
static int64_t get_signed(const struct format_spec *spec, va_list *parg)
{
    int64_t value;

    if (spec->length == 2U) {
        value = va_arg(*parg, long long);
    } else if (spec->length == 1U) {
        value = va_arg(*parg, long);
    } else {
        value = va_arg(*parg, int);
    }
    return value;
}

/* Emit characters, produced by a floating number with given modificators.*/
static void print_float(const struct emitter *e,
                        long double value,
                        int32_t pad,
                        int32_t precision,
                        bool neg,
                        bool pad_with_zero)
{
    uint64_t int_part = (uint64_t) value;
    long double fractional = value - (long double) int_part;

    print_num(e, int_part, 10U, pad - (precision + 1), neg, pad_with_zero);

    emit(e, '.');

    for (int32_t i = 0; i < precision; i++) {
        fractional *= 10.0L;
    }
    print_num(e, (uint64_t) fractional, 10U, precision, false, true);
}

/* Emit characters, produced by one conversion with its specification. */
static void print_conversion(const struct emitter *e, char conversion,
                             const struct format_spec *spec, va_list *parg)
{
    switch (conversion) {
        case 's':
            print_string(e, va_arg(*parg, const char *), spec);
            break;
        case 'p':
            {
                /*
                 * MISRA C:2025 Rule 11.6 deviation: %p prints the address
                 * held by a pointer.
                 */
                uintptr_t address = (uintptr_t) va_arg(*parg, void *);

                emit(e, '0');
                emit(e, 'x');
                print_num(e, address, 16U, spec->pad, false,
                          spec->pad_with_zero);
            }
            break;
        case 'x':
            print_num(e, get_unsigned(spec, parg), 16U, spec->pad, false,
                      spec->pad_with_zero);
            break;
        case 'd':
        case 'i':
            {
                int64_t value = get_signed(spec, parg);
                uint64_t magnitude = (value < 0) ? (0U - (uint64_t) value)
                    : (uint64_t) value;

                print_num(e, magnitude, 10U, spec->pad, value < 0,
                          spec->pad_with_zero);
            }
            break;
        case 'u':
            print_num(e, get_unsigned(spec, parg), 10U, spec->pad, false,
                      spec->pad_with_zero);
            break;
        case 'f':
            {
                long double value;
                int32_t precision = (spec->precision == 0) ? 6
                    : spec->precision;

                if (spec->length == 2U) {
                    value = va_arg(*parg, long double);
                } else {
                    value = va_arg(*parg, double);
                }
                print_float(e, (value < 0.0L) ? -value : value, spec->pad,
                    precision, value < 0.0L, spec->pad_with_zero);
            }
            break;
        case 'c':
            //TODO implement l!=0 case
            emit(e, (char) va_arg(*parg, int));
            break;
        case '%':
            emit(e, '%');
            break;
        default:
            emit(e, '%');
            emit(e, conversion);
            break;
    }
}

/*
 * Parse format specifier (without leading '%') from format and emit
 * characters it produces.
 *
 * Return pointer to the character followed format specifier.
 */
//XXX for now precision is used only for floating points and strings
static const char * handle_fmt(const struct emitter *e,
    const char* format, va_list* parg)
{
    struct format_spec spec = { 0U, 0, 0, false };
    bool was_dot = false; // There was '.' in the specification.
    bool parsing = true;
    const char *f = format;

    while (parsing) {
        char c = *f;

        if ((c >= '0') && (c <= '9')) {
            int32_t digit = c - '0';

            if (was_dot) {
                spec.precision = (spec.precision * 10) + digit;
            } else if ((digit == 0) && (spec.pad == 0)) {
                spec.pad_with_zero = true;
            } else {
                spec.pad = (spec.pad * 10) + digit;
            }
            f++;
        } else if (c == 'l') {
            spec.length++;
            f++;
        } else if (c == 'z') {
            // size_t is unsigned long on the LP64 targets.
            spec.length = 1U;
            f++;
        } else if (c == '.') {
            was_dot = true;
            f++;
        } else {
            parsing = false;
        }
    }

    if (*f != '\0') {
        print_conversion(e, *f, &spec, parg);
        f++;
    }
    return f;
}

void printf_emitter(emit_character_t emit_character, void *private_data,
    const char* format, va_list arg)
{
    struct emitter e;
    const char *f = format;

    e.emit_character = emit_character;
    e.private_data = private_data;

    /*
     * Where va_list is an array type (e.g. x86_64), our 'va_list' argument
     * is really a pointer, so we cannot pass a reference to it.
     *
     * Because of that we create our copy of the arg.
     *
     * After va_copy() do not forget to call va_end() at the end.
     */
    va_list arg1;

    va_copy(arg1, arg);

    while (*f != '\0') {
        if (*f == '%') {
            f = handle_fmt(&e, f + 1, &arg1);
        } else {
            emit(&e, *f);
            f++;
        }
    }

    va_end(arg1);
}
