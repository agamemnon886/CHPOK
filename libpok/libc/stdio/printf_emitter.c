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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the types are moved to
 * libpok/libc/stdio/printf_emitter.h; struct emitter is the typedef emitter_t,
 * struct format_spec is the typedef format_spec_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * The formatting of libpok's printf() family: the conversions %%s, %%p, %%x,
 * %%d, %%i, %%u, %%f, %%c and %%, with a field width, the 0 flag, a precision
 * and the l, ll and z length modifiers, emitted character by character.
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

/**
 * @funcname `emit`
 *
 * @description
 * Emits one character with an emitter's output function.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `e` | `const emitter_t *` | [in] The emitter. |
 * | `c` | `char` | [in] The character. |
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
 * 1. Call the function `e->emit_character` with the arguments `c` and
 *    `e->private_data`.
 *
 * @calls
 * - `e->emit_character()` (indirect)
 */
static void emit(const emitter_t *e, char c)
{
    e->emit_character(c, e->private_data);
}

/**
 * @funcname `print_num`
 *
 * @description
 * Emits a number in a base, with its sign and the padding of the field width
 * (zeros after the sign, or spaces before it).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `e` | `const emitter_t *` | [in] The emitter. |
 * | `value` | `uint64_t` | [in] The magnitude. |
 * | `base` | `uint32_t` | [in] The base (10 or 16). |
 * | `pad` | `int32_t` | [in] The field width. |
 * | `neg` | `bool` | [in] Whether a minus sign comes first. |
 * | `pad_with_zero` | `bool` | [in] Whether to pad with zeros. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `digit_str` | `char[32]` | The digits, least significant first |
 * | `digits` | `static const char[17]` | The digit characters |
 * | `rest` | `uint64_t` | The part not converted yet |
 * | `size` | `int32_t` | The number of digits |
 * | `width` | `int32_t` | The width left for the digits |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `size` to `0`.
 * 2. Set `width` to `pad`.
 * 3. Set `rest` to `value`.
 * 4. Repeat the following while `rest` is not equal to `0U` (at least once):
 *    1. Set `digit_str[size]` to `digits[rest % base]`.
 *    2. Divide `rest` by `base`.
 *    3. Increment `size`.
 * 5. If `neg` is true:
 *    1. Decrement `width`.
 * 6. If `pad_with_zero` is true:
 *    1. If `neg` is true:
 *       1. Call `emit()` with the arguments `e` and `'-'`.
 *    2. While `size` is less than `width`:
 *       1. Call `emit()` with the arguments `e` and `'0'`.
 *       2. Decrement `width`.
 * 7. Otherwise:
 *    1. While `size` is less than `width`:
 *       1. Call `emit()` with the arguments `e` and `' '`.
 *       2. Decrement `width`.
 *    2. If `neg` is true:
 *       1. Call `emit()` with the arguments `e` and `'-'`.
 * 8. While `size` is greater than `0`:
 *    1. Decrement `size`.
 *    2. Call `emit()` with the arguments `e` and `digit_str[size]`.
 *
 * @calls
 * - `emit()`
 */
