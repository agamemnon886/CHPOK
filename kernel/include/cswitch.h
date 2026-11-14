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
 * its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the descriptions that start with a
 * function name write it in lower case, as in the code.
 */

#ifndef JET_CSWITCH_H
#define JET_CSWITCH_H

#include <asp/cswitch.h>

/**
 * @name Context switch functions
 * The generic names of the architecture's context switch functions.
 * @{
 */

/**
 * @macroname `jet_context_init`
 *
 * @description
 * ja_context_init(): see asp/cswitch.h.
 */
#define jet_context_init ja_context_init

/**
 * @macroname `jet_context_switch`
 *
 * @description
 * ja_context_switch(): see asp/cswitch.h.
 */
#define jet_context_switch ja_context_switch

/**
 * @macroname `jet_context_jump`
 *
 * @description
 * ja_context_jump(): see asp/cswitch.h.
 */
#define jet_context_jump ja_context_jump

/**
 * @macroname `jet_context_restart`
 *
 * @description
 * ja_context_restart(): see asp/cswitch.h.
 */
#define jet_context_restart ja_context_restart

/**
 * @macroname `jet_context_restart_and_save`
 *
 * @description
 * ja_context_restart_and_save(): see asp/cswitch.h.
 */
#define jet_context_restart_and_save ja_context_restart_and_save

/** @} */


#endif /* JET_CSWITCH_H */
