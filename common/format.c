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
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: made from
 * hypervisor/core/printf.c, as the formatter that the hypervisor shares
 * with the kernel: the characters go to a function of the caller
 * (jet_format()) instead of the hypervisor console, and a precision limits
 * the characters of a string, as in the formatter of the kernel that this
 * one replaces.
 */

/**
 * @file
 * Formatted output shared by the kernel and the hypervisor: jet_format()
 * gives the characters of a printf format and its arguments to a function
 * of its caller. Conversions: %%d (%%i), %%u, %%x, %%p, %%s, %%c and %%; the
 * flag 0, a field width, a precision (for %%s: the maximum number of
 * characters), and the length modifiers l, ll and z. Anything else is
 * written as it is.
 */

#include <common/format.h>

/*
 * MISRA C:2025 Rule 17.1 deviation (this file): the variable arguments of
 * the caller are read with va_arg().
 */

/**
 * @funcname `put`
 *
 * @description
 * Gives a character to the output function of the caller of jet_format().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `const format_out_t *` | [in] The output. |
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
 * 1. Call the function `out->emit` with the arguments `c` and `out->data`.
 *
 * @calls
 * - `out->emit()` (indirect)
 */
static void put(const format_out_t *out, char c)
{
    out->emit(c, out->data);
}

/**
 * @funcname `put_repeated`
 *
 * @description
 * Adds several copies of a character to the output (padding).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `const format_out_t *` | [in] The output. |
 * | `c` | `char` | [in] The character. |
 * | `count` | `uint32_t` | [in] The number of copies. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The copy being added |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `count`, repeat (after each
 *    pass: increment `i`):
 *    1. Call `put()` with the arguments `out` and `c`.
 *
 * @calls
 * - `put()`
 */
static void put_repeated(const format_out_t *out, char c, uint32_t count)
{
    for (uint32_t i = 0U; i < count; i++) {
        put(out, c);
    }
}

/**
 * @funcname `put_number`
 *
 * @description
 * Adds a number in base 10 or 16, with its sign and the padding of the
 * field width (zeros after the sign, or spaces before it).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `const format_out_t *` | [in] The output. |
 * | `value` | `uint64_t` | [in] The magnitude of the number. |
 * | `base` | `uint32_t` | [in] 10 or 16. |
 * | `negative` | `bool` | [in] Whether a minus sign comes first. |
 * | `conv` | `const format_conv_t *` | [in] The conversion: zero padding and field width. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `digit_chars` | `static const char[17]` | The digit characters |
 * | `digits` | `char[FORMAT_NUMBER_DIGITS]` | The digits, least significant first |
 * | `count` | `uint32_t` | The number of digits |
 * | `rest` | `uint64_t` | The part of the value not yet converted |
 * | `length` | `uint32_t` | The length of the number with its sign |
 * | `padding` | `uint32_t` | The padding up to the field width |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `count` to `0U`.
 * 2. Set `rest` to `value`.
 * 3. Set `padding` to `0U`.
 * 4. Repeat the following while `rest` is not equal to `0U` (at least once):
 *    1. Set `digits[count]` to `digit_chars[rest % base]`.
 *    2. Divide `rest` by `base`.
 *    3. Increment `count`.
 * 5. Set `length` to `(count + 1U)` if `negative` is true, otherwise `count`.
 * 6. If `conv->width` is greater than `length`:
 *    1. Set `padding` to `conv->width - length`.
 * 7. If `conv->zero_pad` is true:
 *    1. If `negative` is true:
 *       1. Call `put()` with the arguments `out` and `'-'`.
 *    2. Call `put_repeated()` with the arguments `out`, `'0'` and `padding`.
 * 8. Otherwise:
 *    1. Call `put_repeated()` with the arguments `out`, `' '` and `padding`.
 *    2. If `negative` is true:
 *       1. Call `put()` with the arguments `out` and `'-'`.
 * 9. While `count` is greater than `0U`:
 *    1. Decrement `count`.
 *    2. Call `put()` with the arguments `out` and `digits[count]`.
 *
 * @calls
 * - `put()`
 * - `put_repeated()`
 */
