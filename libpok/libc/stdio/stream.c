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
 * Modified by Kirill Brazhnikov, 2026-10-31: thread-safe output:
 * per-stream lock and per-call output buffer.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: essentially
 * Boolean conditions; U suffixes for unsigned constants; discarded
 * results are cast to void; internal linkage for file-local definitions;
 * characters are emitted as char; the console system call's result is
 * discarded explicitly; deviations are documented where a guideline can't
 * be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: increments and
 * assignments are separate statements; operands of mixed precedence are
 * parenthesized; deviations are documented where a guideline can't be
 * followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * The output streams of libpok (stdout and stderr: the console) and the
 * output operations into them: an operation collects its characters on
 * the caller's stack and writes them under the stream's lock, so outputs
 * of concurrent processes don't mix.
 */

#include "stream.h"
#include <core/syscall.h>
#include <assert.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the callback gets its
 * private data as a pointer to void, which points to the object named by the
 * type it is converted to.
 */

/**
 * @funcname `stream_flush`
 *
 * @description
 * Writes out a stream's buffer, which isn't empty.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stream` | `FILE *` | [in,out] The stream. |
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
 * 1. Check that `stream->buffer_pos > 0U` holds (assert: stop otherwise).
 * 2. Call the function `stream->s_ops->write_bytes` with the arguments
 *    `stream`, `stream->buffer` and `stream->buffer_pos`.
 * 3. Set `stream->buffer_pos` to `0`.
 *
 * @calls
 * - `printf()`
 * - `abort()`
 * - `stream->s_ops->write_bytes()` (indirect)
 */
static void stream_flush(FILE* stream)
{
    /* MISRA C:2025 Rule 21.8: see the deviation for assert() in <assert.h>. */
    assert(stream->buffer_pos > 0U);

    stream->s_ops->write_bytes(stream, stream->buffer,
        stream->buffer_pos);
    // TODO: check errors
    stream->buffer_pos = 0;
}

/**
 * @funcname `stdio_stream_emit_character`
 *
 * @description
 * Writes a character into a stream: into its buffer (flushed when full, or at a
 * newline in line mode), or directly if it has no buffer.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 * | `_stream` | `void *` | [in,out] The stream. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c_real` | `char` | The character |
 * | `stream` | `FILE *` | The stream |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `stream` to `_stream`.
 * 2. Set `c_real` to `c`.
 * 3. If `stream->buffer_pos` is equal to `stream->buffer_size`:
 *    1. If `stream->buffer_pos` is not equal to `0U`:
 *       1. Call `stream_flush()` with the argument `stream`.
 *    2. Otherwise:
 *       1. Call the function `stream->s_ops->write_bytes` with the arguments
 *          `stream`, the address of `c_real` and `1`.
 *       2. Increment `stream->bytes_written`.
 *       3. Return.
 * 4. Set `stream->buffer[stream->buffer_pos]` to `c_real`.
 * 5. Increment `stream->buffer_pos`.
 * 6. Increment `stream->bytes_written`.
 * 7. If `c_real` is equal to `'\n'` and `stream->buffer_mode` is equal to
 *    `_IOLBF`:
 *    1. Call `stream_flush()` with the argument `stream`.
 *
 * @calls
 * - `stream_flush()`
 * - `stream->s_ops->write_bytes()` (indirect)
 */
void stdio_stream_emit_character(char c, void* _stream)
{
    FILE* stream = _stream;
    char c_real = c;

    if(stream->buffer_pos == stream->buffer_size) {
        // Buffer is full or doesn't exist.
        if(stream->buffer_pos != 0U) {
            // Buffer is full. Need to flush it before continue.
            stream_flush(stream);
        }
        else {
            // Buffer doesn't exist. Unconditionally use non-buffered mode.
            stream->s_ops->write_bytes(stream, &c_real, 1);
            stream->bytes_written++;
            // TODO: check errors
            return;
        }
    }

    // Buffer is available. Use it.
    stream->buffer[stream->buffer_pos] = c_real;
    stream->buffer_pos++;
    stream->bytes_written++;
    if((c_real == '\n') && (stream->buffer_mode == _IOLBF)) {
        // In case of newline and per-line mode, flush buffer immediately.
        stream_flush(stream);
    }
}

/**
 * @funcname `stdio_stream_complete_operation`
 *
 * @description
 * Completes an operation on a stream: the buffer may be used even in _IONBF
 * mode, and is flushed then.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stream` | `FILE *` | [in,out] The stream. |
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
 * 1. If `stream->buffer_pos` is greater than `0U` and `stream->buffer_mode` is
 *    equal to `_IONBF`:
 *    1. Call `stream_flush()` with the argument `stream`.
 *
 * @calls
 * - `stream_flush()`
 */
void stdio_stream_complete_operation(FILE* stream)
{
    if((stream->buffer_pos > 0U) && (stream->buffer_mode == _IONBF)) {
        // Flush buffer in non-buffered mode.
        stream_flush(stream);
    }
}

/**
 * @funcname `stdio_operation_begin`
 *
 * @description
 * Starts an output operation into a stream.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `op` | `stdio_operation_t *` | [out] The operation. |
 * | `stream` | `FILE *` | [in] The stream. |
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
 * 1. Set `op->stream` to `stream`.
 * 2. Set `op->pos` to `0`.
 * 3. Set `op->bytes_written` to `0`.
 *
 * @calls
 * None.
 */
