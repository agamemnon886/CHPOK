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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: braces around
 * all if/else/loop bodies; discarded results are cast to void;
 * essentially Boolean conditions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * ret_e (was ret_t).
 */

/**
 * @file
 * The ARINC_RECEIVER component: messages received from the network go to
 * a user partition through an ARINC 653 queuing or sampling port.
 */

#include <arinc653/queueing.h>
#include <arinc653/sampling.h>
#include <stdio.h>

#include <port_info.h>

#include "ARINC_RECEIVER_gen.h"

/**
 * @funcname `send_msg_to_user_partition_queuing`
 *
 * @description
 * Sends a message to the user partition through the queuing port (no wait).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_RECEIVER *` | [in] The component. |
 * | `payload` | `const char *` | [in] The message. |
 * | `length` | `size_t` | [in] Its length. |
 *
 * @return EOK, EAGAIN if the queue is full (the message is dropped), or EINVAL.
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
 * 1. Call `SEND_QUEUING_MESSAGE()` with the arguments `self->state.port_id`,
 *    `(const APEX_BYTE *) payload`, `length`, `0` and the address of `ret`.
 * 2. If `ret` is not equal to `NO_ERROR`:
 *    1. If `ret` is equal to `NOT_AVAILABLE`:
 *       1. Call `printf()` with the arguments
 *          `"ARINC_RECEIVER: %%s port queue is full, drop packet\n"` and
 *          `self->state.port_name`.
 *       2. Return `EAGAIN`.
 *    2. Otherwise:
 *       1. Call `printf()` with the arguments
 *          `"ARINC_RECEIVER: %%s port error: %%d\n"`,
 *          `self->state.port_name` and `ret`.
 *       2. Return `EINVAL`.
 * 3. Return `EOK`.
 *
 * @calls
 * - `SEND_QUEUING_MESSAGE()`
 * - `printf()`
 */
static ret_e send_msg_to_user_partition_queuing(ARINC_RECEIVER *self,
    const char *payload, size_t length)
{
    RETURN_CODE_TYPE ret;

    SEND_QUEUING_MESSAGE(
            self->state.port_id,
            (const APEX_BYTE *) payload,
            length,
            0,
            &ret);

    if (ret != NO_ERROR) {
        if (ret == NOT_AVAILABLE) {
            (void) printf(
                "ARINC_RECEIVER: %s port queue is full, drop packet\n",
                    self->state.port_name);
            return EAGAIN; //what should be returned???
        } else {
            (void) printf(
                "ARINC_RECEIVER: %s port error: %d\n",
                self->state.port_name, ret);
            return EINVAL;
        }
    }
    return EOK;
}

/**
 * @funcname `send_msg_to_user_partition_sampling`
 *
 * @description
 * Writes a message to the user partition through the sampling port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_RECEIVER *` | [in] The component. |
 * | `payload` | `const char *` | [in] The message. |
 * | `length` | `size_t` | [in] Its length. |
 *
 * @return EOK or EINVAL.
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
 * 1. Call `WRITE_SAMPLING_MESSAGE()` with the arguments `self->state.port_id`,
 *    `(const APEX_BYTE *) payload`, `length` and the address of `ret`.
 * 2. If `ret` is not equal to `NO_ERROR`:
 *    1. Call `printf()` with the arguments
 *       `"ARINC_RECEIVER: %%s port error: %%d\n"`,
 *       `self->state.port_name` and `ret`.
 *    2. Return `EINVAL`.
 * 3. Return `EOK`.
 *
 * @calls
 * - `WRITE_SAMPLING_MESSAGE()`
 * - `printf()`
 */
static ret_e send_msg_to_user_partition_sampling(ARINC_RECEIVER *self,
    const char *payload, size_t length)
{
    RETURN_CODE_TYPE ret;

    WRITE_SAMPLING_MESSAGE(
            self->state.port_id,
            (const APEX_BYTE *) payload,
            length,
            &ret);

    if (ret != NO_ERROR) {
        (void) printf("ARINC_RECEIVER: %s port error: %d\n",
                      self->state.port_name, ret);
        return EINVAL;
    }
    return EOK;

}

/**
 * @funcname `arinc_receiver_init`
 *
 * @description
 * The init function of the component: creates its queuing or sampling port.
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
 *
 * @calls
 * - `CREATE_QUEUING_PORT()`
 * - `CREATE_SAMPLING_PORT()`
 */
void arinc_receiver_init(ARINC_RECEIVER *self)
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
}

/**
 * @funcname `arinc_receive_message`
 *
 * @description
 * The handle operation of the component's in port: passes a message to the user
 * partition.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `ARINC_RECEIVER *` | [in] The component. |
 * | `payload` | `const char *` | [in] The message. |
 * | `payload_size` | `size_t` | [in] Its length. |
 *
 * @return The result of the send (EOK on success).
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
 * 1. If `self->state.is_queuing_port` is not equal to `0`:
 *    1. Call `send_msg_to_user_partition_queuing()` with the arguments `self`,
 *       `payload` and `payload_size`, and return its result.
 * 2. Otherwise:
 *    1. Call `send_msg_to_user_partition_sampling()` with the arguments `self`,
 *       `payload` and `payload_size`, and return its result.
 *
 * @calls
 * - `send_msg_to_user_partition_queuing()`
 * - `send_msg_to_user_partition_sampling()`
 */
ret_e arinc_receive_message(ARINC_RECEIVER *self, const char *payload,
    size_t payload_size)
{
    //printf("ARINC_RECEIVER: %s got message\n", self->state.tmp_name);
    if (self->state.is_queuing_port != 0) {
        return send_msg_to_user_partition_queuing(self, payload, payload_size);
    } else {
        return send_msg_to_user_partition_sampling(self, payload, payload_size);
    }
}
