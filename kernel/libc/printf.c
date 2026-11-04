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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the formatting
 * code is rewritten like libpok's printf_emitter.c (fixed-width and Boolean
 * types, a parsed conversion specification, one break per switch clause,
 * characters emitted as char); vprintf(), which had a non-standard
 * signature, is now the static format_output(); the commented-out
 * floating-point code is removed. %p and %z now read a pointer and a size_t
 * (they read an unsigned int, which truncated 64-bit values), and the most
 * negative %d value is printed without overflow. snprintf() takes a size_t
 * and returns the number of characters written, as documented in libc.h (it
 * returned the space left).
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: digits is
 * defined in the function that uses it; deviations are documented where a
 * guideline can't be followed. Long lines are wrapped at 80 columns.
 */

#include <config.h>

#if defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_INSTRUMENTATION) || defined (POK_NEEDS_COVERAGE_INFOS)

#include <types.h>
#include <libc.h>
/*
 * MISRA C:2025 Rule 17.1 deviation (this file): printf() and snprintf() are
 * the kernel's formatted output; their arguments are read with va_arg().
 */
#include <stdarg.h>
#include <asp/arch.h>
#include <cons.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the callback gets its
 * private data as a pointer to void, which points to the object named by the
 * type it is converted to.
 * MISRA C:2025 Rule 18.4 deviation (this file): the format string is walked
 * with a pointer into it.
 */

/* Emit single character. */
typedef void (*emit_character_t)(char c, void *private_data);

#define BUF_SIZE 128U

/*
 * some types
 */

struct s_file
{
    char   buff[BUF_SIZE];
    size_t pos;
};

struct s_sprintf
{
    char  *ptr;
    size_t size; // Size of the buffer, including the terminating '\0'.
    size_t written; // Number of characters written into the buffer.
};

/*
 * buffered I/O
 */

static void buf_flush(struct s_file *file)
{
    /*
     * Normally, we do not want `printf` strings messed from different partitions.
     * 
     * Disable interrupts if they are not currently disabled.
     */
    pok_bool_t need_critical_section = ja_preempt_enabled();
    
    if(need_critical_section) {
        ja_preempt_disable();
    }
    (void) pok_cons_write (file->buff, file->pos);
    if(need_critical_section) {
        ja_preempt_enable();
    }

    file->pos = 0U;
}

static struct s_file* init_buffered_output(void)
{
    static struct s_file res;

    res.pos = 0U;
    return &res;
}

static void buf_putc(char c, void *out)
{
    struct s_file * file = out;

    file->buff[file->pos] = c;
    file->pos++;

    if (file->pos == BUF_SIZE) {
        buf_flush(file);
    }
}

static void sprintf_putc(char c, void *out)
{
    struct s_sprintf *out_s = out;

    // The last byte of the buffer is kept for the terminating '\0'.
    if ((out_s->written + 1U) < out_s->size) {
        out_s->ptr[out_s->written] = c;
        out_s->written++;
    }
}

static void close_buffered_output(struct s_file *file)
{
    buf_flush(file);
}


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
//XXX for now precision is used only for strings
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

/* Generic printf-like function. */
static void format_output(emit_character_t emit_character, void *out,
    const char* format, va_list *args)
{
    struct emitter e;
    const char *f = format;

    e.emit_character = emit_character;
    e.private_data = out;

    while (*f != '\0') {
        if (*f == '%') {
            f = handle_fmt(&e, f + 1, args);
        } else {
            emit(&e, *f);
            f++;
        }
    }
}

//Why no void?!
int printf(const char *format, ...)
{
    pok_bool_t preempt_enabled = ja_preempt_enabled();
    ja_preempt_disable();

    va_list args;
    struct s_file* out_file = init_buffered_output();

    va_start(args, format);
    format_output(buf_putc, out_file, format, &args);
    va_end(args);

    close_buffered_output(out_file);

    if(preempt_enabled) {
        ja_preempt_enable();
    }

    return 0;
}

int snprintf(char *dst, size_t size, const char *format, ...)
{
    struct s_sprintf out;
    out.ptr = dst;
    out.size = size;
    out.written = 0U;

    va_list args;
    va_start(args, format);
    format_output(sprintf_putc, &out, format, &args);
    va_end(args);

    if (size > 0U) {
        dst[out.written] = '\0';
    }

    // The count doesn't fit into int only for outputs of 2 GiB or more.
    return (int) out.written;
}


#endif
