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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file and
 * its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and declarations
 * are in include/hv/hv.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/src/printf.c (the common code and its headers are in
 * hypervisor/core); the formatter is jet_format() of common/format.c, shared
 * with the kernel; buffer_put() is its output function.
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 */

/**
 * @file
 * Formatted output of the hypervisor: hv_printf() writes one message to the
 * hypervisor console, under the console lock, so that the messages of different
 * CPUs don't mix. The formatter is jet_format() (common/format.c), which the
 * kernel uses too.
 */

#include <hv/hv.h>

/*
 * MISRA C:2025 Rule 17.1 deviation (this file): hv_printf() takes
 * variable arguments, which jet_format() reads with va_arg(); GCC checks
 * each call against its format (HV_PRINTF).
 * MISRA C:2025 Rule 11.5 deviation (this file): buffer_put() gets the
 * output buffer of hv_printf() as a pointer to void, through jet_format().
 */
#include <stdarg.h>
#include <common/format.h>

/**
 * @funcname `flush`
 *
 * @description
 * Writes the buffered characters to the console and empties the buffer.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `output_t *` | [in,out] The output buffer. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_PRINTF_FLUSH_01}: `flush()` shall write the buffered characters
 *   of a message to the console and empty the buffer.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_002}
 *
 * @llr
 * - \llrid{LLR_PRINTF_FLUSH_01}: `flush()` shall call `hv_console_write()` once
 *   with `buffer` and `length` of `out`, and then set `length` of `out` to 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_FLUSH_01}
 *
 * @design
 * 1. Call `hv_console_write()` with the arguments `out->buffer` and
 *    `out->length`. \llrref{LLR_PRINTF_FLUSH_01}
 * 2. Set `out->length` to `0U`. \llrref{LLR_PRINTF_FLUSH_01}
 *
 * @calls
 * - `hv_console_write()`
 */
static void flush(output_t *out)
{
    hv_console_write(out->buffer, out->length);
    out->length = 0U;
}

/**
 * @funcname `buffer_put`
 *
 * @description
 * Adds a character to the output buffer, writing the buffer out first when
 * it is full: the function to which jet_format() gives the characters of
 * hv_printf().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 * | `data` | `void *` | [in,out] The output buffer (an output_t). |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `output_t *` | The output buffer |
 *
 * @hlr
 * - \hlrid{HLR_PRINTF_BUFFER_PUT_01}: `buffer_put()` shall add a character of
 *   the formatted output to the buffer, writing the buffer out first when it is
 *   full.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_PRINTF_BUFFER_PUT_01}: If and only if `length` of the output
 *   that `data` points to is `OUT_BUFFER_SIZE`, `buffer_put()` shall call
 *   `flush()` once with the output, before it stores `c`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_BUFFER_PUT_01}
 * - \llrid{LLR_PRINTF_BUFFER_PUT_02}: `buffer_put()` shall store `c` in
 *   `buffer` of the output at the index `length` (the value after `flush()`, if
 *   it is called), and then increment `length`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_BUFFER_PUT_01}
 *
 * @design
 * 1. Set `out` to `data`. \llrref{LLR_PRINTF_BUFFER_PUT_01},
 *    \llrref{LLR_PRINTF_BUFFER_PUT_02}
 * 2. If `out->length` is equal to `OUT_BUFFER_SIZE`:
 *    \llrref{LLR_PRINTF_BUFFER_PUT_01}
 *    1. Call `flush()` with the argument `out`.
 * 3. Set `out->buffer[out->length]` to `c`. \llrref{LLR_PRINTF_BUFFER_PUT_02}
 * 4. Increment `out->length`. \llrref{LLR_PRINTF_BUFFER_PUT_02}
 *
 * @calls
 * - `flush()`
 */
static void buffer_put(char c, void *data)
{
    output_t *out = data;

    if (out->length == OUT_BUFFER_SIZE) {
        flush(out);
    }
    out->buffer[out->length] = c;
    out->length++;
}

/**
 * @funcname `hv_printf`
 *
 * @description
 * Formatted output to the hypervisor console (the conversions of
 * jet_format()), written as one message under the console lock.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `format` | `const char *` | [in] The format. |
 * | `...` | `...` | [in] The arguments of the conversions. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `out` | `output_t` | The output buffer |
 * | `args` | `va_list` | The variable arguments |
 *
 * @hlr
 * - \hlrid{HLR_PRINTF_HV_PRINTF_01}: `hv_printf()` shall write formatted text
 *   to the console of the hypervisor as one message, with the conversions of
 *   the shared formatter.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_CON_002}
 *   - \srqref{SRQ_HV_LIB_001}
 *
 * @llr
 * - \llrid{LLR_PRINTF_HV_PRINTF_01}: `hv_printf()` shall set `length` of a
 *   local output to 0 and then call, in this order, `hv_console_lock()`,
 *   `jet_format()` with `buffer_put`, the address of the output, `format` and
 *   the address of the list of the arguments that follow `format`, `flush()`
 *   with the address of the output, and `hv_console_unlock()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PRINTF_HV_PRINTF_01}
 *
 * @design
 * 1. Set `out.length` to `0U`. \llrref{LLR_PRINTF_HV_PRINTF_01}
 * 2. Call `hv_console_lock()`. \llrref{LLR_PRINTF_HV_PRINTF_01}
 * 3. Use the macro `va_start()` with the arguments `args` and `format`.
 *    \llrref{LLR_PRINTF_HV_PRINTF_01}
 * 4. Call `jet_format()` with the arguments `buffer_put`, the address of
 *    `out`, `format` and the address of `args`.
 *    \llrref{LLR_PRINTF_HV_PRINTF_01}
 * 5. Use the macro `va_end()` with the argument `args`.
 *    \llrref{LLR_PRINTF_HV_PRINTF_01}
 * 6. Call `flush()` with the address of `out`. \llrref{LLR_PRINTF_HV_PRINTF_01}
 * 7. Call `hv_console_unlock()`. \llrref{LLR_PRINTF_HV_PRINTF_01}
 *
 * @calls
 * - `hv_console_lock()`
 * - `jet_format()`
 * - `flush()`
 * - `hv_console_unlock()`
 */
void hv_printf(const char *format, ...)
{
    output_t out;
    va_list args;

    out.length = 0U;
    hv_console_lock();
    va_start(args, format);
    jet_format(buffer_put, &out, format, &args);
    va_end(args);
    flush(&out);
    hv_console_unlock();
}
