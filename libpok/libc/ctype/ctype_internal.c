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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; internal linkage for file-local definitions;
 * the classification functions return 1 or 0.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: #include
 * directives come before the code; deviations are documented where a
 * guideline can't be followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macro GET_CTYPE_CLASS is moved
 * to ctype_internal.h.
 */

/**
 * @file
 * <ctype.h> of libpok: the character classes and case conversions, from
 * the tables of the current locale (only "C").
 */

#include <ctype.h>
#include <stdint.h>
#include "ctype_internal.h"
#include "ctype_tables_c.h"

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): the tables have an entry for
 * EOF (-1) first, so they are indexed from their start plus one.
 */

/**
 * @funcname `class_is`
 *
 * @description
 * Tells whether the bits of a mask of a character's class are equal to a value.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 * | `mask` | `uint8_t` | [in] The mask. |
 * | `value` | `uint8_t` | [in] The value. |
 *
 * @return 1 if they are, 0 otherwise.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctype_table_classification` | `unsigned char *` | The character classes of each character |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `1` if `(GET_CTYPE_CLASS(c) & mask)` is equal to `value`, otherwise
 *    `0`.
 *
 * @calls
 * None.
 */
static int class_is(int c, uint8_t mask, uint8_t value)
{
    return ((GET_CTYPE_CLASS(c) & mask) == value) ? 1 : 0;
}

/**
 * @funcname `isalnum`
 *
 * @description
 * Tells whether a character is a letter or a digit (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_ALNUM` and `CTB_ALNUM`, and
 *    return its result.
 *
 * @calls
 * - `class_is()`
 */
int isalnum(int c)
{
    return class_is(c, CTB_ALNUM, CTB_ALNUM);
}
/**
 * @funcname `isalpha`
 *
 * @description
 * Tells whether a character is a letter (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_ALNUM | CTB_ALPHA_OR_BLANK`
 *    and `CTB_ALNUM | CTB_ALPHA_OR_BLANK`, and return its result.
 *
 * @calls
 * - `class_is()`
 */
int isalpha(int c)
{
    return class_is(c, CTB_ALNUM | CTB_ALPHA_OR_BLANK,
                    CTB_ALNUM | CTB_ALPHA_OR_BLANK);
}
/**
 * @funcname `isblank`
 *
 * @description
 * Tells whether a character is blank: space or tab (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_ALNUM | CTB_ALPHA_OR_BLANK`
 *    and `CTB_ALPHA_OR_BLANK`, and return its result.
 *
 * @calls
 * - `class_is()`
 */
int isblank(int c)
{
    return class_is(c, CTB_ALNUM | CTB_ALPHA_OR_BLANK, CTB_ALPHA_OR_BLANK);
}
/**
 * @funcname `iscntrl`
 *
 * @description
 * Tells whether a character is a control character (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_ALNUM | CTB_DIGIT_OR_CNTRL`
 *    and `CTB_DIGIT_OR_CNTRL`, and return its result.
 *
 * @calls
 * - `class_is()`
 */
int iscntrl(int c)
{
    return class_is(c, CTB_ALNUM | CTB_DIGIT_OR_CNTRL, CTB_DIGIT_OR_CNTRL);
}
/**
 * @funcname `isdigit`
 *
 * @description
 * Tells whether a character is a decimal digit (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_ALNUM | CTB_DIGIT_OR_CNTRL`
 *    and `CTB_ALNUM | CTB_DIGIT_OR_CNTRL`, and return its result.
 *
 * @calls
 * - `class_is()`
 */
int isdigit(int c)
{
    return class_is(c, CTB_ALNUM | CTB_DIGIT_OR_CNTRL,
                    CTB_ALNUM | CTB_DIGIT_OR_CNTRL);
}
/**
 * @funcname `isgraph`
 *
 * @description
 * Tells whether a character is printable, other than space (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_GRAPH` and `CTB_GRAPH`, and
 *    return its result.
 *
 * @calls
 * - `class_is()`
 */
