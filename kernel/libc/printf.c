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
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macro and the types are moved
 * to kernel/include/libc.h, and the structures are typedefs (s_file_t,
 * s_sprintf_t, emitter_t, format_spec_t).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 */

/**
 * @file
 * printf() and snprintf() of the kernel: %%s, %%p, %%x, %%d, %%i, %%u, %%c and
 * %%, with a field width, the 0 flag, a precision for strings and the l, ll and
 * z length modifiers. printf() writes to the main console, one message at a
 * time.
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

/*
 * buffered I/O
 */

/**
 * @funcname `buf_flush`
 *
 * @description
 * Writes the buffered characters of printf() to the main console, with
 * interrupts disabled (so that messages don't mix), and empties the buffer.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `file` | `s_file_t *` | [in,out] The buffer. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `need_critical_section` | `pok_bool_t` | Whether interrupts were enabled |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `ja_preempt_enabled()`, and store the result in
 *    `need_critical_section`.
 * 2. If `need_critical_section` is true:
 *    1. Call `ja_preempt_disable()`.
 * 3. Call `pok_cons_write()` with the arguments `file->buff` and `file->pos`.
 * 4. If `need_critical_section` is true:
 *    1. Call `ja_preempt_enable()`.
 * 5. Set `file->pos` to `0U`.
 *
 * @calls
 * - `ja_preempt_enabled()`
 * - `ja_preempt_disable()`
 * - `pok_cons_write()`
 * - `ja_preempt_enable()`
 */
static void buf_flush(s_file_t *file)
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

/**
 * @funcname `init_buffered_output`
 *
 * @description
 * Returns the empty output buffer of printf().
 *
 * @return The buffer.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `res` | `static s_file_t` | The buffer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `res.pos` to `0U`.
 * 2. Return the address of `res`.
 *
 * @calls
 * None.
 */
static s_file_t* init_buffered_output(void)
{
    static s_file_t res;

    res.pos = 0U;
    return &res;
}

/**
 * @funcname `buf_putc`
 *
 * @description
 * The character output of printf(): adds a character to the buffer, writing the
 * buffer out when it is full.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 * | `out` | `void *` | [in,out] The buffer. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `file` | `s_file_t *` | The buffer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `file` to `out`.
 * 2. Set `file->buff[file->pos]` to `c`.
 * 3. Increment `file->pos`.
 * 4. If `file->pos` is equal to `BUF_SIZE`:
 *    1. Call `buf_flush()` with the argument `file`.
 *
 * @calls
 * - `buf_flush()`
 */
static void buf_putc(char c, void *out)
{
    s_file_t * file = out;

    file->buff[file->pos] = c;
    file->pos++;

    if (file->pos == BUF_SIZE) {
        buf_flush(file);
    }
}

/**
 * @funcname `sprintf_putc`
 *
 * @description
 * The character output of snprintf(): adds a character to the destination, if
 * there is room (one byte stays for the NUL).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 * | `out` | `void *` | [in,out] The destination and its size. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out_s` | `s_sprintf_t *` | The destination |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `out_s` to `out`.
 * 2. If `(out_s->written + 1U)` is less than `out_s->size`:
 *    1. Set `out_s->ptr[out_s->written]` to `c`.
 *    2. Increment `out_s->written`.
 *
 * @calls
 * None.
 */
static void sprintf_putc(char c, void *out)
{
    s_sprintf_t *out_s = out;

    // The last byte of the buffer is kept for the terminating '\0'.
    if ((out_s->written + 1U) < out_s->size) {
        out_s->ptr[out_s->written] = c;
        out_s->written++;
    }
}

/**
 * @funcname `close_buffered_output`
 *
 * @description
 * Writes out what remains in the buffer of printf().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `file` | `s_file_t *` | [in,out] The buffer. |
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
 * 1. Call `buf_flush()` with the argument `file`.
 *
 * @calls
 * - `buf_flush()`
 */
static void close_buffered_output(s_file_t *file)
{
    buf_flush(file);
}


