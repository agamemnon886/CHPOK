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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; U suffixes for unsigned constants;
 * characters are emitted as char; deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; msection_t is used by its typedef name.
 */

/* 
 * Generic definition of the stream.
 */

#ifndef LIBJET_STDIO_STREAM_H
#define LIBJET_STDIO_STREAM_H

/*
 * MISRA C:2025 Rule 21.6 deviation: this header is internal to the
 * <stdio.h> implementation of libpok's C library.
 */
#include <stdio.h>
#include <msection.h>

/*
 * Writes bytes into the object under a stream: the stream, the bytes and
 * their number. Returns the number of bytes actually written.
 */
/**
 * @typename `stdio_write_bytes_fn_t`
 *
 * @description
 * Writes bytes into the object under a stream (the write_bytes operation of
 * stdio_stream_ops_t): its parameters are the stream, the bytes and their
 * number; it returns the number of bytes actually written.
 */
typedef size_t (*stdio_write_bytes_fn_t)(FILE* stream,
    const char* bytes, size_t size);

/**
 * @typename `stdio_stream_ops_t`
 *
 * @description
 * The operations of a stream on the object under it (the console).
 */
typedef struct {
    /**
     * Write bytes into underline object.
     *
     * Return number of bytes which has been actually writen.
     */
    stdio_write_bytes_fn_t write_bytes;
    // Reading streams are currently unsupported.
} stdio_stream_ops_t;

/**
 * @typename `FILE`
 *
 * @description
 * A stream of the C library (FILE): its buffer and the operations on the object
 * under it.
 */
struct stdio_stream
{
    /**
     * Pointer to the buffer used by the stream. May be NULL.
     */
    char* buffer;
    /**
     * Size of the buffer used by the stream. May be 0.
     */
    size_t buffer_size;
    /**
     * Mode of the buffer (_IOFBF, _IOLBF or _IONBF).
     */
    int buffer_mode;
    /**
     * Relation of the buffer with underline object.
     */
    const stdio_stream_ops_t* s_ops;
    /**
     * Current position in the buffer.
     */
    size_t buffer_pos;
    /**
     * Number of bytes written into the stream.
     *
     * This counter may be reseted manually when needed.
     */
    size_t bytes_written;
    /**
     * Protects the stream from concurrent accesses by processes of the
     * partition. Statically initialized with STDIO_STREAM_LOCK_INIT.
     *
     * Use stdio_operation_* functions for write into the stream.
     */
    msection_t lock;
};

/* Initializer of 'lock' field: free msection (see msection_init()). */

/**
 * @macroname `STDIO_STREAM_LOCK_INIT`
 *
 * @description
 * The initializer of the lock of a stream: a free critical section (see
 * msection_init()).
 */
#define STDIO_STREAM_LOCK_INIT \
    { .owner = JET_THREAD_ID_NONE, .msection_kernel_flags = 0U }

/* Callback of type 'emit_character_t' for emit characters into stream.*/
/** \cond */
void stdio_stream_emit_character(char c, void* _stream);
/* 
 * Complete operation for the stream.
 * 
 * Buffer may be used by 'stdio_stream_emit_character' even in _IONBF mode;
 * this function flushes the buffer.
 */
void stdio_stream_complete_operation(FILE* stream);
/** \endcond */

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
/**
 * @typename `stdio_operation_t`
 *
 * @description
 * One output operation into a stream (printf(), puts(), ...). Its characters
 * are collected on the caller's stack and written into the stream under its
 * lock, so outputs of concurrent operations do not mix or duplicate. The
 * formatting (which reads the caller's arguments and may fault on wrong ones)
 * is done without the lock: a process which faults while owning the lock would
 * block the other processes printing.
 */
typedef struct {
    FILE* stream; /**< The stream written. */
    /**
     * Number of collected characters, not written into the stream yet.
     */
    size_t pos;
    /**
     * Number of characters emitted by the operation.
     */
    size_t bytes_written;
    char buffer[BUFSIZ]; /**< The characters collected. */
} stdio_operation_t;

/** \cond */
void stdio_operation_begin(stdio_operation_t* op, FILE* stream);

/* Callback of type 'emit_character_t' for emit characters into operation.*/
void stdio_operation_emit_character(char c, void* _op);

/*
 * Complete the operation: write remaining characters into the stream
 * and complete operation for the stream.
 *
 * Return number of characters emitted by the operation.
 */
size_t stdio_operation_end(stdio_operation_t* op);
/** \endcond */

#endif /* __LIBJET_STDIO_STREAM_H_ */