static void put_number(const format_out_t *out, uint64_t value, uint32_t base,
                       bool negative, const format_conv_t *conv)
{
    static const char digit_chars[] = "0123456789abcdef";
    char digits[FORMAT_NUMBER_DIGITS];
    uint32_t count = 0U;
    uint64_t rest = value;
    uint32_t length;
    uint32_t padding = 0U;

    do {
        digits[count] = digit_chars[rest % base];
        rest /= base;
        count++;
    } while (rest != 0U);

    length = negative ? (count + 1U) : count;
    if (conv->width > length) {
        padding = conv->width - length;
    }
    if (conv->zero_pad) {
        if (negative) {
            put(out, '-');
        }
        put_repeated(out, '0', padding);
    } else {
        put_repeated(out, ' ', padding);
        if (negative) {
            put(out, '-');
        }
    }
    while (count > 0U) {
        count--;
        put(out, digits[count]);
    }
}

/**
 * @funcname `put_string`
 *
 * @description
 * Adds a string, right-aligned in the field width, and at most as many of
 * its characters as the precision, if one is given; a NULL pointer is
 * written as "(null)".
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `const format_out_t *` | [in] The output. |
 * | `s` | `const char *` | [in] The string, or NULL. |
 * | `conv` | `const format_conv_t *` | [in] The conversion: field width and precision. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `text` | `const char *` | The string, or "(null)" |
 * | `length` | `uint32_t` | The number of characters written |
 * | `i` | `uint32_t` | The index of the character |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `text` to `s` if `s` is not NULL, otherwise `"(null)"`.
 * 2. Set `length` to `0U`.
 * 3. While `text[length]` is not equal to `'\0'`:
 *    1. Increment `length`.
 * 4. If `conv->precision` is greater than `0U` and `conv->precision` is less
 *    than `length`:
 *    1. Set `length` to `conv->precision`.
 * 5. If `conv->width` is greater than `length`:
 *    1. Call `put_repeated()` with the arguments `out`, `' '` and
 *       `conv->width - length`.
 * 6. Set `i` to `0U`; then while `i` is less than `length`, repeat (after each
 *    pass: increment `i`):
 *    1. Call `put()` with the arguments `out` and `text[i]`.
 *
 * @calls
 * - `put_repeated()`
 * - `put()`
 */
static void put_string(const format_out_t *out, const char *s,
                       const format_conv_t *conv)
{
    const char *text = (s != NULL) ? s : "(null)";
    uint32_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    if ((conv->precision > 0U) && (conv->precision < length)) {
        length = conv->precision;
    }
    if (conv->width > length) {
        put_repeated(out, ' ', conv->width - length);
    }
    for (uint32_t i = 0U; i < length; i++) {
        put(out, text[i]);
    }
}

/**
 * @funcname `unsigned_argument`
 *
 * @description
 * Reads the next variable argument as an unsigned number of the
 * conversion's length (unsigned int, long or long long).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `conv` | `const format_conv_t *` | [in] The conversion: the number of l modifiers. |
 * | `args` | `va_list *` | [in,out] The variable arguments. |
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
 * 1. If `conv->longs` is greater than or equal to `2U`:
 *    1. Set `value` to `va_arg(*args, unsigned long long)`.
 * 2. Otherwise, if `conv->longs` is equal to `1U`:
 *    1. Set `value` to `va_arg(*args, unsigned long)`.
 * 3. Otherwise:
 *    1. Set `value` to `va_arg(*args, unsigned int)`.
 * 4. Return `value`.
 *
 * @calls
 * None.
 */
static uint64_t unsigned_argument(const format_conv_t *conv,
                                  va_list *args)
{
    uint64_t value;

    if (conv->longs >= 2U) {
        value = va_arg(*args, unsigned long long);
    } else if (conv->longs == 1U) {
        value = va_arg(*args, unsigned long);
    } else {
        value = va_arg(*args, unsigned int);
    }
    return value;
}

/**
 * @funcname `signed_argument`
 *
 * @description
 * Reads the next variable argument as a signed number of the conversion's
 * length (int, long or long long).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `conv` | `const format_conv_t *` | [in] The conversion: the number of l modifiers. |
 * | `args` | `va_list *` | [in,out] The variable arguments. |
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
 * 1. If `conv->longs` is greater than or equal to `2U`:
 *    1. Set `value` to `va_arg(*args, long long)`.
 * 2. Otherwise, if `conv->longs` is equal to `1U`:
 *    1. Set `value` to `va_arg(*args, long)`.
 * 3. Otherwise:
 *    1. Set `value` to `va_arg(*args, int)`.
 * 4. Return `value`.
 *
 * @calls
 * None.
 */
