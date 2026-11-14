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
 * The glue code of the component MAC_SENDER: the wrappers of its
 * in ports, the calls of its out ports, and its init and activity
 * functions (generated from misc/templates/component.c.tpl).
 */

#include <lib/common.h>
#include "MAC_SENDER_gen.h"






/**
 * @funcname `wrapper_mac_sender_send`
 *
 * @description
 * The mac_send operation of the in port portA: calls
 * mac_sender_send() of the component.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg0` | `self_t *` | [in] The component. |
 * | `arg1` | `char *` | [in,out] Argument 1 of mac_send. |
 * | `arg2` | `size_t` | [in] Argument 2 of mac_send. |
 * | `arg3` | `size_t` | [in] Argument 3 of mac_send. |
 * | `arg4` | `uint8_t *` | [in] Argument 4 of mac_send. |
 * | `arg5` | `ethertype_e` | [in] Argument 5 of mac_send. |
 *
 * @return The result of mac_sender_send().
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
 * 1. Call `mac_sender_send()` with the arguments
 *    `(MAC_SENDER*) arg0`,
 *    `arg1`,
 *    `arg2`,
 *    `arg3`,
 *    `arg4` and
 *    `arg5`, and return its result.
 *
 * @calls
 * - `mac_sender_send()`
 */
    static ret_e wrapper_mac_sender_send(
        self_t *arg0, char * arg1, size_t arg2, size_t arg3, uint8_t * arg4, ethertype_e arg5)
    {
        return mac_sender_send((MAC_SENDER*) arg0, arg1, arg2, arg3, arg4, arg5);
    }

/**
 * @funcname `wrapper_mac_flush`
 *
 * @description
 * The flush operation of the in port portA: calls
 * mac_flush() of the component.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg0` | `self_t *` | [in] The component. |
 *
 * @return The result of mac_flush().
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
 * 1. Call `mac_flush()` with the argument
 *    `(MAC_SENDER*) arg0`, and return its result.
 *
 * @calls
 * - `mac_flush()`
 */
    static ret_e wrapper_mac_flush(
        self_t *arg0)
    {
        return mac_flush((MAC_SENDER*) arg0);
    }



/**
 * @funcname `MAC_SENDER_call_portB_send`
 *
 * @description
 * Calls the send operation of the component linked to the out
 * port portB; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_SENDER *` | [in] The component. |
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
 * 1. If `self->out.portB.ops` is NULL:
 *    1. Call `printf()` with the argument
 *       `"WRONG CONFIG: out port portB of component "`
 *       `"MAC_SENDER was not initialized\n"`.
 * 2. Call the function `self->out.portB.ops->send` with the arguments
 *    `self->out.portB.owner`,
 *    `arg1`,
 *    `arg2` and
 *    `arg3`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.portB.ops->send()` (indirect)
 */
      ret_e MAC_SENDER_call_portB_send(MAC_SENDER *self, char * arg1, size_t arg2, size_t arg3)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "MAC_SENDER was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->send(self->out.portB.owner, arg1, arg2, arg3);
      }
/**
 * @funcname `MAC_SENDER_call_portB_flush`
 *
 * @description
 * Calls the flush operation of the component linked to the out
 * port portB; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_SENDER *` | [in] The component. |
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
 * 1. If `self->out.portB.ops` is NULL:
 *    1. Call `printf()` with the argument
 *       `"WRONG CONFIG: out port portB of component "`
 *       `"MAC_SENDER was not initialized\n"`.
 * 2. Call the function `self->out.portB.ops->flush` with the argument
 *    `self->out.portB.owner`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.portB.ops->flush()` (indirect)
 */
      ret_e MAC_SENDER_call_portB_flush(MAC_SENDER *self)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "MAC_SENDER was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->flush(self->out.portB.owner);
      }


/**
 * @funcname `__MAC_SENDER_init__`
 *
 * @description
 * Initializes the component (from the glue code at partition start): the
 * operations of its in ports.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_SENDER *` | [out] The component. |
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
 * 1. Set `self->in.portA.ops.mac_send` to `wrapper_mac_sender_send`.
 * 2. Set `self->in.portA.ops.flush` to `wrapper_mac_flush`.
 *
 * @calls
 * None.
 */
void __MAC_SENDER_init__(MAC_SENDER *self)
{
            self->in.portA.ops.mac_send = wrapper_mac_sender_send;
            self->in.portA.ops.flush = wrapper_mac_flush;

}

/**
 * @funcname `__MAC_SENDER_activity__`
 *
 * @description
 * One step of the component's activity (called in the loop of the glue
 * code): the component has none.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `MAC_SENDER *` | [in] The component. |
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
void __MAC_SENDER_activity__(MAC_SENDER *self)
{
        (void) self; // the component has no activity
}
