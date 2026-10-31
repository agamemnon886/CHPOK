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

/* 
 * Generic definition of the stream.
 */

#ifndef __LIBJET_STDIO_STREAM_H__
#define __LIBJET_STDIO_STREAM_H__

#include <stdio.h>
#include <msection.h>

struct stdio_stream_ops;

struct stdio_stream
{
    /* Pointer to the buffer used by the stream. May be NULL. */
    char* buffer;
    /* Size of the buffer used by the stream. May be 0.*/
    size_t buffer_size;
    /* Mode of the buffer (_IOFBF, _IOLBF or _IONBF). */
    int buffer_mode;
    /* Relation of the buffer with underline object. */
    const struct stdio_stream_ops* s_ops;
    /* Current position in the buffer. */
    size_t buffer_pos;
    /* 
     * Number of bytes written into the stream.
     * 
     * This counter may be reseted manually when needed.
     */
    size_t bytes_written;
    /*
     * Protects the stream from concurrent accesses by processes of the
     * partition. Statically initialized with STDIO_STREAM_LOCK_INIT.
     *
     * Use stdio_operation_* functions for write into the stream.
     */
    struct msection lock;
};

/* Initializer of 'lock' field: free msection (see msection_init()). */
#define STDIO_STREAM_LOCK_INIT { .owner = JET_THREAD_ID_NONE, .msection_kernel_flags = 0 }

struct stdio_stream_ops
{
    /* 
     * Write bytes into underline object.
     * 
     * Return number of bytes which has been actually writen.
     */
    size_t (*write_bytes)(struct stdio_stream* stream,
        const char* bytes, size_t size);
    // Reading streams are currently unsupported.
};

/* Callback of type 'emit_character_t' for emit characters into stream.*/
void stdio_stream_emit_character(int c, void* _stream);
/* 
 * Complete operation for the stream.
 * 
 * Buffer may be used by 'stdio_stream_emit_character' even in _IONBF mode;
 * this function flushes the buffer.
 */
void stdio_stream_complete_operation(struct stdio_stream* stream);

/*
 * One output operation into the stream (printf(), puts(), ...).
 *
 * Characters of the operation are collected on the caller's stack and
 * written into the stream under its lock, so outputs of concurrent
 * operations do not mix or duplicate.
 *
 * Formatting (which reads caller's arguments and may fault on wrong
 * ones) is performed without the lock: a process which faults while
 * owning the lock would block other processes printing.
 */
struct stdio_operation
{
    struct stdio_stream* stream;
    /* Number of collected characters, not written into the stream yet. */
    size_t pos;
    /* Number of characters emitted by the operation. */
    size_t bytes_written;
    char buffer[BUFSIZ];
};

void stdio_operation_begin(struct stdio_operation* op, struct stdio_stream* stream);

/* Callback of type 'emit_character_t' for emit characters into operation.*/
void stdio_operation_emit_character(int c, void* _op);

/*
 * Complete the operation: write remaining characters into the stream
 * and complete operation for the stream.
 *
 * Return number of characters emitted by the operation.
 */
size_t stdio_operation_end(struct stdio_operation* op);

#endif /* __LIBJET_STDIO_STREAM_H_ */
