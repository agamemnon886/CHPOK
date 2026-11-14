/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/include/interfaces/port_example.yaml).
 */
/*
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef INTERFACES_PORT_EXAMPLE_H
#define INTERFACES_PORT_EXAMPLE_H

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
 * @typename `port_example_send_fn_t`
 *
 * @description
 * The function send of the port_example interface.
 * Sends data: arg0 is the component, arg1 the data and arg2 its size.
 */
typedef void (*port_example_send_fn_t)(
    self_t * arg0,
    void * arg1,
    size_t arg2);

/**
 * @typename `port_example_flush_fn_t`
 *
 * @description
 * The function flush of the port_example interface.
 * Sends what the component (arg0) has queued.
 */
typedef void (*port_example_flush_fn_t)(
    self_t * arg0);

/**
 * @typename `port_example`
 *
 * @description
 * The operations of a port of the port_example interface: those of
 * the component that implements it.
 * An example of an interface (no component uses it): it sends data.
 */
typedef struct {
    port_example_send_fn_t send;
    /**< Sends data: arg0 is the component, arg1 the data and arg2 its
     * size.
     */
    port_example_flush_fn_t flush;
    /**< Sends what the component (arg0) has queued.
     */
} port_example;


#endif

