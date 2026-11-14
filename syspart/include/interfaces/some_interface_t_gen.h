/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/include/interfaces/tick.yaml).
 */
/*
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef INTERFACES_SOME_INTERFACE_T_H
#define INTERFACES_SOME_INTERFACE_T_H

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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; the parameters of the operations are
 * named.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen comments for the file,
 * its type and the type's members (the descriptions are in the interface's
 * configuration: description).
 */


#include <lib/common.h>

/**
 * @typename `some_interface_t_tick_fn_t`
 *
 * @description
 * The function tick of the some_interface_t interface.
 * Tells the component (arg0) that a period has passed.
 */
typedef void (*some_interface_t_tick_fn_t)(
    self_t * arg0);

/**
 * @typename `some_interface_t`
 *
 * @description
 * The operations of a port of the some_interface_t interface: those of
 * the component that implements it.
 * An example of an interface (no component uses it): a periodic tick.
 */
typedef struct {
    some_interface_t_tick_fn_t tick;
    /**< Tells the component (arg0) that a period has passed.
     */
} some_interface_t;


#endif

