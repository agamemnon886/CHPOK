/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/arp/config.yaml).
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
 * The glue code of the component ARP_ANSWERER: the wrappers of its
 * in ports, the calls of its out ports, and its init and activity
 * functions (generated from misc/templates/component.c.tpl).
 */

#include <lib/common.h>
#include "ARP_ANSWERER_gen.h"






/**
 * @funcname `wrapper_arp_receive`
 *
 * @description
 * The handle operation of the in port portA: calls
 * arp_receive() of the component.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg0` | `self_t *` | [in] The component. |
 * | `arg1` | `const char *` | [in] Argument 1 of handle. |
 * | `arg2` | `size_t` | [in] Argument 2 of handle. |
 *
 * @return The result of arp_receive().
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
 * 1. Call `arp_receive()` with the arguments
 *    `(ARP_ANSWERER*) arg0`,
 *    `arg1` and
 *    `arg2`, and return its result.
 *
 * @calls
 * - `arp_receive()`
 */
    static ret_e wrapper_arp_receive(
        self_t *arg0, const char * arg1, size_t arg2)
    {
        return arp_receive((ARP_ANSWERER*) arg0, arg1, arg2);
    }



/**
 * @funcname `ARP_ANSWERER_call_portB_mac_send`
 *
 * @description
 * Calls the mac_send operation of the component linked to the out
 * port portB; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARP_ANSWERER *` | [in] The component. |
 * | `arg1` | `char *` | [in,out] Argument 1 of mac_send. |
 * | `arg2` | `size_t` | [in] Argument 2 of mac_send. |
 * | `arg3` | `size_t` | [in] Argument 3 of mac_send. |
 * | `arg4` | `uint8_t *` | [in] Argument 4 of mac_send. |
 * | `arg5` | `ethertype_e` | [in] Argument 5 of mac_send. |
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
 *       `"ARP_ANSWERER was not initialized\n"`.
 * 2. Call the function `self->out.portB.ops->mac_send` with the arguments
 *    `self->out.portB.owner`,
 *    `arg1`,
 *    `arg2`,
 *    `arg3`,
 *    `arg4` and
 *    `arg5`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.portB.ops->mac_send()` (indirect)
 */
      ret_e ARP_ANSWERER_call_portB_mac_send(ARP_ANSWERER *self, char * arg1, size_t arg2, size_t arg3, uint8_t * arg4, ethertype_e arg5)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "ARP_ANSWERER was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->mac_send(self->out.portB.owner, arg1, arg2, arg3, arg4, arg5);
      }
/**
 * @funcname `ARP_ANSWERER_call_portB_flush`
 *
 * @description
 * Calls the flush operation of the component linked to the out
 * port portB; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARP_ANSWERER *` | [in] The component. |
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
 *       `"ARP_ANSWERER was not initialized\n"`.
 * 2. Call the function `self->out.portB.ops->flush` with the argument
 *    `self->out.portB.owner`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.portB.ops->flush()` (indirect)
 */
      ret_e ARP_ANSWERER_call_portB_flush(ARP_ANSWERER *self)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "ARP_ANSWERER was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->flush(self->out.portB.owner);
      }


/**
 * @funcname `__ARP_ANSWERER_init__`
 *
 * @description
 * Initializes the component (from the glue code at partition start): the
 * operations of its in ports.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARP_ANSWERER *` | [out] The component. |
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
 * 1. Set `self->in.portA.ops.handle` to `wrapper_arp_receive`.
 *
 * @calls
 * None.
 */
void __ARP_ANSWERER_init__(ARP_ANSWERER *self)
{
            self->in.portA.ops.handle = wrapper_arp_receive;

}

/**
 * @funcname `__ARP_ANSWERER_activity__`
 *
 * @description
 * One step of the component's activity (called in the loop of the glue
 * code): the component has none.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARP_ANSWERER *` | [in] The component. |
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
void __ARP_ANSWERER_activity__(ARP_ANSWERER *self)
{
        (void) self; // the component has no activity
}