static int64_t signed_argument(const format_conv_t *conv, va_list *args)
{
    int64_t value;

    if (conv->longs >= 2U) {
        value = va_arg(*args, long long);
    } else if (conv->longs == 1U) {
        value = va_arg(*args, long);
    } else {
        value = va_arg(*args, int);
    }
    return value;
}

/**
 * @funcname `put_conversion`
 *
 * @description
 * Adds the output of one conversion, with the next argument.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `const format_out_t *` | [in] The output. |
 * | `c` | `char` | [in] The conversion character. |
 * | `conv` | `const format_conv_t *` | [in] The flags, field width, precision and length of the conversion. |
 * | `args` | `va_list *` | [in,out] The variable arguments. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `int64_t` | The argument of %%d and %%i |
 * | `magnitude` | `uint64_t` | Its magnitude (that of the most negative value fits too) |
 * | `address` | `uintptr_t` | The argument of %%p |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Depending on `c`:
 *    1. Case `'d'` or `'i'`:
 *       1. Call `signed_argument()` with the arguments `conv` and `args`, and
 *          store the result in `value`.
 *       2. Set `magnitude` to `(0U - (uint64_t) value)` if `value` is less than
 *          `0`, otherwise `(uint64_t) value`.
 *       3. Call `put_number()` with the arguments `out`, `magnitude`, `10U`,
 *          `value < 0` and `conv`.
 *       4. Leave the switch.
 *    2. Case `'u'`:
 *       1. Call `put_number()` with the arguments `out`,
 *          `unsigned_argument(conv, args)`, `10U`, `false` and `conv`.
 *       2. Leave the switch.
 *    3. Case `'x'`:
 *       1. Call `put_number()` with the arguments `out`,
 *          `unsigned_argument(conv, args)`, `16U`, `false` and `conv`.
 *       2. Leave the switch.
 *    4. Case `'p'`:
 *       1. Set `address` to `(uintptr_t) va_arg(*args, void *)`.
 *       2. Call `put()` with the arguments `out` and `'0'`.
 *       3. Call `put()` with the arguments `out` and `'x'`.
 *       4. Call `put_number()` with the arguments `out`, `address`, `16U`,
 *          `false` and `conv`.
 *       5. Leave the switch.
 *    5. Case `'s'`:
 *       1. Call `put_string()` with the arguments `out`,
 *          `va_arg(*args, const char *)` and `conv`.
 *       2. Leave the switch.
 *    6. Case `'c'`:
 *       1. Call `put()` with the arguments `out` and
 *          `(char) va_arg(*args, int)`.
 *       2. Leave the switch.
 *    7. Case `'%%'`:
 *       1. Call `put()` with the arguments `out` and `'%%'`.
 *       2. Leave the switch.
 *    8. Otherwise (default):
 *       1. Call `put()` with the arguments `out` and `'%%'`.
 *       2. Call `put()` with the arguments `out` and `c`.
 *       3. Leave the switch.
 *
 * @calls
 * - `signed_argument()`
 * - `put_number()`
 * - `unsigned_argument()`
 * - `put()`
 * - `put_string()`
 */
static void put_conversion(const format_out_t *out, char c,
                           const format_conv_t *conv, va_list *args)
{
    switch (c) {
        case 'd':
        case 'i':
            {
                int64_t value = signed_argument(conv, args);
                // The magnitude of the most negative value fits too.
                uint64_t magnitude = (value < 0) ? (0U - (uint64_t) value)
                                                 : (uint64_t) value;

                put_number(out, magnitude, 10U, value < 0, conv);
            }
            break;
        case 'u':
            put_number(out, unsigned_argument(conv, args), 10U, false,
                       conv);
            break;
        case 'x':
            put_number(out, unsigned_argument(conv, args), 16U, false,
                       conv);
            break;
        case 'p':
            {
                /*
                 * MISRA C:2025 Rule 11.6 deviation: %p prints the
                 * address held by a pointer.
                 */
                uintptr_t address = (uintptr_t) va_arg(*args, void *);

                put(out, '0');
                put(out, 'x');
                put_number(out, address, 16U, false, conv);
            }
            break;
        case 's':
            put_string(out, va_arg(*args, const char *), conv);
            break;
        case 'c':
            put(out, (char) va_arg(*args, int));
            break;
        case '%':
            put(out, '%');
            break;
        default:
            put(out, '%');
            put(out, c);
            break;
    }
}

