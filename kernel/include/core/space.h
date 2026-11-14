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
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for the file and
 * its macros; the unused macros pok_create_space and pok_space_get_current are
 * removed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the descriptions that start with a
 * function name write it in lower case, as in the code.
 */

#ifndef JET_SPACE_H
#define JET_SPACE_H

#include <asp/space.h>

/**
 * @name Space functions
 * The generic names of the architecture's space functions.
 * @{
 */

/**
 * @macroname `pok_space_switch`
 *
 * @description
 * ja_space_switch(): switches to a space.
 */
#define pok_space_switch ja_space_switch

/**
 * @macroname `jet_user_space_jump`
 *
 * @description
 * ja_user_space_jump(): enters user space.
 */
#define jet_user_space_jump ja_user_space_jump

/** @} */


#endif /* JET_SPACE_H */
