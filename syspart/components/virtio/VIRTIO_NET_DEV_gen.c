/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/virtio/config.yaml).
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
 * The glue code of the component VIRTIO_NET_DEV: the wrappers of its
 * in ports, the calls of its out ports, and its init and activity
 * functions (generated from misc/templates/component.c.tpl).
 */

#include <lib/common.h>
#include "VIRTIO_NET_DEV_gen.h"






/**
 * @funcname `wrapper_send_frame`
 *
 * @description
 * The send operation of the in port portA: calls
 * send_frame() of the component.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg0` | `self_t *` | [in,out] The component. |
 * | `arg1` | `char *` | [in,out] Argument 1 of send. |
 * | `arg2` | `size_t` | [in] Argument 2 of send. |
 * | `arg3` | `size_t` | [in] Argument 3 of send. |
 *
 * @return The result of send_frame().
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
 * 1. Call `send_frame()` with the arguments
 *    `(VIRTIO_NET_DEV*) arg0`,
 *    `arg1`,
 *    `arg2` and
 *    `arg3`, and return its result.
 *
 * @calls
 * - `send_frame()`
 */
    static ret_e wrapper_send_frame(
        self_t *arg0, char * arg1, size_t arg2, size_t arg3)
    {
        return send_frame((VIRTIO_NET_DEV*) arg0, arg1, arg2, arg3);
    }

/**
 * @funcname `wrapper_flush_send`
 *
 * @description
 * The flush operation of the in port portA: calls
 * flush_send() of the component.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg0` | `self_t *` | [in] The component. |
 *
 * @return The result of flush_send().
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
 * 1. Call `flush_send()` with the argument
 *    `(VIRTIO_NET_DEV*) arg0`, and return its result.
 *
 * @calls
 * - `flush_send()`
 */
    static ret_e wrapper_flush_send(
        self_t *arg0)
    {
        return flush_send((VIRTIO_NET_DEV*) arg0);
    }



/**
 * @funcname `VIRTIO_NET_DEV_call_portB_handle`
 *
 * @description
 * Calls the handle operation of the component linked to the out
 * port portB; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `VIRTIO_NET_DEV *` | [in] The component. |
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
 * 1. If `self->out.portB.ops` is NULL:
 *    1. Call `printf()` with the argument
 *       `"WRONG CONFIG: out port portB of component "`
 *       `"VIRTIO_NET_DEV was not initialized\n"`.
 * 2. Call the function `self->out.portB.ops->handle` with the arguments
 *    `self->out.portB.owner`,
 *    `arg1` and
 *    `arg2`, and return its result.
 *
 * @calls
 * - `printf()`
 * - `self->out.portB.ops->handle()` (indirect)
 */
      ret_e VIRTIO_NET_DEV_call_portB_handle(VIRTIO_NET_DEV *self, const char * arg1, size_t arg2)
      {
         if (self->out.portB.ops == NULL) {
             (void) printf("WRONG CONFIG: out port portB of component "
                           "VIRTIO_NET_DEV was not initialized\n");
             //fatal_error?
         }
         return self->out.portB.ops->handle(self->out.portB.owner, arg1, arg2);
      }


/**
 * @funcname `__VIRTIO_NET_DEV_init__`
 *
 * @description
 * Initializes the component (from the glue code at partition start): the
 * operations of its in ports, then its init
 * function virtio_init().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `VIRTIO_NET_DEV *` | [in,out] The component. |
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
 * 1. Set `self->in.portA.ops.send` to `wrapper_send_frame`.
 * 2. Set `self->in.portA.ops.flush` to `wrapper_flush_send`.
 * 3. Call `virtio_init()` with the argument `self`.
 *
 * @calls
 * - `virtio_init()`
 */
void __VIRTIO_NET_DEV_init__(VIRTIO_NET_DEV *self)
{
            self->in.portA.ops.send = wrapper_send_frame;
            self->in.portA.ops.flush = wrapper_flush_send;

        virtio_init(self);
}

/**
 * @funcname `__VIRTIO_NET_DEV_activity__`
 *
 * @description
 * One step of the component's activity (called in the loop of the glue
 * code): its activity function
 * virtio_receive_activity().
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `VIRTIO_NET_DEV *` | [in,out] The component. |
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
 * 1. Call `virtio_receive_activity()` with the argument `self`.
 *
 * @calls
 * - `virtio_receive_activity()`
 */
void __VIRTIO_NET_DEV_activity__(VIRTIO_NET_DEV *self)
{
        virtio_receive_activity(self);
}