void stdio_operation_begin(stdio_operation_t* op, FILE* stream)
{
    op->stream = stream;
    op->pos = 0;
    op->bytes_written = 0;
}

/**
 * @funcname `stdio_operation_write`
 *
 * @description
 * Writes the characters collected by an operation into its stream. The stream's
 * buffer is flushed (in non-buffered mode) before the lock is released, so
 * other processes never see these characters in it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `op` | `stdio_operation_t *` | [in,out] The operation. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `size_t` | The index of the character |
 * | `stream` | `FILE *` | The stream |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `stream` to `op->stream`.
 * 2. Call `msection_enter()` with the address of `stream->lock`.
 * 3. Set `i` to `0`; then while `i` is less than `op->pos`, repeat (after each
 *    pass: increment `i`):
 *    1. Call `stdio_stream_emit_character()` with the arguments `op->buffer[i]`
 *       and `stream`.
 * 4. Call `stdio_stream_complete_operation()` with the argument `stream`.
 * 5. Call `msection_leave()` with the address of `stream->lock`.
 * 6. Set `op->pos` to `0`.
 *
 * @calls
 * - `msection_enter()`
 * - `stdio_stream_emit_character()`
 * - `stdio_stream_complete_operation()`
 * - `msection_leave()`
 */
static void stdio_operation_write(stdio_operation_t* op)
{
    FILE* stream = op->stream;

    msection_enter(&stream->lock);

    for(size_t i = 0; i < op->pos; i++) {
        stdio_stream_emit_character(op->buffer[i], stream);
    }
    stdio_stream_complete_operation(stream);

    msection_leave(&stream->lock);

    op->pos = 0;
}

/**
 * @funcname `stdio_operation_emit_character`
 *
 * @description
 * The output function of an operation: collects a character (a long output is
 * written by chunks).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `char` | [in] The character. |
 * | `_op` | `void *` | [in,out] The operation. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `op` | `stdio_operation_t *` | The operation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `op` to `_op`.
 * 2. If `op->pos` is equal to `sizeof(op->buffer)`:
 *    1. Call `stdio_operation_write()` with the argument `op`.
 * 3. Set `op->buffer[op->pos]` to `c`.
 * 4. Increment `op->pos`.
 * 5. Increment `op->bytes_written`.
 *
 * @calls
 * - `stdio_operation_write()`
 */
void stdio_operation_emit_character(char c, void* _op)
{
    stdio_operation_t* op = _op;

    if(op->pos == sizeof(op->buffer)) {
        // Long output: write it by chunks.
        stdio_operation_write(op);
    }

    op->buffer[op->pos] = c;
    op->pos++;
    op->bytes_written++;
}

/**
 * @funcname `stdio_operation_end`
 *
 * @description
 * Completes an operation: writes the remaining characters into the stream.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `op` | `stdio_operation_t *` | [in,out] The operation. |
 *
 * @return The number of characters emitted by the operation.
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
 * 1. If `op->pos` is greater than `0U`:
 *    1. Call `stdio_operation_write()` with the argument `op`.
 * 2. Return `op->bytes_written`.
 *
 * @calls
 * - `stdio_operation_write()`
 */
size_t stdio_operation_end(stdio_operation_t* op)
{
    if(op->pos > 0U) {
        stdio_operation_write(op);
    }

    return op->bytes_written;
}

/**
 * @funcname `console_write_bytes`
 *
 * @description
 * Writes bytes to the console (the write_bytes callback of the console stream).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `stream` | `FILE *` | [in] The stream (not used). |
 * | `bytes` | `const char *` | [in] The bytes. |
 * | `size` | `size_t` | [in] Their number. |
 *
 * @return size.
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
 * 1. Mark `stream` as not used.
 * 2. Mark
 *    `pok_syscall2 (POK_SYSCALL_CONSWRITE, (uintptr_t)bytes, (uintptr_t)size)`
 *    as not used.
 * 3. Return `size`.
 *
 * @calls
 * - `lja_do_syscall()`
 */
static size_t console_write_bytes(FILE* stream,
    const char* bytes, size_t size)
{
    (void)stream;

    /*
     * MISRA C:2025 Rule 11.4 deviation: system call arguments are passed as
     * integers; the kernel converts the address back to a pointer.
     */
    (void) pok_syscall2 (POK_SYSCALL_CONSWRITE, (uintptr_t)bytes,
                         (uintptr_t)size);
    // TODO: Check errors.
    return size;
}

static const stdio_stream_ops_t console_stream_ops = {
    .write_bytes = &console_write_bytes
};

static char console_buffer[BUFSIZ];
/* Stream, which is connected to (output) console. */
static FILE console_stream = {
    .buffer = console_buffer,
    .buffer_size = BUFSIZ,
    .buffer_mode = _IONBF,
    .s_ops = &console_stream_ops,
    .buffer_pos = 0,
    .bytes_written = 0,
    .lock = STDIO_STREAM_LOCK_INIT,
};

FILE* stderr = &console_stream;
FILE* stdout = &console_stream;
