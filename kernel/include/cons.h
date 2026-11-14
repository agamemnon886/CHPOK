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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; __user/__kuser are renamed
 * JET_USER/JET_KUSER.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; struct jet_console is the typedef jet_console_t; jet_iostream_t
 * is used by its typedef name; Doxygen comments for the file and its types.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macro CONSBUFSIZE and the type
 * cons_buffer_t of kernel/libc/console.c are moved here; the enum types are
 * named with _e: pok_ret_e (was pok_ret_t); the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef JET_CONS_H
#define JET_CONS_H

#include <types.h>
#include <asp/cons.h>
#include <common.h>
#include <errno.h>
#include <stdint.h>

/* 
 * Do-nothing stream.
 * 
 * Everything which is written to it disappears.
 * 
 * Reading from it always returns 0.
 * 
 * Exposed for future use.
 */
extern jet_iostream_t jet_stream_null;

/**
 * @typename `jet_console_t`
 *
 * @description
 * Console which incorporates write and read streams.
 */
typedef struct {
    /**
     * Stream for write. Not NULL.
     */
    jet_iostream_t* write_stream;
    /**
     * Stream for read. Not NULL.
     */
    jet_iostream_t* read_stream;
} jet_console_t;

/* 
 * Main and debug consoles.
 * 
 * Exposed for future use.
 */
extern jet_console_t jet_console_main;
extern jet_console_t jet_console_debug;

/* 
 * Read characters from console until 'length' characters has been read
 * or none characters are left in the console.
 */
/** \cond */
size_t jet_console_read(char* s, size_t length);

/*
 * Write characters into console until all of them will be written.
 * 
 * Currently always returns 'length'.
 */
size_t jet_console_write(const char* s, size_t length);

/* 
 * Read characters from debug console until 'length' characters has been read
 * or none characters are left in the console.
 */
size_t jet_console_read_debug(char* s, size_t length);

/*
 * Write characters into debug console until all of them will be written.
 * 
 * Currently always returns 'length'.
 */
size_t jet_console_write_debug(const char* s, size_t length);


/* Syscall for write into main console from user space. */
pok_ret_e jet_console_write_user(const char* JET_USER s, size_t length);


// Functions for backward compatibility. TODO: Remove them and their usage.
pok_bool_t pok_cons_write (const char* s,
                       size_t length);
pok_bool_t pok_cons_write_1 (const char* s,
                       size_t length);

int32_t data_to_read_0(void);
int32_t read_serial_0(void);
int32_t data_to_read_1(void);
int32_t read_serial_1(void);

/** \endcond */

/**
 * @macroname `CONSBUFSIZE`
 *
 * @description
 * The size of the ring buffer of the characters read from the main console
 * (cons_buffer_t).
 */
#define CONSBUFSIZE 512U

/**
 * @typename `cons_buffer_t`
 *
 * @description
 * The ring buffer of the characters read from the main console and not yet
 * taken by getchar() (cons_intr() fills it).
 */
typedef struct {
    uint8_t buf[CONSBUFSIZE]; /**< The characters. */
    uint32_t rpos; /**< Where the next character is read. */
    uint32_t wpos; /**< Where the next character is written. */
} cons_buffer_t;

#endif /* JET_CONS_H */
