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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; discarded results are cast to void;
 * essentially Boolean conditions; operands of mixed precedence are
 * parenthesized.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ret_e (was ret_t).
 */

/**
 * @file
 * The ARINC_SENDER component: messages of a user partition, received
 * through an ARINC 653 queuing or sampling port, are sent to the network.
 */
#include <arinc653/queueing.h>
#include <arinc653/sampling.h>
#include <stdio.h>

#include <port_info.h>

#include <smalloc.h>

#include "ARINC_SENDER_gen.h"

/**
 * @funcname `receive_msg_queuing`
 *
 * @description
 * Receives a message of the user partition from the queuing port (no wait),
 * after the space reserved for the headers in the port buffer.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_SENDER *` | [in] The component. |
 *
 * @return 0 if a message was received, -1 otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dst_place` | `sys_port_data_t *` | The port buffer |
 * | `ret` | `RETURN_CODE_TYPE` | The result of the ARINC 653 service |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `dst_place` to `self->state.port_buffer`.
 * 2. Call `RECEIVE_QUEUING_MESSAGE()` with the arguments `self->state.port_id`,
 *    `0`, `(MESSAGE_ADDR_TYPE ) (dst_place->data + self->state.overhead)`, the
 *    address of `dst_place->message_size` and the address of `ret`.
 * 3. If `ret` is not equal to `NO_ERROR`:
 *    1. If `ret` is not equal to `NOT_AVAILABLE`:
 *       1. Call `printf()` with the arguments
 *          `"ARINC_SENDER: %%s port error: %%d\n"`,
 *          `self->state.port_name` and `(int) ret`.
 *    2. Return `-1`.
 * 4. Return `0`.
 *
 * @calls
 * - `RECEIVE_QUEUING_MESSAGE()`
 * - `printf()`
 */
static int32_t receive_msg_queuing(ARINC_SENDER *self)
{
    sys_port_data_t *dst_place = self->state.port_buffer;

    RETURN_CODE_TYPE ret;
    RECEIVE_QUEUING_MESSAGE(
            self->state.port_id,
            0,
            (MESSAGE_ADDR_TYPE ) (dst_place->data + self->state.overhead),
            &dst_place->message_size,
            &ret
            );

    if (ret != NO_ERROR) {
        if (ret != NOT_AVAILABLE) {
            (void) printf(
                "ARINC_SENDER: %s port error: %d\n",
                self->state.port_name, (int) ret);
        }
        return -1;
    }
    return 0;
}

/**
 * @funcname `receive_msg_samping`
 *
 * @description
 * Reads a new message of the user partition from the sampling port, after the
 * space reserved for the headers in the port buffer.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_SENDER *` | [in] The component. |
 *
 * @return 0 if a new message was read, -1 otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dst_place` | `sys_port_data_t *` | The port buffer |
 * | `ret` | `RETURN_CODE_TYPE` | The result of the ARINC 653 service |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `dst_place` to `self->state.port_buffer`.
 * 2. If `SYS_SAMPLING_PORT_CHECK_IS_NEW_DATA(self->state.port_id)` is false:
 *    1. Return `-1`.
 * 3. Call `READ_SAMPLING_MESSAGE()` with the arguments `self->state.port_id`,
 *    `(MESSAGE_ADDR_TYPE ) (dst_place->data + self->state.overhead)`, the
 *    address of `dst_place->message_size`, `NULL` and the address of `ret`.
 * 4. If `ret` is not equal to `NO_ERROR`:
 *    1. If `ret` is not equal to `NOT_AVAILABLE`:
 *       1. Call `printf()` with the arguments
 *          `"ARINC_SENDER: %%s port error: %%d\n"`,
 *          `self->state.port_name` and `(int) ret`.
 *    2. Return `-1`.
 * 5. Return `0`.
 *
 * @calls
 * - `SYS_SAMPLING_PORT_CHECK_IS_NEW_DATA()`
 * - `READ_SAMPLING_MESSAGE()`
 * - `printf()`
 */
static int32_t receive_msg_samping(ARINC_SENDER *self)
{
    RETURN_CODE_TYPE ret;
    sys_port_data_t *dst_place = self->state.port_buffer;

    if (!SYS_SAMPLING_PORT_CHECK_IS_NEW_DATA(self->state.port_id)) {
        return -1;
    }

    READ_SAMPLING_MESSAGE(
            self->state.port_id,
            (MESSAGE_ADDR_TYPE ) (dst_place->data + self->state.overhead),
            &dst_place->message_size,
            NULL,
            &ret
            );

    if (ret != NO_ERROR) {
        if (ret != NOT_AVAILABLE) {
            (void) printf(
                "ARINC_SENDER: %s port error: %d\n",
                self->state.port_name, (int) ret);
        }
        return -1;
    }

    return 0;
}

