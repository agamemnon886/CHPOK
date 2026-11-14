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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: an include
 * guard is added; U suffixes for unsigned constants; the class bits are
 * unsigned.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macro GET_CTYPE_CLASS is moved
 * here from libpok/libc/ctype/ctype_internal.c.
 */


#ifndef LIBJET_CTYPE_INTERNAL_H
#define LIBJET_CTYPE_INTERNAL_H

/* Way for storing information about characters classification. */

/**
 * @macroname `CTYPE_TABLE_SIZE`
 *
 * @description
 * According to C99, only ints transformed into 'unsigned char' or EOF should be
 * classified.
 *
 * 256(unsigned char) + 1(EOF) = 257(total).
 */
#define CTYPE_TABLE_SIZE 257

/* 
 * In total, there are 12 character classes:
 * 
 * 1)  upper
 * 2)  lower
 * 3)  alpha
 * 4)  digit
 * 5)  xdigit
 * 6)  space
 * 7)  print
 * 8)  graph
 * 9)  blank
 * 10) cntrl
 * 11) punct
 * 12) alnum
 * 
 * Using bit-per class doesn't fit into byte(8 bit) of information about
 * single character.
 * 
 * We use "packaging" for reduce number of bits:
 * 
 * 0. Assumption (does not directly follow from C99):
 *   "digit" cannot be "cntrl".
 * 
 * 1. According to C99, "alnum" can be neither "space" nor "punct".
 * 
 * 2. According to C99, "blank" is subclass of "space", so "alnum" cannot
 * be "blank".
 * 
 * 3. According to C99, "alpha" cannot be "cntrl".
 * 
 * 4. According to C99, "alnum" is combination of "alpha" and "digit". As
 * "alpha" cannot be "cntrl" (p.3), "digit" cannot be "cntrl" (p. 0), then
 * "alnum" cannot be "cntrl".
 * 
 * 5. According to C99, "alnum" includes "upper", "lower", "alpha", "digit".
 *
 * So 9 classes noted above may be encoded into 5 bits:
 *  - bit_alnum
 *  - bit_upper_or_space (depended from bit_alnum)
 *  - bit_lower_or_punct (depended from bit_alnum)
 *  - bit_alpha_or_blank (depended from bit_alnum)
 *  - bit_digit_or_control (depended from bit_alnum)
 * 
 * Using some other assumptions about digit, xdigit and alpha,
 * number of bits may be reduced futher.
 */

/**
 * @name Character class bits
 * The bits of the classification of a character (ctype_table_classification):
 * the 12 classes of C99 packed into 8 bits, as explained in the comment above.
 * @{
 */

/**
 * @macroname `CTB_ALNUM`
 *
 * @description
 * The class bit "alnum".
 */
#define CTB_ALNUM 1U

/**
 * @macroname `CTB_UPPER_OR_SPACE`
 *
 * @description
 * The class bit "upper" with CTB_ALNUM, "space" without it.
 */
#define CTB_UPPER_OR_SPACE 2U

/**
 * @macroname `CTB_LOWER_OR_PUNCT`
 *
 * @description
 * The class bit "lower" with CTB_ALNUM, "punct" without it.
 */
#define CTB_LOWER_OR_PUNCT 4U

/**
 * @macroname `CTB_ALPHA_OR_BLANK`
 *
 * @description
 * The class bit "alpha" with CTB_ALNUM, "blank" without it.
 */
#define CTB_ALPHA_OR_BLANK 8U

/**
 * @macroname `CTB_DIGIT_OR_CNTRL`
 *
 * @description
 * The class bit "digit" with CTB_ALNUM, "cntrl" without it.
 */
#define CTB_DIGIT_OR_CNTRL 16U

/**
 * @macroname `CTB_PRINT`
 *
 * @description
 * The class bit "print".
 */
#define CTB_PRINT 32U

/**
 * @macroname `CTB_GRAPH`
 *
 * @description
 * The class bit "graph".
 */
#define CTB_GRAPH 64U

/**
 * @macroname `CTB_XDIGIT`
 *
 * @description
 * The class bit "xdigit".
 */
#define CTB_XDIGIT 128U

/** @} */


/* 
 * Pointer to the array of size CTYPE_TABLE_SIZE, which contains
 * classifications bits for the characters.
 */
extern unsigned char* ctype_table_classification;

/* 
 * Pointer to the array of size CTYPE_TABLE_SIZE, which contains results
 * of "tolower" conversion for the characters.
 */
extern int* ctype_table_lower;

/* 
 * Pointer to the array of size CTYPE_TABLE_SIZE, which contains results
 * of "toupper" conversion for the characters.
 */
extern int* ctype_table_upper;

/**
 * @macroname `GET_CTYPE_CLASS`
 *
 * @description
 * The class of a character (its bits in ctype_table_classification). The tables
 * have an entry for EOF (-1) first, so they are indexed from their start plus
 * one.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `c` | The character, or EOF. |
 */
#define GET_CTYPE_CLASS(c) ((ctype_table_classification + 1)[c])

#endif /* LIBJET_CTYPE_INTERNAL_H */
