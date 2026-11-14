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
 * The glue code of the component ARINC_SENDER: the wrappers of its
 * in ports, the calls of its out ports, and its init and activity
 * functions (generated from misc/templates/component.c.tpl).
 */

#include <lib/common.h>
#include "ARINC_SENDER_gen.h"








/**
 * @funcname `ARINC_SENDER_call_portA_send`
 *
 * @description
 * Calls the send operation of the component linked to the out
 * port portA; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_SENDER *` | [in] The component. |
 * | `arg1` | `char *` | [in,out] Argument 1 of send. |
 * | `arg2` | `size_t` | [in] Argument 2 of send. |
 * | `arg3` | `size_t` | [in] Argument 3 of send. |
 *
 * @return The result of the operation.
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
 * 1. If `self->out.portA.ops` is NULL:
 *    1. Call `printf()` with the argument
 *       `"WRONG CONFIG: out port portA of component "`
 *       `"ARINC_SENDER was not initialized\n"`.
 * 2. Call the function `self->out.portA.ops->send` with the arguments
 *    `self->out.portA.owner`,
 *    `arg1`,
 *    `arg2` and
 *    `arg3`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.portA.ops->send()` (indirect)
 */
      ret_e ARINC_SENDER_call_portA_send(ARINC_SENDER *self, char * arg1, size_t arg2, size_t arg3)
      {
         if (self->out.portA.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portA of component "
                           "ARINC_SENDER was not initialized\n");
             //fatal_error?
         }
         return self->out.portA.ops->send(self->out.portA.owner, arg1, arg2, arg3);
      }
/**
 * @funcname `ARINC_SENDER_call_portA_flush`
 *
 * @description
 * Calls the flush operation of the component linked to the out
 * port portA; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_SENDER *` | [in] The component. |
 *
 * @return The result of the operation.
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
 * 1. If `self->out.portA.ops` is NULL:
 *    1. Call `printf()` with the argument
 *       `"WRONG CONFIG: out port portA of component "`
 *       `"ARINC_SENDER was not initialized\n"`.
 * 2. Call the function `self->out.portA.ops->flush` with the argument
 *    `self->out.portA.owner`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.portA.ops->flush()` (indirect)
 */
      ret_e ARINC_SENDER_call_portA_flush(ARINC_SENDER *self)
      {
         if (self->out.portA.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portA of component "
                           "ARINC_SENDER was not initialized\n");
             //fatal_error?
         }
         return self->out.portA.ops->flush(self->out.portA.owner);
      }


/**
 * @funcname `__ARINC_SENDER_init__`
 *
 * @description
 * Initializes the component (from the glue code at partition start): the
 * operations of its in ports, then its init
 * function arinc_sender_init().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_SENDER *` | [in,out] The component. |
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
 * 1. Call `arinc_sender_init()` with the argument `self`.
 *
 * @calls
 * - `arinc_sender_init()`
 */
void __ARINC_SENDER_init__(ARINC_SENDER *self)
{

        arinc_sender_init(self);
}

/**
 * @funcname `__ARINC_SENDER_activity__`
 *
 * @description
 * One step of the component's activity (called in the loop of the glue
 * code): its activity function
 * arinc_sender_activity().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_SENDER *` | [in,out] The component. |
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
 * 1. Call `arinc_sender_activity()` with the argument `self`.
 *
 * @calls
 * - `arinc_sender_activity()`
 */
void __ARINC_SENDER_activity__(ARINC_SENDER *self)
{
        arinc_sender_activity(self);
}
