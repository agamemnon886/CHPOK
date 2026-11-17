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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: requirements with more than one
 * "shall" are split into requirements of one "shall" each.
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
 * - \hlrid{HLR_FORMAT_PUT_01}: `put()` shall give one character of the
 *   formatted output to the output function of the caller.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_FORMAT_PUT_01}: `put()` shall call the function `emit` of `out`
 *   once with `c` and `data` of `out`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_01}
 *
 * @design
 * 1. Call the function `out->emit` with the arguments `c` and `out->data`.
 *    \llrref{LLR_FORMAT_PUT_01}
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
 * - \hlrid{HLR_FORMAT_PUT_REPEATED_01}: `put_repeated()` shall give a number of
 *   copies of a character to the output, for padding.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_FORMAT_PUT_REPEATED_01}: `put_repeated()` shall call `put()`
 *   `count` times with `out` and `c`, and not at all if `count` is 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_REPEATED_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `count`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_FORMAT_PUT_REPEATED_01}
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
 * - \hlrid{HLR_FORMAT_PUT_NUMBER_01}: `put_number()` shall give a number to the
 *   output in base 10 or 16, with its sign and padded to the field width: with
 *   zeros after the sign, or with spaces before it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_FORMAT_PUT_NUMBER_01}: `put_number()` shall end with the calls
 *   of `put()` with `out` and each digit of `value` in the base `base` (a
 *   character of `"0123456789abcdef"`), from the highest digit on, without
 *   leading zeros, and with the one digit `'0'` for the value 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_NUMBER_01}
 * - \llrid{LLR_FORMAT_PUT_NUMBER_02}: If `width` of `conv` is greater than the
 *   length of the number (the number of its digits, plus 1 if and only if
 *   `negative` is true), `put_number()` shall give `width` minus that length to
 *   `put_repeated()` as the count.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_NUMBER_01}
 * - \llrid{LLR_FORMAT_PUT_NUMBER_03}: If `width` of `conv` is not greater than
 *   the length of the number, `put_number()` shall give 0 to `put_repeated()`
 *   as the count.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_NUMBER_01}
 * - \llrid{LLR_FORMAT_PUT_NUMBER_04}: If `zero_pad` of `conv` is true,
 *   `put_number()` shall call `put_repeated()` once with `out`, `'0'` and the
 *   count, after the sign and before the digits.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_NUMBER_01}
 * - \llrid{LLR_FORMAT_PUT_NUMBER_05}: If `zero_pad` of `conv` is false,
 *   `put_number()` shall call `put_repeated()` once with `out`, `' '` and the
 *   count, before the sign and the digits.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_NUMBER_01}
 * - \llrid{LLR_FORMAT_PUT_NUMBER_06}: If and only if `negative` is true,
 *   `put_number()` shall call `put()` once with `out` and `'-'` as the sign.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_NUMBER_01}
 *
 * @design
 * 1. Set `count` to `0U`. \llrref{LLR_FORMAT_PUT_NUMBER_01}
 * 2. Set `rest` to `value`. \llrref{LLR_FORMAT_PUT_NUMBER_01}
 * 3. Set `padding` to `0U`. \llrref{LLR_FORMAT_PUT_NUMBER_03}
 * 4. Repeat the following while `rest` is not equal to `0U` (at least once):
 *    \llrref{LLR_FORMAT_PUT_NUMBER_01}
 *    1. Set `digits[count]` to `digit_chars[rest % base]`.
 *    2. Divide `rest` by `base`.
 *    3. Increment `count`.
 * 5. Set `length` to `(count + 1U)` if `negative` is true, otherwise `count`.
 *    \llrref{LLR_FORMAT_PUT_NUMBER_02}, \llrref{LLR_FORMAT_PUT_NUMBER_03}
 * 6. If `conv->width` is greater than `length`:
 *    \llrref{LLR_FORMAT_PUT_NUMBER_02}, \llrref{LLR_FORMAT_PUT_NUMBER_03}
 *    1. Set `padding` to `conv->width - length`.
 *       \llrref{LLR_FORMAT_PUT_NUMBER_02}
 * 7. If `conv->zero_pad` is true: \llrref{LLR_FORMAT_PUT_NUMBER_04},
 *    \llrref{LLR_FORMAT_PUT_NUMBER_06}
 *    1. If `negative` is true: \llrref{LLR_FORMAT_PUT_NUMBER_06}
 *       1. Call `put()` with the arguments `out` and `'-'`.
 *    2. Call `put_repeated()` with the arguments `out`, `'0'` and `padding`.
 *       \llrref{LLR_FORMAT_PUT_NUMBER_04}
 * 8. Otherwise: \llrref{LLR_FORMAT_PUT_NUMBER_05},
 *    \llrref{LLR_FORMAT_PUT_NUMBER_06}
 *    1. Call `put_repeated()` with the arguments `out`, `' '` and `padding`.
 *       \llrref{LLR_FORMAT_PUT_NUMBER_05}
 *    2. If `negative` is true: \llrref{LLR_FORMAT_PUT_NUMBER_06}
 *       1. Call `put()` with the arguments `out` and `'-'`.
 * 9. While `count` is greater than `0U`: \llrref{LLR_FORMAT_PUT_NUMBER_01}
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
 * - \hlrid{HLR_FORMAT_PUT_STRING_01}: `put_string()` shall give a string to the
 *   output, right-aligned in the field width, at most as many characters as a
 *   precision that is given, and `"(null)"` for NULL.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_FORMAT_PUT_STRING_01}: `put_string()` shall take as the text the
 *   string at `s`, and `"(null)"` if and only if `s` is NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_STRING_01}
 * - \llrid{LLR_FORMAT_PUT_STRING_02}: If `precision` of `conv` is greater than
 *   0 and less than the number of characters of the text before its `'\0'`,
 *   `put_string()` shall call `put()` with `out` and each of the first
 *   `precision` characters of the text, in their order.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_STRING_01}
 * - \llrid{LLR_FORMAT_PUT_STRING_03}: If `precision` of `conv` is 0 or is not
 *   less than the number of characters of the text before its `'\0'`,
 *   `put_string()` shall call `put()` with `out` and each character of the text
 *   before its `'\0'`, in their order.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_STRING_01}
 * - \llrid{LLR_FORMAT_PUT_STRING_04}: If and only if `width` of `conv` is
 *   greater than the number of characters given to `put()`, `put_string()`
 *   shall first call `put_repeated()` once with `out`, `' '` and `width` minus
 *   that number.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_STRING_01}
 *
 * @design
 * 1. Set `text` to `s` if `s` is not NULL, otherwise `"(null)"`.
 *    \llrref{LLR_FORMAT_PUT_STRING_01}
 * 2. Set `length` to `0U`. \llrref{LLR_FORMAT_PUT_STRING_02},
 *    \llrref{LLR_FORMAT_PUT_STRING_03}
 * 3. While `text[length]` is not equal to `'\0'`:
 *    \llrref{LLR_FORMAT_PUT_STRING_02}, \llrref{LLR_FORMAT_PUT_STRING_03}
 *    1. Increment `length`.
 * 4. If `conv->precision` is greater than `0U` and `conv->precision` is less
 *    than `length`: \llrref{LLR_FORMAT_PUT_STRING_02},
 *    \llrref{LLR_FORMAT_PUT_STRING_03}
 *    1. Set `length` to `conv->precision`. \llrref{LLR_FORMAT_PUT_STRING_02}
 * 5. If `conv->width` is greater than `length`:
 *    \llrref{LLR_FORMAT_PUT_STRING_04}
 *    1. Call `put_repeated()` with the arguments `out`, `' '` and
 *       `conv->width - length`.
 * 6. Set `i` to `0U`; then while `i` is less than `length`, repeat (after each
 *    pass: increment `i`): \llrref{LLR_FORMAT_PUT_STRING_02},
 *    \llrref{LLR_FORMAT_PUT_STRING_03}
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
 * - \hlrid{HLR_FORMAT_UNSIGNED_ARGUMENT_01}: `unsigned_argument()` shall take
 *   the next argument as an unsigned number of the length of the conversion.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_FORMAT_UNSIGNED_ARGUMENT_01}: If `longs` of `conv` is 2 or more,
 *   `unsigned_argument()` shall return the next argument of `args`, taken as an
 *   `unsigned long long`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_UNSIGNED_ARGUMENT_01}
 * - \llrid{LLR_FORMAT_UNSIGNED_ARGUMENT_02}: If `longs` of `conv` is 1,
 *   `unsigned_argument()` shall return the next argument of `args`, taken as an
 *   `unsigned long`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_UNSIGNED_ARGUMENT_01}
 * - \llrid{LLR_FORMAT_UNSIGNED_ARGUMENT_03}: If `longs` of `conv` is 0,
 *   `unsigned_argument()` shall return the next argument of `args`, taken as an
 *   `unsigned int`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_UNSIGNED_ARGUMENT_01}
 *
 * @design
 * 1. If `conv->longs` is greater than or equal to `2U`:
 *    \llrref{LLR_FORMAT_UNSIGNED_ARGUMENT_01}
 *    1. Set `value` to `va_arg(*args, unsigned long long)`.
 * 2. Otherwise, if `conv->longs` is equal to `1U`:
 *    \llrref{LLR_FORMAT_UNSIGNED_ARGUMENT_02}
 *    1. Set `value` to `va_arg(*args, unsigned long)`.
 * 3. Otherwise: \llrref{LLR_FORMAT_UNSIGNED_ARGUMENT_03}
 *    1. Set `value` to `va_arg(*args, unsigned int)`.
 * 4. Return `value`. \llrref{LLR_FORMAT_UNSIGNED_ARGUMENT_01},
 *    \llrref{LLR_FORMAT_UNSIGNED_ARGUMENT_02},
 *    \llrref{LLR_FORMAT_UNSIGNED_ARGUMENT_03}
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
 * - \hlrid{HLR_FORMAT_SIGNED_ARGUMENT_01}: `signed_argument()` shall take the
 *   next argument as a signed number of the length of the conversion.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_FORMAT_SIGNED_ARGUMENT_01}: If `longs` of `conv` is 2 or more,
 *   `signed_argument()` shall return the next argument of `args`, taken as a
 *   `long long`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_SIGNED_ARGUMENT_01}
 * - \llrid{LLR_FORMAT_SIGNED_ARGUMENT_02}: If `longs` of `conv` is 1,
 *   `signed_argument()` shall return the next argument of `args`, taken as a
 *   `long`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_SIGNED_ARGUMENT_01}
 * - \llrid{LLR_FORMAT_SIGNED_ARGUMENT_03}: If `longs` of `conv` is 0,
 *   `signed_argument()` shall return the next argument of `args`, taken as an
 *   `int`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_SIGNED_ARGUMENT_01}
 *
 * @design
 * 1. If `conv->longs` is greater than or equal to `2U`:
 *    \llrref{LLR_FORMAT_SIGNED_ARGUMENT_01}
 *    1. Set `value` to `va_arg(*args, long long)`.
 * 2. Otherwise, if `conv->longs` is equal to `1U`:
 *    \llrref{LLR_FORMAT_SIGNED_ARGUMENT_02}
 *    1. Set `value` to `va_arg(*args, long)`.
 * 3. Otherwise: \llrref{LLR_FORMAT_SIGNED_ARGUMENT_03}
 *    1. Set `value` to `va_arg(*args, int)`.
 * 4. Return `value`. \llrref{LLR_FORMAT_SIGNED_ARGUMENT_01},
 *    \llrref{LLR_FORMAT_SIGNED_ARGUMENT_02},
 *    \llrref{LLR_FORMAT_SIGNED_ARGUMENT_03}
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
 * - \hlrid{HLR_FORMAT_PUT_CONVERSION_01}: `put_conversion()` shall give the
 *   output of one conversion with the next argument: signed and unsigned
 *   decimal, hexadecimal, pointer, string, character and the percent sign.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 * - \hlrid{HLR_FORMAT_PUT_CONVERSION_02}: `put_conversion()` shall give an
 *   unknown conversion as it is written.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_FORMAT_PUT_CONVERSION_01}: If `c` is `'d'` or `'i'` and the
 *   result of `signed_argument()` for `conv` and `args` is less than 0,
 *   `put_conversion()` shall call `put_number()` once with `out`, 0 minus that
 *   result as an unsigned number, the base 10, true and `conv`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_CONVERSION_01}
 * - \llrid{LLR_FORMAT_PUT_CONVERSION_02}: If `c` is `'d'` or `'i'` and the
 *   result of `signed_argument()` for `conv` and `args` is not less than 0,
 *   `put_conversion()` shall call `put_number()` once with `out`, that result,
 *   the base 10, false and `conv`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_CONVERSION_01}
 * - \llrid{LLR_FORMAT_PUT_CONVERSION_03}: If `c` is `'u'`, `put_conversion()`
 *   shall call `put_number()` once with `out`, the result of
 *   `unsigned_argument()` for `conv` and `args`, the base 10, false and `conv`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_CONVERSION_01}
 * - \llrid{LLR_FORMAT_PUT_CONVERSION_04}: If `c` is `'x'`, `put_conversion()`
 *   shall call `put_number()` once with `out`, the result of
 *   `unsigned_argument()` for `conv` and `args`, the base 16, false and `conv`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_CONVERSION_01}
 * - \llrid{LLR_FORMAT_PUT_CONVERSION_05}: If `c` is `'p'`, `put_conversion()`
 *   shall call `put()` with `out` and `'0'`, then `put()` with `out` and `'x'`,
 *   and then `put_number()` with `out`, the next argument of `args` (taken as a
 *   `void *`) as a number, the base 16, false and `conv`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_CONVERSION_01}
 * - \llrid{LLR_FORMAT_PUT_CONVERSION_06}: If `c` is `'s'`, `put_conversion()`
 *   shall call `put_string()` once with `out`, the next argument of `args`
 *   (taken as a `const char *`) and `conv`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_CONVERSION_01}
 * - \llrid{LLR_FORMAT_PUT_CONVERSION_07}: If `c` is `'c'`, `put_conversion()`
 *   shall call `put()` once with `out` and the next argument of `args` (taken
 *   as an `int`) converted to `char`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_CONVERSION_01}
 * - \llrid{LLR_FORMAT_PUT_CONVERSION_08}: If `c` is `'%%'`, `put_conversion()`
 *   shall call `put()` once with `out` and `'%%'`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_CONVERSION_01}
 * - \llrid{LLR_FORMAT_PUT_CONVERSION_09}: If `c` is none of `'d'`, `'i'`,
 *   `'u'`, `'x'`, `'p'`, `'s'`, `'c'` and `'%%'`, `put_conversion()` shall call
 *   `put()` with `out` and `'%%'`, and then `put()` with `out` and `c`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_PUT_CONVERSION_02}
 *
 * @design
 * 1. Depending on `c`: \llrref{LLR_FORMAT_PUT_CONVERSION_01},
 *    \llrref{LLR_FORMAT_PUT_CONVERSION_02},
 *    \llrref{LLR_FORMAT_PUT_CONVERSION_03},
 *    \llrref{LLR_FORMAT_PUT_CONVERSION_04},
 *    \llrref{LLR_FORMAT_PUT_CONVERSION_05},
 *    \llrref{LLR_FORMAT_PUT_CONVERSION_06},
 *    \llrref{LLR_FORMAT_PUT_CONVERSION_07},
 *    \llrref{LLR_FORMAT_PUT_CONVERSION_08},
 *    \llrref{LLR_FORMAT_PUT_CONVERSION_09}
 *    1. Case `'d'` or `'i'`: \llrref{LLR_FORMAT_PUT_CONVERSION_01},
 *       \llrref{LLR_FORMAT_PUT_CONVERSION_02}
 *       1. Call `signed_argument()` with the arguments `conv` and `args`, and
 *          store the result in `value`.
 *       2. Set `magnitude` to `(0U - (uint64_t) value)` if `value` is less than
 *          `0`, otherwise `(uint64_t) value`.
 *       3. Call `put_number()` with the arguments `out`, `magnitude`, `10U`,
 *          `value < 0` and `conv`.
 *       4. Leave the switch.
 *    2. Case `'u'`: \llrref{LLR_FORMAT_PUT_CONVERSION_03}
 *       1. Call `put_number()` with the arguments `out`,
 *          `unsigned_argument(conv, args)`, `10U`, `false` and `conv`.
 *       2. Leave the switch.
 *    3. Case `'x'`: \llrref{LLR_FORMAT_PUT_CONVERSION_04}
 *       1. Call `put_number()` with the arguments `out`,
 *          `unsigned_argument(conv, args)`, `16U`, `false` and `conv`.
 *       2. Leave the switch.
 *    4. Case `'p'`: \llrref{LLR_FORMAT_PUT_CONVERSION_05}
 *       1. Set `address` to `(uintptr_t) va_arg(*args, void *)`.
 *       2. Call `put()` with the arguments `out` and `'0'`.
 *       3. Call `put()` with the arguments `out` and `'x'`.
 *       4. Call `put_number()` with the arguments `out`, `address`, `16U`,
 *          `false` and `conv`.
 *       5. Leave the switch.
 *    5. Case `'s'`: \llrref{LLR_FORMAT_PUT_CONVERSION_06}
 *       1. Call `put_string()` with the arguments `out`,
 *          `va_arg(*args, const char *)` and `conv`.
 *       2. Leave the switch.
 *    6. Case `'c'`: \llrref{LLR_FORMAT_PUT_CONVERSION_07}
 *       1. Call `put()` with the arguments `out` and
 *          `(char) va_arg(*args, int)`.
 *       2. Leave the switch.
 *    7. Case `'%%'`: \llrref{LLR_FORMAT_PUT_CONVERSION_08}
 *       1. Call `put()` with the arguments `out` and `'%%'`.
 *       2. Leave the switch.
 *    8. Otherwise (default): \llrref{LLR_FORMAT_PUT_CONVERSION_09}
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
 * - \hlrid{HLR_FORMAT_IS_DIGIT_01}: `is_digit()` shall tell whether a character
 *   of a format is a decimal digit.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_FORMAT_IS_DIGIT_01}: If `c` is not less than `'0'` and is not
 *   greater than `'9'`, `is_digit()` shall return true.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_IS_DIGIT_01}
 * - \llrid{LLR_FORMAT_IS_DIGIT_02}: If `c` is less than `'0'` or is greater
 *   than `'9'`, `is_digit()` shall return false.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_IS_DIGIT_01}
 *
 * @design
 * 1. Return `(c >= '0') && (c <= '9')`. \llrref{LLR_FORMAT_IS_DIGIT_01},
 *    \llrref{LLR_FORMAT_IS_DIGIT_02}
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
 * - \hlrid{HLR_FORMAT_JET_FORMAT_01}: `jet_format()` shall give the characters
 *   of a format to an output function of the caller, one at a time, with each
 *   conversion replaced by its argument, formatted with its flags, width,
 *   precision and length.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *   - \srqref{SRQ_JET_CONS_002}
 *
 * @llr
 * - \llrid{LLR_FORMAT_JET_FORMAT_01}: For each character of `format` before its
 *   `'\0'` that is not `'%%'` and is not part of a conversion, `jet_format()`
 *   shall call `put()` once with an output made of `emit` and `data`, and the
 *   character.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_JET_FORMAT_01}
 * - \llrid{LLR_FORMAT_JET_FORMAT_02}: `jet_format()` shall set `zero_pad` of
 *   the conversion that a `'%%'` starts to true if a `'0'` follows the `'%%'`,
 *   and to false if it does not.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_JET_FORMAT_01}
 * - \llrid{LLR_FORMAT_JET_FORMAT_03}: `jet_format()` shall set `width` of the
 *   conversion to the decimal number of the digits (`is_digit()`) that follow,
 *   and to 0 if no digit follows.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_JET_FORMAT_01}
 * - \llrid{LLR_FORMAT_JET_FORMAT_04}: If a `'.'` follows, `jet_format()` shall
 *   set `precision` of the conversion to the decimal number of the digits
 *   (`is_digit()`) that follow the `'.'`, and to 0 if no digit follows it.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_JET_FORMAT_01}
 * - \llrid{LLR_FORMAT_JET_FORMAT_05}: If no `'.'` follows, `jet_format()` shall
 *   set `precision` of the conversion to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_JET_FORMAT_01}
 * - \llrid{LLR_FORMAT_JET_FORMAT_06}: For each `'l'` that follows, before a
 *   character that is neither `'l'` nor `'z'`, `jet_format()` shall add 1 to
 *   `longs` of the conversion, which is 0 at first.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_JET_FORMAT_01}
 * - \llrid{LLR_FORMAT_JET_FORMAT_07}: For each `'z'` that follows, before a
 *   character that is neither `'l'` nor `'z'`, `jet_format()` shall set `longs`
 *   of the conversion to 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_JET_FORMAT_01}
 * - \llrid{LLR_FORMAT_JET_FORMAT_08}: If the character that follows these is
 *   not `'\0'`, `jet_format()` shall call `put_conversion()` once with the
 *   output, that character, the conversion and `args`, and go on with the
 *   character after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_JET_FORMAT_01}
 * - \llrid{LLR_FORMAT_JET_FORMAT_09}: If the character that follows these is
 *   `'\0'`, `jet_format()` shall return without calling `put_conversion()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_FORMAT_JET_FORMAT_01}
 *
 * @design
 * 1. Set `i` to `0U`. \llrref{LLR_FORMAT_JET_FORMAT_01},
 *    \llrref{LLR_FORMAT_JET_FORMAT_02}, \llrref{LLR_FORMAT_JET_FORMAT_03},
 *    \llrref{LLR_FORMAT_JET_FORMAT_04}, \llrref{LLR_FORMAT_JET_FORMAT_05},
 *    \llrref{LLR_FORMAT_JET_FORMAT_06}, \llrref{LLR_FORMAT_JET_FORMAT_07},
 *    \llrref{LLR_FORMAT_JET_FORMAT_08}, \llrref{LLR_FORMAT_JET_FORMAT_09}
 * 2. Set `out.emit` to `emit`. \llrref{LLR_FORMAT_JET_FORMAT_01},
 *    \llrref{LLR_FORMAT_JET_FORMAT_08}
 * 3. Set `out.data` to `data`. \llrref{LLR_FORMAT_JET_FORMAT_01},
 *    \llrref{LLR_FORMAT_JET_FORMAT_08}
 * 4. While `format[i]` is not equal to `'\0'`:
 *    \llrref{LLR_FORMAT_JET_FORMAT_01}, \llrref{LLR_FORMAT_JET_FORMAT_02},
 *    \llrref{LLR_FORMAT_JET_FORMAT_03}, \llrref{LLR_FORMAT_JET_FORMAT_04},
 *    \llrref{LLR_FORMAT_JET_FORMAT_05}, \llrref{LLR_FORMAT_JET_FORMAT_06},
 *    \llrref{LLR_FORMAT_JET_FORMAT_07}, \llrref{LLR_FORMAT_JET_FORMAT_08},
 *    \llrref{LLR_FORMAT_JET_FORMAT_09}
 *    1. If `format[i]` is not equal to `'%%'`:
 *       \llrref{LLR_FORMAT_JET_FORMAT_01}
 *       1. Call `put()` with the arguments the address of `out` and
 *          `format[i]`.
 *       2. Increment `i`.
 *    2. Otherwise: \llrref{LLR_FORMAT_JET_FORMAT_02},
 *       \llrref{LLR_FORMAT_JET_FORMAT_03}, \llrref{LLR_FORMAT_JET_FORMAT_04},
 *       \llrref{LLR_FORMAT_JET_FORMAT_05}, \llrref{LLR_FORMAT_JET_FORMAT_06},
 *       \llrref{LLR_FORMAT_JET_FORMAT_07}, \llrref{LLR_FORMAT_JET_FORMAT_08},
 *       \llrref{LLR_FORMAT_JET_FORMAT_09}
 *       1. Set `conv` to `{ false, 0U, 0U, 0U }`.
 *          \llrref{LLR_FORMAT_JET_FORMAT_02},
 *          \llrref{LLR_FORMAT_JET_FORMAT_03},
 *          \llrref{LLR_FORMAT_JET_FORMAT_05}, \llrref{LLR_FORMAT_JET_FORMAT_06}
 *       2. Increment `i`.
 *       3. If `format[i]` is equal to `'0'`: \llrref{LLR_FORMAT_JET_FORMAT_02}
 *          1. Set `conv.zero_pad` to `true`.
 *          2. Increment `i`.
 *       4. While `is_digit(format[i])` is true:
 *          \llrref{LLR_FORMAT_JET_FORMAT_03}
 *          1. Set `conv.width` to
 *             `(conv.width * 10U) + ((uint32_t) format[i] - (uint32_t) '0')`.
 *          2. Increment `i`.
 *       5. If `format[i]` is equal to `'.'`: \llrref{LLR_FORMAT_JET_FORMAT_04},
 *          \llrref{LLR_FORMAT_JET_FORMAT_05}
 *          1. Increment `i`.
 *          2. While `is_digit(format[i])` is true:
 *             1. Set `conv.precision` to `(conv.precision * 10U) +
 *                ((uint32_t) format[i] - (uint32_t) '0')`.
 *             2. Increment `i`.
 *       6. While `format[i]` is equal to `'l'` or `format[i]` is equal to
 *          `'z'`: \llrref{LLR_FORMAT_JET_FORMAT_06},
 *          \llrref{LLR_FORMAT_JET_FORMAT_07}
 *          1. Set `conv.longs` to `1U` if `format[i]` is equal to `'z'`,
 *             otherwise `(conv.longs + 1U)`.
 *          2. Increment `i`.
 *       7. If `format[i]` is not equal to `'\0'`:
 *          \llrref{LLR_FORMAT_JET_FORMAT_08}, \llrref{LLR_FORMAT_JET_FORMAT_09}
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
