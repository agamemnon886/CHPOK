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
 * guard isn't a reserved identifier; U suffixes for unsigned constants;
 * no restrict qualifiers; deviations are documented where a guideline
 * can't be followed.
 */

/*
 * MISRA C:2025 Rule 5.10 and Rule 20.15 deviation (this header): this is
 * the <stdio.h> header of libpok's own C library (partitions are built
 * freestanding, without the system one), so it declares the standard
 * names and defines BUFSIZ, EOF and the _IO*BF buffering modes. Rule 17.1:
 * vprintf() and vsnprintf() take a va_list, as in the C standard.
 */
#ifndef LIBJET_STDIO_H
#define LIBJET_STDIO_H

#include <stddef.h>
#include <stdarg.h>

// Buffer size use by setbuf(). (Not, that function itself is not accessible).
#define BUFSIZ 128U

// Input/output is fully buffered.
#define _IOFBF 1
// Input/output is line-buffered.
#define _IOLBF 2
// Input/output is not buffered at all.
#define _IONBF 3

#define EOF ((int)(-1))

typedef struct stdio_stream FILE;

extern FILE* stderr;
extern FILE* stdout;
extern FILE* stdin;

typedef size_t fpos_t;

int printf(const char * format, ...)
    __attribute__ ((format(printf, 1, 2)));
int vprintf(const char * format, va_list arg)
    __attribute__ ((format(printf, 1, 0)));

int snprintf(char * s, size_t n, const char * format, ...)
    __attribute__ ((format(printf, 3, 4)));
int vsnprintf(char * s, size_t n, const char * format, va_list arg)
    __attribute__ ((format(printf, 3, 0)));

int putchar(int c);
int puts(const char *s);

#endif /* __LIBJET_STDIO_H_ */