int isgraph(int c)
{
    return class_is(c, CTB_GRAPH, CTB_GRAPH);
}
/**
 * @funcname `islower`
 *
 * @description
 * Tells whether a character is a lower case letter (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_ALNUM | CTB_LOWER_OR_PUNCT`
 *    and `CTB_ALNUM | CTB_LOWER_OR_PUNCT`, and return its result.
 *
 * @calls
 * - `class_is()`
 */
int islower(int c)
{
    return class_is(c, CTB_ALNUM | CTB_LOWER_OR_PUNCT,
                    CTB_ALNUM | CTB_LOWER_OR_PUNCT);
}
/**
 * @funcname `isprint`
 *
 * @description
 * Tells whether a character is printable, space included (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_PRINT` and `CTB_PRINT`, and
 *    return its result.
 *
 * @calls
 * - `class_is()`
 */
int isprint(int c)
{
    return class_is(c, CTB_PRINT, CTB_PRINT);
}
/**
 * @funcname `ispunct`
 *
 * @description
 * Tells whether a character is a punctuation character (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_ALNUM | CTB_LOWER_OR_PUNCT`
 *    and `CTB_LOWER_OR_PUNCT`, and return its result.
 *
 * @calls
 * - `class_is()`
 */
int ispunct(int c)
{
    return class_is(c, CTB_ALNUM | CTB_LOWER_OR_PUNCT, CTB_LOWER_OR_PUNCT);
}
/**
 * @funcname `isspace`
 *
 * @description
 * Tells whether a character is white space (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_ALNUM | CTB_UPPER_OR_SPACE`
 *    and `CTB_UPPER_OR_SPACE`, and return its result.
 *
 * @calls
 * - `class_is()`
 */
int isspace(int c)
{
    return class_is(c, CTB_ALNUM | CTB_UPPER_OR_SPACE, CTB_UPPER_OR_SPACE);
}
/**
 * @funcname `isupper`
 *
 * @description
 * Tells whether a character is an upper case letter (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_ALNUM | CTB_UPPER_OR_SPACE`
 *    and `CTB_ALNUM | CTB_UPPER_OR_SPACE`, and return its result.
 *
 * @calls
 * - `class_is()`
 */
int isupper(int c)
{
    return class_is(c, CTB_ALNUM | CTB_UPPER_OR_SPACE,
                    CTB_ALNUM | CTB_UPPER_OR_SPACE);
}
/**
 * @funcname `isxdigit`
 *
 * @description
 * Tells whether a character is a hexadecimal digit (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return Non-zero if it is.
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
 * 1. Call `class_is()` with the arguments `c`, `CTB_XDIGIT` and `CTB_XDIGIT`,
 *    and return its result.
 *
 * @calls
 * - `class_is()`
 */
int isxdigit(int c)
{
    return class_is(c, CTB_XDIGIT, CTB_XDIGIT);
}
/**
 * @funcname `tolower`
 *
 * @description
 * Converts a character to lower case (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return The lower case character, or c.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctype_table_lower` | `int *` | The lower case of each character |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `(ctype_table_lower + 1)[c]`.
 *
 * @calls
 * None.
 */
int tolower(int c)
{
    return (ctype_table_lower + 1)[c];
}
/**
 * @funcname `toupper`
 *
 * @description
 * Converts a character to upper case (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `c` | `int` | [in] The character (unsigned char value or EOF). |
 *
 * @return The upper case character, or c.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ctype_table_upper` | `int *` | The upper case of each character |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `(ctype_table_upper + 1)[c]`.
 *
 * @calls
 * None.
 */
int toupper(int c)
{
    return (ctype_table_upper + 1)[c];
}

/* Currently, the only possible locale is "C" (tables from ctype_tables_c.h). */

unsigned char* ctype_table_classification = ctype_table_classification_C;

int* ctype_table_lower = ctype_table_lower_C;
int* ctype_table_upper = ctype_table_upper_C;
