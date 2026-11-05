/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; deviations are documented where a
 * guideline can't be followed. memset(), strlen(), strnlen() and
 * snprintf() have the standard parameter and return types; vprintf(),
 * which had a non-standard signature, is no longer exported; strncasecmp()
 * is declared only in <types.h>.
 * Modified by Kirill Brazhnikov, 2026-11-03: long lines are wrapped at 80
 * columns.
 * Modified by Kirill Brazhnikov, 2026-11-04: the declarations of the GDB
 * server (struct regs, struct T_breakpoint, handle_exception(),
 * pok_gdb_thread*()) are removed: the server keeps them to itself or
 * declares them in <gdb.h>.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_KERNEL_LIBC_H
#define POK_KERNEL_LIBC_H

#include <config.h>
/*
 * MISRA C:2025 Rule 17.1 deviation: the printf family below
 * take variable arguments; they're the kernel's console output.
 */
#include <stdarg.h>
#include <types.h>

/*
 * MISRA C:2025 Rule 5.10 and Dir 1.2 deviation: this is the kernel's own C
 * library (the kernel is built freestanding, without the system one), so its
 * functions have the standard names. __attribute__((format)) is a GNU
 * extension that makes the compiler check printf formats.
 */

void  *memcpy(void * to, const void * from, size_t n);
void *memmove(void *dest, const void *src, size_t count);
int memcmp(const void *v1, const void *v2, size_t n);

/* avoid errors for windows */
/*__attribute__ ((weak))*/
void  *memset(void *dest, int val, size_t count);

size_t strlen (const char* str);
size_t strnlen (const char* str, size_t n);

char* strncpy(char* dest, const char* src, size_t n);

int   strcmp (const char *s1, const char *s2);
int   strncmp(const char *s1, const char *s2, size_t size);
// strncasecmp() is declared in <types.h>, which uses it for
// pok_compare_names().

char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);

void *memchr(const void *s, int c, size_t n);

#if defined (POK_NEEDS_CONSOLE) || defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_INSTRUMENTATION) || defined (POK_NEEDS_COVERAGE_INFOS)

int printf(const char *format, ...)__attribute__ ((format(printf, 1, 2)));

/* 
 * Unlike to standard C function, this returns number of characters which are ACTUALLY written.
 * (but terminating '\0' doesn't count, as usual).
 * 
 * Linux kernel has 'scnprintf' function with exactly that semantic.
 * 
 * TODO: Probably, rename this function for prevent result misconfusion?
 */
int snprintf(char *dst, size_t size, const char *format, ...)
    __attribute__ ((format(printf, 3, 4)));
char * readline(const char *prompt);
int getchar(void);
int getchar2(void);
void monitor(void);

void pok_monitor_thread(void);
void pok_monitor_thread_init(void);
#endif /* NEEDS_CONSOLE or NEEDS_DEBUG */

#endif