/**
 * @funcname `arinc_sender_activity`
 *
 * @description
 * The activity of the component: receives a message of the user partition and
 * sends it through the out port, then flushes it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_SENDER *` | [in] The component. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `dst_place` | `sys_port_data_t *` | The port buffer |
 * | `receive_error` | `int32_t` | Whether no message was received |
 * | `res` | `ret_e` | The result of the send |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `self->state.is_queuing_port` is not equal to `0`:
 *    1. Call `receive_msg_queuing()` with the argument `self`, and store the
 *       result in `receive_error`.
 * 2. Otherwise:
 *    1. Call `receive_msg_samping()` with the argument `self`, and store the
 *       result in `receive_error`.
 * 3. If `receive_error` is not equal to `0`:
 *    1. Return.
 * 4. Set `dst_place` to `self->state.port_buffer`.
 * 5. Call `ARINC_SENDER_call_portA_send()` with the arguments `self`,
 *    `dst_place->data + self->state.overhead`, `dst_place->message_size` and
 *    `self->state.overhead`, and store the result in `res`.
 * 6. If `res` is not equal to `EOK`:
 *    1. Call `printf()` with the argument
 *       `"ARINC_SENDER: Error in send_udp\n"`.
 * 7. Call `ARINC_SENDER_call_portA_flush()` with the argument `self`.
 *
 * @calls
 * - `receive_msg_queuing()`
 * - `receive_msg_samping()`
 * - `ARINC_SENDER_call_portA_send()`
 * - `printf()`
 * - `ARINC_SENDER_call_portA_flush()`
 */
void arinc_sender_activity(ARINC_SENDER *self)
{
    int32_t receive_error;
    if (self->state.is_queuing_port != 0) {
        receive_error = receive_msg_queuing(self);
    } else {
        receive_error = receive_msg_samping(self);
    }

    if (receive_error != 0) {
        return;
    }

    sys_port_data_t *dst_place = self->state.port_buffer;
    ret_e res = ARINC_SENDER_call_portA_send(self,
            dst_place->data + self->state.overhead,
            dst_place->message_size,
            self->state.overhead
            );

    if (res != EOK) {
        (void) printf("ARINC_SENDER: Error in send_udp\n");
    }

    (void) ARINC_SENDER_call_portA_flush(self);
}

/**
 * @funcname `arinc_sender_init`
 *
 * @description
 * The init function of the component: creates its queuing or sampling port and
 * allocates its port buffer (with room for the headers).
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
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `RETURN_CODE_TYPE` | The result of the ARINC 653 service |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `self->state.is_queuing_port` is not equal to `0`:
 *    1. Call `CREATE_QUEUING_PORT()` with the arguments
 *       `self->state.port_name`, `self->state.port_max_message_size`,
 *       `self->state.q_port_max_nb_messages`, `self->state.port_direction`,
 *       `FIFO`, the address of `self->state.port_id` and the address of `ret`.
 * 2. Otherwise:
 *    1. Call `CREATE_SAMPLING_PORT()` with the arguments
 *       `self->state.port_name`, `self->state.port_max_message_size`,
 *       `self->state.port_direction`, `0`, the address of `self->state.port_id`
 *       and the address of `ret`.
 * 3. If `ret` is not equal to `NO_ERROR`:
 *    1. Call `printf()` with the arguments
 *       `"ARINC_SENDER: error %%d during creation %%s port\n"`, `ret` and
 *       `self->state.port_name`.
 *    2. Return.
 * 4. Call `printf()` with the arguments
 *    `"ARINC_SENDER: successfuly create %%s port\n"`
 *    and `self->state.port_name`.
 * 5. Call `smalloc()` with the argument
 *    `sizeof(*self->state.port_buffer) + self->state.overhead + (size_t)`
 *    `self->state.port_max_message_size`, and store the result in
 *    `self->state.port_buffer`.
 *
 * @calls
 * - `CREATE_QUEUING_PORT()`
 * - `CREATE_SAMPLING_PORT()`
 * - `printf()`
 * - `smalloc()`
 */
void arinc_sender_init(ARINC_SENDER *self)
{
    RETURN_CODE_TYPE ret;
    if (self->state.is_queuing_port != 0) {
        CREATE_QUEUING_PORT(
                self->state.port_name,
                self->state.port_max_message_size,
                self->state.q_port_max_nb_messages,
                self->state.port_direction,
                FIFO,
                &self->state.port_id,
                &ret);
    } else {
        CREATE_SAMPLING_PORT(
                self->state.port_name,
                self->state.port_max_message_size,
                self->state.port_direction,
                0, //in future should be any positive number
                &self->state.port_id,
                &ret);
    }

    if (ret != NO_ERROR) {
        (void) printf(
            "ARINC_SENDER: error %d during creation %s port\n", ret,
                self->state.port_name);
        return;
    }

    (void) printf("ARINC_SENDER: successfuly create %s port\n",
                  self->state.port_name);

    self->state.port_buffer = smalloc(
            sizeof(*self->state.port_buffer) +
            self->state.overhead +
            (size_t) self->state.port_max_message_size);
}
