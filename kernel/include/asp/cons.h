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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 */

#ifndef JET_ASP_CONS_H
#define JET_ASP_CONS_H

#include <types.h>

/*
 * I/O stream.
 */
struct jet_iostream
{
    /*
     * Write given string of given length into stream.
     * 
     * Return number of characters which has been written.
     * 
     * Never return 0.
     * 
     * TODO: Is returning error should be supported?
     * 
     * NULL means that writing is not supported.
     */
    size_t (*write)(const char* s, size_t length);

    /*
     * Read from the stream into given string.
     * 
     * At most 'length' characters will be read.
     * 
     * Return number of characters which has been read.
     * Return 0 if there is no available characters in the stream.
     * 
     * TODO: Is returning error should be supported?
     * 
     * NULL means that reading is not supported.
     */
    size_t (*read)(char* s, size_t length);
    
    /* If not NULL, this function should be called before use
     * .write and .read methods.
     * 
     * NOTE: This function may be called many times.
     */
    void (*init)(void);
};

/* 
 * Default streams provided by arch.
 * 
 * Any of them may be NULL which means "not supported".
 */

/* Default arch stream for reading. */
extern struct jet_iostream* ja_stream_default_read;
/* Default arch stream for writting. */
extern struct jet_iostream* ja_stream_default_write;
/* Default additional arch stream for reading. */
extern struct jet_iostream* ja_stream_default_read_debug;
/* Default additional arch stream for writting. */
extern struct jet_iostream* ja_stream_default_write_debug;

#endif /* JET_ASP_CONS_H */
