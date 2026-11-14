/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/mac/config.yaml).
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
 * The glue code of the component MAC_RECEIVER: the wrappers of its
 * in ports, the calls of its out ports, and its init and activity
 * functions (generated from misc/templates/component.c.tpl).
 */

#include <lib/common.h>
#include "MAC_RECEIVER_gen.h"






/**
 * @funcname `wrapper_mac_receive`
 *
 * @description
 * The handle operation of the in port portA: calls
 * mac_receive() of the component.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg0` | `self_t *` | [in] The component. |
 * | `arg1` | `const char *` | [in] Argument 1 of handle. |
 * | `arg2` | `size_t` | [in] Argument 2 of handle. |
 *
 * @return The result of mac_receive().
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
 * 1. Call `mac_receive()` with the arguments
 *    `(MAC_RECEIVER*) arg0`,
 *    `arg1` and
 *    `arg2`, and return its result.
 *
 * @calls
 * - `mac_receive()`
 */
    static ret_e wrapper_mac_receive(
        self_t *arg0, const char * arg1, size_t arg2)
    {
        return mac_receive((MAC_RECEIVER*) arg0, arg1, arg2);
    }



/**
 * @funcname `MAC_RECEIVER_call_port_UDP_handle`
 *
 * @description
 * Calls the handle operation of the component linked to the out
 * port port_UDP; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_RECEIVER *` | [in] The component. |
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
 * 1. If `self->out.port_UDP.ops` is NULL:
 *    1. Call `printf()` with the argument
 *       `"WRONG CONFIG: out port port_UDP of component "`
 *       `"MAC_RECEIVER was not initialized\n"`.
 * 2. Call the function `self->out.port_UDP.ops->handle` with the arguments
 *    `self->out.port_UDP.owner`,
 *    `arg1` and
 *    `arg2`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.port_UDP.ops->handle()` (indirect)
 */
      ret_e MAC_RECEIVER_call_port_UDP_handle(MAC_RECEIVER *self, const char * arg1, size_t arg2)
      {
         if (self->out.port_UDP.ops == NULL) {
             (void) printf("WRONG CONFIG: out port port_UDP of component "
                           "MAC_RECEIVER was not initialized\n");
             //fatal_error?
         }
         return self->out.port_UDP.ops->handle(self->out.port_UDP.owner, arg1, arg2);
      }
/**
 * @funcname `MAC_RECEIVER_call_port_ARP_handle`
 *
 * @description
 * Calls the handle operation of the component linked to the out
 * port port_ARP; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_RECEIVER *` | [in] The component. |
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
 * 1. If `self->out.port_ARP.ops` is NULL:
 *    1. Call `printf()` with the argument
 *       `"WRONG CONFIG: out port port_ARP of component "`
 *       `"MAC_RECEIVER was not initialized\n"`.
 * 2. Call the function `self->out.port_ARP.ops->handle` with the arguments
 *    `self->out.port_ARP.owner`,
 *    `arg1` and
 *    `arg2`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.port_ARP.ops->handle()` (indirect)
 */
      ret_e MAC_RECEIVER_call_port_ARP_handle(MAC_RECEIVER *self, const char * arg1, size_t arg2)
      {
         if (self->out.port_ARP.ops == NULL) {
             (void) printf("WRONG CONFIG: out port port_ARP of component "
                           "MAC_RECEIVER was not initialized\n");
             //fatal_error?
         }
         return self->out.port_ARP.ops->handle(self->out.port_ARP.owner, arg1, arg2);
      }


/**
 * @funcname `__MAC_RECEIVER_init__`
 *
 * @description
 * Initializes the component (from the glue code at partition start): the
 * operations of its in ports.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_RECEIVER *` | [out] The component. |
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
 * 1. Set `self->in.portA.ops.handle` to `wrapper_mac_receive`.
 *
 * @calls
 * None.
 */
void __MAC_RECEIVER_init__(MAC_RECEIVER *self)
{
            self->in.portA.ops.handle = wrapper_mac_receive;

}

/**
 * @funcname `__MAC_RECEIVER_activity__`
 *
 * @description
 * One step of the component's activity (called in the loop of the glue
 * code): the component has none.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_RECEIVER *` | [in] The component. |
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
void __MAC_RECEIVER_activity__(MAC_RECEIVER *self)
{
        (void) self; // the component has no activity
}
