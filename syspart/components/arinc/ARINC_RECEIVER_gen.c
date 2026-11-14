/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/arinc/config.yaml).
 */
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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types in
 * the generated code are replaced with the fixed-width types from
 * <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: discarded
 * printf() results are cast to void; the static wrappers aren't named with
 * reserved identifiers.
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: the activity function of a
 * component without an activity discards its unused parameter.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions (the format of the function documentation, see
 * README.md).
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: in @design, each string
 * literal of the "WRONG CONFIG" message is a code span of its own; the
 * parameters' directions come from the interface (args_dir).
 */

/**
 * @file
 * The glue code of the component ARINC_RECEIVER: the wrappers of its
 * in ports, the calls of its out ports, and its init and activity
 * functions (generated from misc/templates/component.c.tpl).
 */

#include <lib/common.h>
#include "ARINC_RECEIVER_gen.h"






/**
 * @funcname `wrapper_arinc_receive_message`
 *
 * @description
 * The handle operation of the in port portA: calls
 * arinc_receive_message() of the component.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg0` | `self_t *` | [in] The component. |
 * | `arg1` | `const char *` | [in] Argument 1 of handle. |
 * | `arg2` | `size_t` | [in] Argument 2 of handle. |
 *
 * @return The result of arinc_receive_message().
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
 * 1. Call `arinc_receive_message()` with the arguments
 *    `(ARINC_RECEIVER*) arg0`,
 *    `arg1` and
 *    `arg2`, and return its result.
 *
 * @calls
 * - `arinc_receive_message()`
 */
    static ret_e wrapper_arinc_receive_message(
        self_t *arg0, const char * arg1, size_t arg2)
    {
        return arinc_receive_message((ARINC_RECEIVER*) arg0, arg1, arg2);
    }





/**
 * @funcname `__ARINC_RECEIVER_init__`
 *
 * @description
 * Initializes the component (from the glue code at partition start): the
 * operations of its in ports, then its init
 * function arinc_receiver_init().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_RECEIVER *` | [in,out] The component. |
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
 * 1. Set `self->in.portA.ops.handle` to `wrapper_arinc_receive_message`.
 * 2. Call `arinc_receiver_init()` with the argument `self`.
 *
 * @calls
 * - `arinc_receiver_init()`
 */
void __ARINC_RECEIVER_init__(ARINC_RECEIVER *self)
{
            self->in.portA.ops.handle = wrapper_arinc_receive_message;

        arinc_receiver_init(self);
}

/**
 * @funcname `__ARINC_RECEIVER_activity__`
 *
 * @description
 * One step of the component's activity (called in the loop of the glue
 * code): the component has none.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_RECEIVER *` | [in] The component. |
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
 * 1. Mark `self` as not used.
 *
 * @calls
 * None.
 */
void __ARINC_RECEIVER_activity__(ARINC_RECEIVER *self)
{
        (void) self; // the component has no activity
}
