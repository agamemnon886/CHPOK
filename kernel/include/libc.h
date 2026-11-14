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
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, the types and the
 * declarations of kernel/libc/printf.c, readline.c and monitor.c are moved
 * here.
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
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * MISRA C:2025 Rule 5.10 and Dir 1.2 deviation: this is the kernel's own C
 * library (the kernel is built freestanding, without the system one), so its
 * functions have the standard names. __attribute__((format)) is a GNU
 * extension that makes the compiler check printf formats.
 */

/** \cond */
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
/** \endcond */

#if defined (POK_NEEDS_CONSOLE) || defined (POK_NEEDS_DEBUG) || defined (POK_NEEDS_INSTRUMENTATION) || defined (POK_NEEDS_COVERAGE_INFOS)

/** \cond */
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
/** \endcond */
#endif /* NEEDS_CONSOLE or NEEDS_DEBUG */

/**
 * @typename `emit_character_t`
 *
 * @description
 * Emit single character: the output function of an emitter (emitter_t),
 * which outputs the character c to the output that private_data points to
 * (the console's buffer or a string). It returns nothing.
 */
typedef void (*emit_character_t)(char c, void *private_data);

/**
 * @macroname `BUF_SIZE`
 *
 * @description
 * The size of the buffer of printf()'s console output (s_file_t), in
 * characters.
 */
#define BUF_SIZE 128U

/**
 * @typename `s_file_t`
 *
 * @description
 * The buffer of the console output of printf(): the characters are
 * written to the console when it is full and at the end of the output.
 */
typedef struct
{
    char   buff[BUF_SIZE]; /**< The characters not written yet. */
    size_t pos;            /**< The number of characters in buff. */
} s_file_t;

/**
 * @typename `s_sprintf_t`
 *
 * @description
 * The output of snprintf(): a buffer of a given size and the number of
 * characters written.
 */
typedef struct
{
    char  *ptr;     /**< Where the next character goes. */
    size_t size;    /**< Size of the buffer, including the terminating
                         '\0'. */
    size_t written; /**< Number of characters written into the buffer. */
} s_sprintf_t;


/**
 * @typename `emitter_t`
 *
 * @description
 * Where the characters go: an output function and its output.
 */
typedef struct {
    emit_character_t emit_character; /**< The output function. */
    void *private_data; /**< The output, passed to emit_character. */
} emitter_t;

/**
 * @typename `format_spec_t`
 *
 * @description
 * Conversion specification: flags, field width, precision and length.
 */
typedef struct {
    /** Number of 'l' modifiers; 'z' counts as one. */
    uint32_t length;
    int32_t pad; /**< Field width. */
    /** The precision (0 if none is given; used only for strings). */
    int32_t precision;
    /** The '0' flag: pad with zeros instead of spaces. */
    bool pad_with_zero;
} format_spec_t;

/**
 * @macroname `BUFMEM`
 *
 * @description
 * The number of lines of the history.
 */
#define BUFMEM 10

/**
 * @macroname `BUFLEN`
 *
 * @description
 * The size of a line of the history, with its terminating NUL.
 */
#define BUFLEN 1024

/**
 * @typename `buff_t`
 *
 * @description
 * A line of the history of readline().
 */
typedef struct {
    char buf[BUFLEN]; /**< The line. */
    int32_t size; /**< Its length. */
    pok_bool_t head; /**< TRUE if it is the newest line of the history. */
    pok_bool_t tail; /**< TRUE if it is the oldest line of the history. */
} buff_t;

/**
 * @macroname `NCOMMANDS`
 *
 * @description
 * The number of commands of the monitor (the elements of its array commands).
 */
#define NCOMMANDS (sizeof(commands) / sizeof(commands[0]))

/**
 * @macroname `WHITESPACE`
 *
 * @description
 * The characters that separate the words of a command line.
 */
#define WHITESPACE "\t\r\n "

/**
 * @macroname `MAXARGS`
 *
 * @description
 * The maximum number of words of a command line, plus one (the NULL that ends
 * argv).
 */
#define MAXARGS 16

/**
 * @typename `command_t`
 *
 * @description
 * A command of the monitor: its name, its arguments and description (for help)
 * and the function that runs it.
 */
typedef struct {
    const char *name; /**< The name, which the command line starts with. */
    const char *argc; /**< Its arguments, as help shows them. */
    const char *desc; /**< Its description, as help shows it. */
    /** The function that runs it: argc words of the command line in argv
     * (argv[0] is the name); it returns 0 (a negative value would end the
     * monitor's loop). */
    int32_t (*func)(int32_t argc, char** argv);
} command_t;

/** \cond */
int32_t mon_help(int32_t argc, char **argv);

int32_t help_about(int32_t argc,char **argv);

int32_t print_partition(int32_t argc, char **argv); // List of partition

int32_t pause_N(int32_t argc, char **argv);// pause partition N. 

int32_t resume_N(int32_t argc, char **argv); // continue partition N

int32_t restart_N(int32_t argc, char **argv); // restart pertition N

int32_t exit_from_monitor(int32_t argc, char **argv); //exit from monitor

int32_t cpu_reset(int32_t argc, char **argv); //cpu reset

int32_t info_partition(int32_t argc,char ** argv);
/** \endcond */

#endif
