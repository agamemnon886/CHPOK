/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/udp_ip/config.yaml).
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
 * The glue code of the component ROUTER: the wrappers of its
 * in ports, the calls of its out ports, and its init and activity
 * functions (generated from misc/templates/component.c.tpl).
 */

#include <lib/common.h>
#include "ROUTER_gen.h"






/**
 * @funcname `wrapper_receive_packet`
 *
 * @description
 * The udp_message_handle operation of the in port portA: calls
 * receive_packet() of the component.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg0` | `self_t *` | [in] The component. |
 * | `arg1` | `const char *` | [in] Argument 1 of udp_message_handle. |
 * | `arg2` | `size_t` | [in] Argument 2 of udp_message_handle. |
 * | `arg3` | `uint32_t` | [in] Argument 3 of udp_message_handle. |
 * | `arg4` | `uint16_t` | [in] Argument 4 of udp_message_handle. |
 *
 * @return The result of receive_packet().
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
 * 1. Call `receive_packet()` with the arguments
 *    `(ROUTER*) arg0`,
 *    `arg1`,
 *    `arg2`,
 *    `arg3` and
 *    `arg4`, and return its result.
 *
 * @calls
 * - `receive_packet()`
 */
    static ret_e wrapper_receive_packet(
        self_t *arg0, const char * arg1, size_t arg2, uint32_t arg3, uint16_t arg4)
    {
        return receive_packet((ROUTER*) arg0, arg1, arg2, arg3, arg4);
    }



/**
 * @funcname `ROUTER_call_portArray_handle_by_index`
 *
 * @description
 * Calls the handle operation of the component linked to the out
 * port portArray at an index; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `idx` | `int32_t` | [in] The index in the port array. |
 * | `self` | `ROUTER *` | [in] The component. |
 * | `arg1` | `const char *` | [in] Argument 1 of handle. |
 * | `arg2` | `size_t` | [in] Argument 2 of handle. |
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
 * 1. If `self->out.portArray[idx].ops` is NULL:
 *    1. Call `printf()` with the argument
 *       `"WRONG CONFIG: out port portArray of component "`
 *       `"ROUTER was not initialized\n"`.
 * 2. Call the function `self->out.portArray[idx].ops->handle` with the arguments
 *    `self->out.portArray[idx].owner`,
 *    `arg1` and
 *    `arg2`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.portArray[idx].ops->handle()` (indirect)
 */
      ret_e ROUTER_call_portArray_handle_by_index(int32_t idx, ROUTER *self, const char * arg1, size_t arg2)
      {
         if (self->out.portArray[idx].ops == NULL) {
             (void) printf("WRONG CONFIG: out port portArray of component "
                           "ROUTER was not initialized\n");
             //fatal_error?
         }
         return self->out.portArray[idx].ops->handle(self->out.portArray[idx].owner, arg1, arg2);
      }


/**
 * @funcname `__ROUTER_init__`
 *
 * @description
 * Initializes the component (from the glue code at partition start): the
 * operations of its in ports.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ROUTER *` | [out] The component. |
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
 * 1. Set `self->in.portA.ops.udp_message_handle` to `wrapper_receive_packet`.
 *
 * @calls
 * None.
 */
void __ROUTER_init__(ROUTER *self)
{
            self->in.portA.ops.udp_message_handle = wrapper_receive_packet;

}

/**
 * @funcname `__ROUTER_activity__`
 *
 * @description
 * One step of the component's activity (called in the loop of the glue
 * code): the component has none.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ROUTER *` | [in] The component. |
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
void __ROUTER_activity__(ROUTER *self)
{
        (void) self; // the component has no activity
}
