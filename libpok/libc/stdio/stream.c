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
 */

#include "stream.h"
#include <core/syscall.h>
#include <assert.h>

// Flush stream with *non-empty* buffer.
void stream_flush(struct stdio_stream* stream)
{
    assert(stream->buffer_pos > 0);

    stream->s_ops->write_bytes(stream, stream->buffer,
        stream->buffer_pos);
    // TODO: check errors
    stream->buffer_pos = 0;
}

void stdio_stream_emit_character(int c, void* _stream)
{
    struct stdio_stream* stream = _stream;
    // Unconditionally convert character into 'char'.
    char c_real = (char)c;

    if(stream->buffer_pos == stream->buffer_size) {
        // Buffer is full or doesn't exist.
        if(stream->buffer_pos) {
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
    stream->buffer[stream->buffer_pos++] = c_real;
    stream->bytes_written++;
    if(c_real == '\n' && stream->buffer_mode == _IOLBF) {
        // In case of newline and per-line mode, flush buffer immediately.
        stream_flush(stream);
    }
}

void stdio_stream_complete_operation(struct stdio_stream* stream)
{
    if(stream->buffer_pos > 0 && stream->buffer_mode == _IONBF) {
        // Flush buffer in non-buffered mode.
        stream_flush(stream);
    }
}

void stdio_operation_begin(struct stdio_operation* op, struct stdio_stream* stream)
{
    op->stream = stream;
    op->pos = 0;
    op->bytes_written = 0;
}

/*
 * Write collected characters into the stream.
 *
 * The stream's buffer is flushed (in non-buffered mode) before the lock
 * is released, so other process never sees our characters in it.
 */
static void stdio_operation_write(struct stdio_operation* op)
{
    struct stdio_stream* stream = op->stream;

    msection_enter(&stream->lock);

    for(size_t i = 0; i < op->pos; i++) {
        stdio_stream_emit_character(op->buffer[i], stream);
    }
    stdio_stream_complete_operation(stream);

    msection_leave(&stream->lock);

    op->pos = 0;
}

void stdio_operation_emit_character(int c, void* _op)
{
    struct stdio_operation* op = _op;

    if(op->pos == sizeof(op->buffer)) {
        // Long output: write it by chunks.
        stdio_operation_write(op);
    }

    op->buffer[op->pos++] = (char)c;
    op->bytes_written++;
}

size_t stdio_operation_end(struct stdio_operation* op)
{
    if(op->pos > 0) {
        stdio_operation_write(op);
    }

    return op->bytes_written;
}

/*
 * Write bytes into the console.
 * 
 * Callback '.write_bytes' for the stream.
 */
static size_t console_write_bytes(struct stdio_stream* stream,
    const char* bytes, size_t size)
{
    (void)stream;

    pok_syscall2 (POK_SYSCALL_CONSWRITE, (unsigned long)bytes, (unsigned long)size);
    // TODO: Check errors.
    return size;
}

static const struct stdio_stream_ops console_stream_ops = {
    .write_bytes = &console_write_bytes
};

static char console_buffer[BUFSIZ];
/* Stream, which is connected to (output) console. */
static struct stdio_stream console_stream = {
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