/**
 * @funcname `emit`
 *
 * @description
 * Outputs one character with an emitter's output function.
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
 * Outputs a number in a base, with its sign and the padding of the field width
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
 * Outputs a string ("(null)" for NULL), right-aligned in the field width, cut
 * at the precision.
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
 * | `len` | `size_t` | The number of characters output |
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
 * Reads the next variable argument as an unsigned number of the conversion's
 * length.
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
 * Reads the next variable argument as a signed number of the conversion's
 * length.
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
 * @funcname `print_conversion`
 *
 * @description
 * Outputs one conversion with the next argument; an unknown conversion is
 * output as it is.
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
 * | `value` | `int64_t` | The argument of %%d and %%i |
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
 *    6. Case `'c'`:
 *       1. Call `emit()` with the arguments `e` and
 *          `(char) va_arg(*parg, int)`.
 *       2. Leave the switch.
 *    7. Case `'%%'`:
 *       1. Call `emit()` with the arguments `e` and `'%%'`.
 *       2. Leave the switch.
 *    8. Otherwise (default):
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

//XXX for now precision is used only for strings

/**
 * @funcname `handle_fmt`
 *
 * @description
 * Reads a conversion specification after a %: the 0 flag, the width, the
 * precision and the length modifiers, then outputs the conversion.
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
 * @funcname `format_output`
 *
 * @description
 * Outputs a format with its arguments through an output function.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `emit_character` | `emit_character_t` | [in] The output function. |
 * | `out` | `void *` | [in] The data of the output function. |
 * | `format` | `const char *` | [in] The format. |
 * | `args` | `va_list *` | [in,out] The variable arguments. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
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
 * 3. Set `e.private_data` to `out`.
 * 4. While the value at `f` is not equal to `'\0'`:
 *    1. If the value at `f` is equal to `'%%'`:
 *       1. Call `handle_fmt()` with the arguments the address of `e`, `f + 1`
 *          and `args`, and store the result in `f`.
 *    2. Otherwise:
 *       1. Call `emit()` with the arguments the address of `e` and the value at
 *          `f`.
 *       2. Increment `f`.
 *
 * @calls
 * - `handle_fmt()`
 * - `emit()`
 */
static void format_output(emit_character_t emit_character, void *out,
    const char* format, va_list *args)
{
    emitter_t e;
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

/**
 * @funcname `printf`
 *
 * @description
 * Formatted output to the main console (C standard, with the conversions of
 * this file), written as one message with preemption disabled.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `format` | `const char *` | [in] The format. |
 * | `...` | `...` | [in] The arguments of the conversions. |
 *
 * @return 0.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `args` | `va_list` | The variable arguments |
 * | `out_file` | `s_file_t *` | The output buffer |
 * | `preempt_enabled` | `pok_bool_t` | Whether preemption was enabled |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `ja_preempt_enabled()`, and store the result in `preempt_enabled`.
 * 2. Call `ja_preempt_disable()`.
 * 3. Call `init_buffered_output()`, and store the result in `out_file`.
 * 4. Use the macro `va_start()` with the arguments `args` and `format`.
 * 5. Call `format_output()` with the arguments `buf_putc`, `out_file`, `format`
 *    and the address of `args`.
 * 6. Use the macro `va_end()` with the argument `args`.
 * 7. Call `close_buffered_output()` with the argument `out_file`.
 * 8. If `preempt_enabled` is true:
 *    1. Call `ja_preempt_enable()`.
 * 9. Return `0`.
 *
 * @calls
 * - `ja_preempt_enabled()`
 * - `ja_preempt_disable()`
 * - `init_buffered_output()`
 * - `format_output()`
 * - `close_buffered_output()`
 * - `ja_preempt_enable()`
 */
int printf(const char *format, ...)
{
    pok_bool_t preempt_enabled = ja_preempt_enabled();
    ja_preempt_disable();

    va_list args;
    s_file_t* out_file = init_buffered_output();

    va_start(args, format);
    format_output(buf_putc, out_file, format, &args);
    va_end(args);

    close_buffered_output(out_file);

    if(preempt_enabled) {
        ja_preempt_enable();
    }

    return 0;
}

/**
 * @funcname `snprintf`
 *
 * @description
 * Formatted output into a buffer of a size, NUL-terminated (C standard, with
 * the conversions of this file); the output is cut to the size.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dst` | `char *` | [out] The buffer. |
 * | `size` | `size_t` | [in] Its size. |
 * | `format` | `const char *` | [in] The format. |
 * | `...` | `...` | [in] The arguments of the conversions. |
 *
 * @return The number of characters written (without the NUL).
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `args` | `va_list` | The variable arguments |
 * | `out` | `s_sprintf_t` | The buffer and what was written |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `out.ptr` to `dst`.
 * 2. Set `out.size` to `size`.
 * 3. Set `out.written` to `0U`.
 * 4. Use the macro `va_start()` with the arguments `args` and `format`.
 * 5. Call `format_output()` with the arguments `sprintf_putc`, the address of
 *    `out`, `format` and the address of `args`.
 * 6. Use the macro `va_end()` with the argument `args`.
 * 7. If `size` is greater than `0U`:
 *    1. Set `dst[out.written]` to `'\0'`.
 * 8. Return `(int) out.written`.
 *
 * @calls
 * - `format_output()`
 */
int snprintf(char *dst, size_t size, const char *format, ...)
{
    s_sprintf_t out;
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
