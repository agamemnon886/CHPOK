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
 * guard isn't a reserved identifier; deviations are documented where a
 * guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 */

/*
 * MISRA C:2025 Rule 5.10 and Rule 20.15 deviation (this header): this is
 * the <stdlib.h> header of libpok's own C library (partitions are built
 * freestanding, without the system one), so it declares the standard
 * names and defines RAND_MAX.
 */
#ifndef LIBJET_STDLIB_H
#define LIBJET_STDLIB_H

#include <stdint.h>
#include <stddef.h>

/**
 * @macroname `RAND_MAX`
 *
 * @description
 * The largest value that rand() returns.
 */
#define RAND_MAX 32767

/** \cond */
void abort(void);

int rand (void);
void srand(unsigned int seed);
/** \endcond */

#endif /* LIBJET_STDLIB_H*/
