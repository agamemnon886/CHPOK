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
 */

#include <ctype.h>
#include <stdint.h>
#include "ctype_internal.h"
#include "ctype_tables_c.h"

/*
 * MISRA C:2025 Rule 18.4 deviation (this file): the tables have an entry for
 * EOF (-1) first, so they are indexed from their start plus one.
 */

#define GET_CTYPE_CLASS(c) ((ctype_table_classification + 1)[c])

/* 1 if the bits 'mask' of the character's class are equal to 'value', 0
 * otherwise. */
static int class_is(int c, uint8_t mask, uint8_t value)
{
    return ((GET_CTYPE_CLASS(c) & mask) == value) ? 1 : 0;
}

int isalnum(int c)
{
    return class_is(c, CTB_ALNUM, CTB_ALNUM);
}
int isalpha(int c)
{
    return class_is(c, CTB_ALNUM | CTB_ALPHA_OR_BLANK,
                    CTB_ALNUM | CTB_ALPHA_OR_BLANK);
}
int isblank(int c)
{
    return class_is(c, CTB_ALNUM | CTB_ALPHA_OR_BLANK, CTB_ALPHA_OR_BLANK);
}
int iscntrl(int c)
{
    return class_is(c, CTB_ALNUM | CTB_DIGIT_OR_CNTRL, CTB_DIGIT_OR_CNTRL);
}
int isdigit(int c)
{
    return class_is(c, CTB_ALNUM | CTB_DIGIT_OR_CNTRL,
                    CTB_ALNUM | CTB_DIGIT_OR_CNTRL);
}
int isgraph(int c)
{
    return class_is(c, CTB_GRAPH, CTB_GRAPH);
}
int islower(int c)
{
    return class_is(c, CTB_ALNUM | CTB_LOWER_OR_PUNCT,
                    CTB_ALNUM | CTB_LOWER_OR_PUNCT);
}
int isprint(int c)
{
    return class_is(c, CTB_PRINT, CTB_PRINT);
}
int ispunct(int c)
{
    return class_is(c, CTB_ALNUM | CTB_LOWER_OR_PUNCT, CTB_LOWER_OR_PUNCT);
}
int isspace(int c)
{
    return class_is(c, CTB_ALNUM | CTB_UPPER_OR_SPACE, CTB_UPPER_OR_SPACE);
}
int isupper(int c)
{
    return class_is(c, CTB_ALNUM | CTB_UPPER_OR_SPACE,
                    CTB_ALNUM | CTB_UPPER_OR_SPACE);
}
int isxdigit(int c)
{
    return class_is(c, CTB_XDIGIT, CTB_XDIGIT);
}
int tolower(int c)
{
    return (ctype_table_lower + 1)[c];
}
int toupper(int c)
{
    return (ctype_table_upper + 1)[c];
}

/* Currently, the only possible locale is "C" (tables from ctype_tables_c.h). */

unsigned char* ctype_table_classification = ctype_table_classification_C;

int* ctype_table_lower = ctype_table_lower_C;
int* ctype_table_upper = ctype_table_upper_C;