static void print_num(const emitter_t *e,
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

/**
 * @funcname `print_string`
 *
 * @description
 * Emits a string ("(null)" for NULL), right-aligned in the field width, cut to
 * the precision if one is given.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `e` | `const emitter_t *` | [in] The emitter. |
 * | `str` | `const char *` | [in] The string, or NULL. |
 * | `spec` | `const format_spec_t *` | [in] The conversion: width and precision. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the character |
 * | `len` | `size_t` | The number of characters emitted |
 * | `s` | `const char *` | The string, or "(null)" |
 * | `width` | `size_t` | The field width |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `s` to `str` if `str` is not NULL, otherwise `"(null)"`.
 * 2. Call `strlen()` with the argument `s`, and store the result in `len`.
 * 3. Set `width` to `(size_t) spec->pad` if `spec->pad` is greater than `0`,
 *    otherwise `0U`.
 * 4. If `spec->precision` is greater than `0` and `(size_t) spec->precision` is
 *    less than `len`:
 *    1. Set `len` to `(size_t) spec->precision`.
 * 5. While `width` is greater than `len`:
 *    1. Call `emit()` with the arguments `e` and `' '`.
 *    2. Decrement `width`.
 * 6. Set `i` to `0U`; then while `i` is less than `len`, repeat (after each
 *    pass: increment `i`):
 *    1. Call `emit()` with the arguments `e` and `s[i]`.
 *
 * @calls
 * - `strlen()`
 * - `emit()`
 */
static void print_string(const emitter_t *e, const char *str,
                         const format_spec_t *spec)
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

/**
 * @funcname `get_unsigned`
 *
 * @description
 * Reads the next variable argument as an unsigned number of the length given by
 * the specification.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `spec` | `const format_spec_t *` | [in] The conversion: its length. |
 * | `parg` | `va_list *` | [in,out] The variable arguments. |
 *
 * @return The argument.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | The argument |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `spec->length` is equal to `2U`:
 *    1. Set `value` to `va_arg(*parg, unsigned long long)`.
 * 2. Otherwise, if `spec->length` is equal to `1U`:
 *    1. Set `value` to `va_arg(*parg, unsigned long)`.
 * 3. Otherwise:
 *    1. Set `value` to `va_arg(*parg, unsigned int)`.
 * 4. Return `value`.
 *
 * @calls
 * None.
 */
static uint64_t get_unsigned(const format_spec_t *spec, va_list *parg)
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

/**
 * @funcname `get_signed`
 *
 * @description
 * Reads the next variable argument as a signed number of the length given by
 * the specification.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `spec` | `const format_spec_t *` | [in] The conversion: its length. |
 * | `parg` | `va_list *` | [in,out] The variable arguments. |
 *
 * @return The argument.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `int64_t` | The argument |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `spec->length` is equal to `2U`:
 *    1. Set `value` to `va_arg(*parg, long long)`.
 * 2. Otherwise, if `spec->length` is equal to `1U`:
 *    1. Set `value` to `va_arg(*parg, long)`.
 * 3. Otherwise:
 *    1. Set `value` to `va_arg(*parg, int)`.
 * 4. Return `value`.
 *
 * @calls
 * None.
 */
static int64_t get_signed(const format_spec_t *spec, va_list *parg)
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

/**
 * @funcname `print_float`
 *
 * @description
 * Emits a floating number that isn't negative: its integer part, a point and
 * the digits of the precision (truncated).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `e` | `const emitter_t *` | [in] The emitter. |
 * | `value` | `long double` | [in] The magnitude. |
 * | `pad` | `int32_t` | [in] The field width. |
 * | `precision` | `int32_t` | [in] The number of digits after the point. |
 * | `neg` | `bool` | [in] Whether a minus sign comes first. |
 * | `pad_with_zero` | `bool` | [in] Whether to pad with zeros. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `fractional` | `long double` | The fractional part, then scaled by the precision |
 * | `i` | `int32_t` | The number of digits scaled |
 * | `int_part` | `uint64_t` | The integer part |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `int_part` to `(uint64_t) value`.
 * 2. Set `fractional` to `value - (long double) int_part`.
 * 3. Call `print_num()` with the arguments `e`, `int_part`, `10U`,
 *    `pad - (precision + 1)`, `neg` and `pad_with_zero`.
 * 4. Call `emit()` with the arguments `e` and `'.'`.
 * 5. Set `i` to `0`; then while `i` is less than `precision`, repeat (after
 *    each pass: increment `i`):
 *    1. Multiply `fractional` by `10.0L`.
 * 6. Call `print_num()` with the arguments `e`, `(uint64_t) fractional`, `10U`,
 *    `precision`, `false` and `true`.
 *
 * @calls
 * - `print_num()`
 * - `emit()`
 */
static void print_float(const emitter_t *e,
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

/**
 * @funcname `print_conversion`
 *
 * @description
 * Emits one conversion with its specification and the next argument; an unknown
 * conversion is emitted as it is.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `e` | `const emitter_t *` | [in] The emitter. |
 * | `conversion` | `char` | [in] The conversion character. |
 * | `spec` | `const format_spec_t *` | [in] The width, precision, length and flags. |
 * | `parg` | `va_list *` | [in,out] The variable arguments. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `address` | `uintptr_t` | The argument of %%p |
 * | `magnitude` | `uint64_t` | Its magnitude |
 * | `precision` | `int32_t` | The precision of %%f (6 by default) |
 * | `value` | `int64_t` | The argument of %%d and %%i, or of %%f |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Depending on `conversion`:
 *    1. Case `'s'`:
 *       1. Call `print_string()` with the arguments `e`,
 *          `va_arg(*parg, const char *)` and `spec`.
 *       2. Leave the switch.
 *    2. Case `'p'`:
 *       1. Set `address` to `(uintptr_t) va_arg(*parg, void *)`.
 *       2. Call `emit()` with the arguments `e` and `'0'`.
 *       3. Call `emit()` with the arguments `e` and `'x'`.
 *       4. Call `print_num()` with the arguments `e`, `address`, `16U`,
 *          `spec->pad`, `false` and `spec->pad_with_zero`.
 *       5. Leave the switch.
 *    3. Case `'x'`:
 *       1. Call `print_num()` with the arguments `e`,
 *          `get_unsigned(spec, parg)`, `16U`, `spec->pad`, `false` and
 *          `spec->pad_with_zero`.
 *       2. Leave the switch.
 *    4. Case `'d'` or `'i'`:
 *       1. Call `get_signed()` with the arguments `spec` and `parg`, and store
 *          the result in `value`.
 *       2. Set `magnitude` to `(0U - (uint64_t) value)` if `value` is less than
 *          `0`, otherwise `(uint64_t) value`.
 *       3. Call `print_num()` with the arguments `e`, `magnitude`, `10U`,
 *          `spec->pad`, `value < 0` and `spec->pad_with_zero`.
 *       4. Leave the switch.
 *    5. Case `'u'`:
 *       1. Call `print_num()` with the arguments `e`,
 *          `get_unsigned(spec, parg)`, `10U`, `spec->pad`, `false` and
 *          `spec->pad_with_zero`.
 *       2. Leave the switch.
 *    6. Case `'f'`:
 *       1. Set `precision` to `6` if `spec->precision` is equal to `0`,
 *          otherwise `spec->precision`.
 *       2. If `spec->length` is equal to `2U`:
 *          1. Set `value` to `va_arg(*parg, long double)`.
 *       3. Otherwise:
 *          1. Set `value` to `va_arg(*parg, double)`.
 *       4. Call `print_float()` with the arguments `e`, `-value` if `value` is
 *          less than `0.0L`, otherwise `value`, `spec->pad`, `precision`,
 *          `value < 0.0L` and `spec->pad_with_zero`.
 *       5. Leave the switch.
 *    7. Case `'c'`:
 *       1. Call `emit()` with the arguments `e` and
 *          `(char) va_arg(*parg, int)`.
 *       2. Leave the switch.
 *    8. Case `'%%'`:
 *       1. Call `emit()` with the arguments `e` and `'%%'`.
 *       2. Leave the switch.
 *    9. Otherwise (default):
 *       1. Call `emit()` with the arguments `e` and `'%%'`.
 *       2. Call `emit()` with the arguments `e` and `conversion`.
 *       3. Leave the switch.
 *
 * @calls
 * - `print_string()`
 * - `emit()`
 * - `print_num()`
 * - `get_unsigned()`
 * - `get_signed()`
 * - `print_float()`
 */
static void print_conversion(const emitter_t *e, char conversion,
                             const format_spec_t *spec, va_list *parg)
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

/**
 * @funcname `handle_fmt`
 *
 * @description
 * Parses a conversion specification (without the leading '%%') from the format
 * and emits the characters it produces.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `e` | `const emitter_t *` | [in] The emitter. |
 * | `format` | `const char *` | [in] The format, after the %. |
 * | `parg` | `va_list *` | [in,out] The variable arguments. |
 *
 * @return The format after the conversion.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | The character of the format |
 * | `digit` | `int32_t` | The value of a digit |
 * | `f` | `const char *` | The format being read |
 * | `parsing` | `bool` | Whether the specification goes on |
 * | `spec` | `format_spec_t` | The conversion specification |
 * | `was_dot` | `bool` | Whether a precision follows |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `spec` to `{ 0U, 0, 0, false }`.
 * 2. Set `was_dot` to `false`.
 * 3. Set `parsing` to `true`.
 * 4. Set `f` to `format`.
 * 5. While `parsing` is true:
 *    1. Set `c` to the value at `f`.
 *    2. If `c` is greater than or equal to `'0'` and `c` is less than or equal
 *       to `'9'`:
 *       1. Set `digit` to `c - '0'`.
 *       2. If `was_dot` is true:
 *          1. Set `spec.precision` to `(spec.precision * 10) + digit`.
 *       3. Otherwise, if `digit` is equal to `0` and `spec.pad` is equal to
 *          `0`:
 *          1. Set `spec.pad_with_zero` to `true`.
 *       4. Otherwise:
 *          1. Set `spec.pad` to `(spec.pad * 10) + digit`.
 *       5. Increment `f`.
 *    3. Otherwise, if `c` is equal to `'l'`:
 *       1. Increment `spec.length`.
 *       2. Increment `f`.
 *    4. Otherwise, if `c` is equal to `'z'`:
 *       1. Set `spec.length` to `1U`.
 *       2. Increment `f`.
 *    5. Otherwise, if `c` is equal to `'.'`:
 *       1. Set `was_dot` to `true`.
 *       2. Increment `f`.
 *    6. Otherwise:
 *       1. Set `parsing` to `false`.
 * 6. If the value at `f` is not equal to `'\0'`:
 *    1. Call `print_conversion()` with the arguments `e`, the value at `f`, the
 *       address of `spec` and `parg`.
 *    2. Increment `f`.
 * 7. Return `f`.
 *
 * @calls
 * - `print_conversion()`
 */
//XXX for now precision is used only for floating points and strings
static const char * handle_fmt(const emitter_t *e,
    const char* format, va_list* parg)
{
    format_spec_t spec = { 0U, 0, 0, false };
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

/**
 * @funcname `printf_emitter`
 *
 * @description
 * For each character that a format and its arguments determine, calls an output
 * function. Where va_list is an array type (e.g. x86_64), the va_list argument
 * is really a pointer and can't be passed by reference, so a copy of it is
 * used.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `emit_character` | `emit_character_t` | [in] The output function. |
 * | `private_data` | `void *` | [in] The data of the output function. |
 * | `format` | `const char *` | [in] The format. |
 * | `arg` | `va_list` | [in] The variable arguments. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg1` | `va_list` | The copy of the variable arguments |
 * | `e` | `emitter_t` | The emitter |
 * | `f` | `const char *` | The format being read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `f` to `format`.
 * 2. Set `e.emit_character` to `emit_character`.
 * 3. Set `e.private_data` to `private_data`.
 * 4. Use the macro `va_copy()` with the arguments `arg1` and `arg`.
 * 5. While the value at `f` is not equal to `'\0'`:
 *    1. If the value at `f` is equal to `'%%'`:
 *       1. Call `handle_fmt()` with the arguments the address of `e`, `f + 1`
 *          and the address of `arg1`, and store the result in `f`.
 *    2. Otherwise:
 *       1. Call `emit()` with the arguments the address of `e` and the value at
 *          `f`.
 *       2. Increment `f`.
 * 6. Use the macro `va_end()` with the argument `arg1`.
 *
 * @calls
 * - `handle_fmt()`
 * - `emit()`
 */
void printf_emitter(emit_character_t emit_character, void *private_data,
    const char* format, va_list arg)
{
    emitter_t e;
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