/**
 * @funcname `is_digit`
 *
 * @description
 * Tells whether a character is a decimal digit.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 *
 * @return true for '0' to '9'.
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
 * 1. Return `(c >= '0') && (c <= '9')`.
 *
 * @calls
 * None.
 */
static bool is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

/**
 * @funcname `jet_format`
 *
 * @description
 * Formatted output: gives the characters of a format, with its conversions
 * replaced by the arguments, to a function of the caller, one at a time
 * (the conversions of this file). printf() and snprintf() of the kernel and
 * hv_printf() of the hypervisor call it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `emit` | `format_emit_fn_t` | [in] The function that takes the characters. |
 * | `data` | `void *` | [in] The data of the caller, passed to emit. |
 * | `format` | `const char *` | [in] The format. |
 * | `args` | `va_list *` | [in,out] The arguments of the conversions. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `format_out_t` | The output: emit and data |
 * | `i` | `size_t` | The index in the format |
 * | `conv` | `format_conv_t` | The conversion being read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`.
 * 2. Set `out.emit` to `emit`.
 * 3. Set `out.data` to `data`.
 * 4. While `format[i]` is not equal to `'\0'`:
 *    1. If `format[i]` is not equal to `'%%'`:
 *       1. Call `put()` with the arguments the address of `out` and
 *          `format[i]`.
 *       2. Increment `i`.
 *    2. Otherwise:
 *       1. Set `conv` to `{ false, 0U, 0U, 0U }`.
 *       2. Increment `i`.
 *       3. If `format[i]` is equal to `'0'`:
 *          1. Set `conv.zero_pad` to `true`.
 *          2. Increment `i`.
 *       4. While `is_digit(format[i])` is true:
 *          1. Set `conv.width` to
 *             `(conv.width * 10U) + ((uint32_t) format[i] - (uint32_t) '0')`.
 *          2. Increment `i`.
 *       5. If `format[i]` is equal to `'.'`:
 *          1. Increment `i`.
 *          2. While `is_digit(format[i])` is true:
 *             1. Set `conv.precision` to `(conv.precision * 10U) +
 *                ((uint32_t) format[i] - (uint32_t) '0')`.
 *             2. Increment `i`.
 *       6. While `format[i]` is equal to `'l'` or `format[i]` is equal to
 *          `'z'`:
 *          1. Set `conv.longs` to `1U` if `format[i]` is equal to `'z'`,
 *             otherwise `(conv.longs + 1U)`.
 *          2. Increment `i`.
 *       7. If `format[i]` is not equal to `'\0'`:
 *          1. Call `put_conversion()` with the arguments the address of `out`,
 *             `format[i]`, the address of `conv` and `args`.
 *          2. Increment `i`.
 *
 * @calls
 * - `put()`
 * - `is_digit()`
 * - `put_conversion()`
 */
void jet_format(format_emit_fn_t emit, void *data, const char *format,
                va_list *args)
{
    format_out_t out;
    size_t i = 0U;

    out.emit = emit;
    out.data = data;
    while (format[i] != '\0') {
        // Ordinary characters are copied; % starts a conversion.
        if (format[i] != '%') {
            put(&out, format[i]);
            i++;
        } else {
            format_conv_t conv = { false, 0U, 0U, 0U };

            i++;
            if (format[i] == '0') {
                conv.zero_pad = true;
                i++;
            }
            while (is_digit(format[i])) {
                conv.width = (conv.width * 10U)
                             + ((uint32_t) format[i] - (uint32_t) '0');
                i++;
            }
            if (format[i] == '.') {
                i++;
                while (is_digit(format[i])) {
                    conv.precision = (conv.precision * 10U)
                                     + ((uint32_t) format[i] - (uint32_t) '0');
                    i++;
                }
            }
            while ((format[i] == 'l') || (format[i] == 'z')) {
                conv.longs = (format[i] == 'z') ? 1U : (conv.longs + 1U);
                i++;
            }
            if (format[i] != '\0') {
                put_conversion(&out, format[i], &conv, args);
                i++;
            }
        }
    }
}
