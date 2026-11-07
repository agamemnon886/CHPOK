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
 * Formatted output of the hypervisor: hv_printf() writes one message to
 * the hypervisor console, under the console lock, so that the messages of
 * different CPUs don't mix.
 *
 * Conversions: %d (%i), %u, %x, %p, %s, %c and %%; the flag 0, a field
 * width, and the length modifiers l, ll and z. Anything else is written
 * as it is.
 */

#include <hv/hv.h>

/*
 * MISRA C:2025 Rule 17.1 deviation (this file): hv_printf() takes
 * variable arguments, read with va_arg(); GCC checks each call against
 * its format (HV_PRINTF).
 */
#include <stdarg.h>

#define OUT_BUFFER_SIZE     128U
#define NUMBER_DIGITS       24U     // 2^64 has 20 decimal digits

// Characters not yet written to the console.
typedef struct {
    char buffer[OUT_BUFFER_SIZE];
    size_t length;
} output_t;

// A conversion specification without its conversion character.
typedef struct {
    bool zero_pad;
    uint32_t width;
    uint32_t longs;     // Number of l modifiers (z counts as one)
} conversion_t;

// Write the buffered characters to the console.
static void flush(output_t *out)
{
    hv_console_write(out->buffer, out->length);
    out->length = 0U;
}

// Add a character to the buffer (writing it out when it is full).
static void put(output_t *out, char c)
{
    if (out->length == OUT_BUFFER_SIZE) {
        flush(out);
    }
    out->buffer[out->length] = c;
    out->length++;
}

// Add count copies of a character.
static void put_repeated(output_t *out, char c, uint32_t count)
{
    for (uint32_t i = 0U; i < count; i++) {
        put(out, c);
    }
}

/*
 * Add a number in base 10 or 16, with its sign and the padding of the field
 * width.
 */
static void put_number(output_t *out, uint64_t value, uint32_t base,
                       bool negative, const conversion_t *conv)
{
    static const char digit_chars[] = "0123456789abcdef";
    char digits[NUMBER_DIGITS];
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

// Add a string, right-aligned in the field width ("(null)" for NULL).
static void put_string(output_t *out, const char *s,
                       const conversion_t *conv)
{
    const char *text = (s != NULL) ? s : "(null)";
    uint32_t length = 0U;

    while (text[length] != '\0') {
        length++;
    }
    if (conv->width > length) {
        put_repeated(out, ' ', conv->width - length);
    }
    for (uint32_t i = 0U; i < length; i++) {
        put(out, text[i]);
    }
}

// Next argument as an unsigned number of the conversion's length.
static uint64_t unsigned_argument(const conversion_t *conv,
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

// Next argument as a signed number of the conversion's length.
static int64_t signed_argument(const conversion_t *conv, va_list *args)
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

// Add the output of one conversion (its character c) with the next argument.
static void put_conversion(output_t *out, char c,
                           const conversion_t *conv, va_list *args)
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

// Check whether c is a decimal digit.
static bool is_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

/*
 * Formatted output to the hypervisor console (conversions above), written as
 * one message under the console lock.
 */
void hv_printf(const char *format, ...)
{
    output_t out;
    va_list args;
    size_t i = 0U;

    out.length = 0U;
    hv_console_lock();
    va_start(args, format);
    while (format[i] != '\0') {
        // Ordinary characters are copied; % starts a conversion.
        if (format[i] != '%') {
            put(&out, format[i]);
            i++;
        } else {
            conversion_t conv = { false, 0U, 0U };

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
            while ((format[i] == 'l') || (format[i] == 'z')) {
                conv.longs = (format[i] == 'z') ? 1U : (conv.longs + 1U);
                i++;
            }
            if (format[i] != '\0') {
                put_conversion(&out, format[i], &conv, &args);
                i++;
            }
        }
    }
    va_end(args);
    flush(&out);
    hv_console_unlock();
}
