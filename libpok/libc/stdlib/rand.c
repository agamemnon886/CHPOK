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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: U suffixes for
 * unsigned constants.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

/**
 * @file
 * rand() and srand() of libpok (the example implementation of C99).
 */

#include <stdlib.h>

// Implementation is taken from the example in C99.
static unsigned long int next = 1;
/**
 * @funcname `rand`
 *
 * @description
 * Returns the next pseudo-random number (C standard; RAND_MAX is 32767).
 *
 * @return The number, 0 to 32767.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `next` | `unsigned long` | The state of the pseudo-random generator |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `next` to `(next * 1103515245U) + 12345U`.
 * 2. Return `(unsigned int)(next/65536U) % 32768U`.
 *
 * @calls
 * None.
 */
int rand(void) // RAND_MAX assumed to be 32767
{
    next = (next * 1103515245U) + 12345U;
    return (unsigned int)(next/65536U) % 32768U;
}

/**
 * @funcname `srand`
 *
 * @description
 * Sets the seed of the pseudo-random numbers (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `seed` | `unsigned int` | [in] The seed. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `next` | `unsigned long` | The state of the pseudo-random generator |
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `next` to `seed`.
 *
 * @calls
 * None.
 */
void srand(unsigned int seed)
{
    next = seed;
}
