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
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from kernel/libc/printf.c
 * (kernel/libc is split up: the routines only the kernel uses are in
 * kernel/core); the formatter is jet_format() of common/format.c, shared with
 * the hypervisor; the kernel's own formatter functions are removed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design.
 */

/**
 * @file
 * printf() and snprintf() of the kernel, on the formatter that the kernel
 * shares with the hypervisor (jet_format(), common/format.c): %%s, %%p, %%x,
 * %%d, %%i, %%u, %%c and %%, with a field width, the 0 flag, a precision for
 * strings and the l, ll and z length modifiers. printf() writes to the main
 * console, one message at a time.
 */

#include <config.h>

#if defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_INSTRUMENTATION) || defined (POK_NEEDS_COVERAGE_INFOS)

#include <types.h>
#include <libc.h>
/*
 * MISRA C:2025 Rule 17.1 deviation (this file): printf() and snprintf() are
 * the kernel's formatted output; their arguments are read with va_arg(), by
 * jet_format().
 */
#include <stdarg.h>
#include <common/format.h>
#include <asp/arch.h>
#include <cons.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the callback gets its
 * private data as a pointer to void, which points to the object named by the
 * type it is converted to.
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
 * - \hlrid{HLR_PRINTF_BUF_FLUSH_01}: `buf_flush()` shall write the buffered
 *   text of the formatted output to the main console with interrupts disabled,
 *   and empty the buffer.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_002}
 *
 * @llr
 * - \llrid{LLR_PRINTF_BUF_FLUSH_01}: If `ja_preempt_enabled()` returns true,
 *   `buf_flush()` shall call `ja_preempt_disable()` before `pok_cons_write()`
 *   and `ja_preempt_enable()` after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_BUF_FLUSH_01}
 * - \llrid{LLR_PRINTF_BUF_FLUSH_02}: If `ja_preempt_enabled()` returns false,
 *   `buf_flush()` shall call neither `ja_preempt_disable()` nor
 *   `ja_preempt_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_BUF_FLUSH_01}
 * - \llrid{LLR_PRINTF_BUF_FLUSH_03}: `buf_flush()` shall call
 *   `pok_cons_write()` once with `buff` and `pos` of `file`, and then set `pos`
 *   of `file` to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_BUF_FLUSH_01}
 *
 * @design
 * 1. Call `ja_preempt_enabled()`, and store the result in
 *    `need_critical_section`. \llrref{LLR_PRINTF_BUF_FLUSH_01},
 *    \llrref{LLR_PRINTF_BUF_FLUSH_02}
 * 2. If `need_critical_section` is true: \llrref{LLR_PRINTF_BUF_FLUSH_01},
 *    \llrref{LLR_PRINTF_BUF_FLUSH_02}
 *    1. Call `ja_preempt_disable()`. \llrref{LLR_PRINTF_BUF_FLUSH_01}
 * 3. Call `pok_cons_write()` with the arguments `file->buff` and `file->pos`.
 *    \llrref{LLR_PRINTF_BUF_FLUSH_03}
 * 4. If `need_critical_section` is true: \llrref{LLR_PRINTF_BUF_FLUSH_01},
 *    \llrref{LLR_PRINTF_BUF_FLUSH_02}
 *    1. Call `ja_preempt_enable()`. \llrref{LLR_PRINTF_BUF_FLUSH_01}
 * 5. Set `file->pos` to `0U`. \llrref{LLR_PRINTF_BUF_FLUSH_03}
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
 * - \hlrid{HLR_PRINTF_INIT_BUFFERED_OUTPUT_01}: `init_buffered_output()` shall
 *   give an empty output buffer for the formatted output.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_002}
 *
 * @llr
 * - \llrid{LLR_PRINTF_INIT_BUFFERED_OUTPUT_01}: `init_buffered_output()` shall
 *   set the position of its static buffer to 0 and return its address.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_INIT_BUFFERED_OUTPUT_01}
 *
 * @design
 * 1. Set `res.pos` to `0U`. \llrref{LLR_PRINTF_INIT_BUFFERED_OUTPUT_01}
 * 2. Return the address of `res`. \llrref{LLR_PRINTF_INIT_BUFFERED_OUTPUT_01}
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
 * - \hlrid{HLR_PRINTF_BUF_PUTC_01}: `buf_putc()` shall add a character of the
 *   formatted output to the buffer, and write the buffer out when it is full.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_002}
 *
 * @llr
 * - \llrid{LLR_PRINTF_BUF_PUTC_01}: `buf_putc()` shall store `c` at `buff[pos]`
 *   of the output that `out` points to and increment its `pos`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_BUF_PUTC_01}
 * - \llrid{LLR_PRINTF_BUF_PUTC_02}: If and only if `pos` is then `BUF_SIZE`,
 *   `buf_putc()` shall call `buf_flush()` once with the output.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_BUF_PUTC_01}
 *
 * @design
 * 1. Set `file` to `out`. \llrref{LLR_PRINTF_BUF_PUTC_01}
 * 2. Set `file->buff[file->pos]` to `c`. \llrref{LLR_PRINTF_BUF_PUTC_01}
 * 3. Increment `file->pos`. \llrref{LLR_PRINTF_BUF_PUTC_01}
 * 4. If `file->pos` is equal to `BUF_SIZE`: \llrref{LLR_PRINTF_BUF_PUTC_02}
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
 * - \hlrid{HLR_PRINTF_SPRINTF_PUTC_01}: `sprintf_putc()` shall add a character
 *   of the formatted output to the destination buffer only while it has room
 *   for it and the final NUL.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *
 * @llr
 * - \llrid{LLR_PRINTF_SPRINTF_PUTC_01}: If `written` plus 1 is less than `size`
 *   of the output that `out` points to, `sprintf_putc()` shall store `c` at
 *   `ptr[written]` and increment `written`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_SPRINTF_PUTC_01}
 * - \llrid{LLR_PRINTF_SPRINTF_PUTC_02}: If `written` plus 1 is not less than
 *   `size`, `sprintf_putc()` shall leave the output as it is.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_SPRINTF_PUTC_01}
 *
 * @design
 * 1. Set `out_s` to `out`. \llrref{LLR_PRINTF_SPRINTF_PUTC_01},
 *    \llrref{LLR_PRINTF_SPRINTF_PUTC_02}
 * 2. If `(out_s->written + 1U)` is less than `out_s->size`:
 *    \llrref{LLR_PRINTF_SPRINTF_PUTC_01}, \llrref{LLR_PRINTF_SPRINTF_PUTC_02}
 *    1. Set `out_s->ptr[out_s->written]` to `c`.
 *       \llrref{LLR_PRINTF_SPRINTF_PUTC_01}
 *    2. Increment `out_s->written`. \llrref{LLR_PRINTF_SPRINTF_PUTC_01}
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
 * - \hlrid{HLR_PRINTF_CLOSE_BUFFERED_OUTPUT_01}: `close_buffered_output()`
 *   shall write out the rest of the buffered formatted output.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_002}
 *
 * @llr
 * - \llrid{LLR_PRINTF_CLOSE_BUFFERED_OUTPUT_01}: `close_buffered_output()`
 *   shall call `buf_flush()` for the buffer.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_CLOSE_BUFFERED_OUTPUT_01}
 *
 * @design
 * 1. Call `buf_flush()` with the argument `file`.
 *    \llrref{LLR_PRINTF_CLOSE_BUFFERED_OUTPUT_01}
 *
 * @calls
 * - `buf_flush()`
 */
static void close_buffered_output(s_file_t *file)
{
    buf_flush(file);
}


/**
 * @funcname `printf`
 *
 * @description
 * Formatted output to the main console (C standard, with the conversions of
 * jet_format()), written as one message with preemption disabled.
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
 * - \hlrid{HLR_PRINTF_PRINTF_01}: `printf()` shall write formatted text, with
 *   the conversions of the shared formatter, to the main console as one
 *   message, with preemption disabled while it is formatted and written.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_002}
 *   - \srqref{SRQ_JET_LIB_002}
 *
 * @llr
 * - \llrid{LLR_PRINTF_PRINTF_01}: `printf()` shall call `ja_preempt_disable()`,
 *   then `jet_format()` with `buf_putc`, the result of
 *   `init_buffered_output()`, `format` and the list of the arguments that
 *   follow it, then `close_buffered_output()` with that output, and return 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_PRINTF_01}
 * - \llrid{LLR_PRINTF_PRINTF_02}: `printf()` shall call `ja_preempt_enable()`
 *   at the end if and only if `ja_preempt_enabled()` returned true at entry.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_PRINTF_01}
 *
 * @design
 * 1. Call `ja_preempt_enabled()`, and store the result in `preempt_enabled`.
 *    \llrref{LLR_PRINTF_PRINTF_02}
 * 2. Call `ja_preempt_disable()`. \llrref{LLR_PRINTF_PRINTF_01}
 * 3. Call `init_buffered_output()`, and store the result in `out_file`.
 *    \llrref{LLR_PRINTF_PRINTF_01}
 * 4. Use the macro `va_start()` with the arguments `args` and `format`.
 *    \llrref{LLR_PRINTF_PRINTF_01}
 * 5. Call `jet_format()` with the arguments `buf_putc`, `out_file`, `format`
 *    and the address of `args`. \llrref{LLR_PRINTF_PRINTF_01}
 * 6. Use the macro `va_end()` with the argument `args`.
 *    \llrref{LLR_PRINTF_PRINTF_01}
 * 7. Call `close_buffered_output()` with the argument `out_file`.
 *    \llrref{LLR_PRINTF_PRINTF_01}
 * 8. If `preempt_enabled` is true: \llrref{LLR_PRINTF_PRINTF_02}
 *    1. Call `ja_preempt_enable()`.
 * 9. Return `0`. \llrref{LLR_PRINTF_PRINTF_01}
 *
 * @calls
 * - `ja_preempt_enabled()`
 * - `ja_preempt_disable()`
 * - `init_buffered_output()`
 * - `jet_format()`
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
    jet_format(buf_putc, out_file, format, &args);
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
 * the conversions of jet_format()); the output is cut to the size.
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
 * - \hlrid{HLR_PRINTF_SNPRINTF_01}: `snprintf()` shall format text into a
 *   buffer of a given size, cut to the size and terminated by a NUL, and give
 *   the number of characters written.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_LIB_002}
 *
 * @llr
 * - \llrid{LLR_PRINTF_SNPRINTF_01}: `snprintf()` shall call `jet_format()` once
 *   with `sprintf_putc`, an output of `dst`, `size` and no characters written,
 *   `format` and the list of the arguments that follow it, and return the
 *   number of characters written.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_SNPRINTF_01}
 * - \llrid{LLR_PRINTF_SNPRINTF_02}: If `size` is greater than 0, `snprintf()`
 *   shall store `'\0'` at `dst` after the characters written.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_SNPRINTF_01}
 * - \llrid{LLR_PRINTF_SNPRINTF_03}: If `size` is 0, `snprintf()` shall write
 *   nothing at `dst`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_SNPRINTF_01}
 *
 * @design
 * 1. Set `out.ptr` to `dst`. \llrref{LLR_PRINTF_SNPRINTF_01}
 * 2. Set `out.size` to `size`. \llrref{LLR_PRINTF_SNPRINTF_01}
 * 3. Set `out.written` to `0U`. \llrref{LLR_PRINTF_SNPRINTF_01}
 * 4. Use the macro `va_start()` with the arguments `args` and `format`.
 *    \llrref{LLR_PRINTF_SNPRINTF_01}
 * 5. Call `jet_format()` with the arguments `sprintf_putc`, the address of
 *    `out`, `format` and the address of `args`. \llrref{LLR_PRINTF_SNPRINTF_01}
 * 6. Use the macro `va_end()` with the argument `args`.
 *    \llrref{LLR_PRINTF_SNPRINTF_01}
 * 7. If `size` is greater than `0U`: \llrref{LLR_PRINTF_SNPRINTF_02},
 *    \llrref{LLR_PRINTF_SNPRINTF_03}
 *    1. Set `dst[out.written]` to `'\0'`. \llrref{LLR_PRINTF_SNPRINTF_02}
 * 8. Return `(int) out.written`. \llrref{LLR_PRINTF_SNPRINTF_01}
 *
 * @calls
 * - `jet_format()`
 */
int snprintf(char *dst, size_t size, const char *format, ...)
{
    s_sprintf_t out;
    out.ptr = dst;
    out.size = size;
    out.written = 0U;

    va_list args;
    va_start(args, format);
    jet_format(sprintf_putc, &out, format, &args);
    va_end(args);

    if (size > 0U) {
        dst[out.written] = '\0';
    }

    // The count doesn't fit into int only for outputs of 2 GiB or more.
    return (int) out.written;
}


#endif
