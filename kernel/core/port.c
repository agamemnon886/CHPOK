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
 * Modified by Kirill Brazhnikov, 2026-10-31: sampling message validity
 * is computed without overflow, so any positive refresh period works.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: the sampling read syscall
 * checks the validity pointer (it checked the length pointer twice).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; braces around all if/else/loop bodies;
 * essentially Boolean conditions; U suffixes for unsigned constants;
 * discarded results are cast to void.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: port IDs equal to
 * the number of ports are rejected (they were used as an index one past the end
 * of the port array); operands of mixed precedence are parenthesized;
 * deviations are documented where a guideline can't be followed. Long lines are
 * wrapped at 80 columns. pok_port_queuing_create() rejects a queuing discipline
 * other than FIFO or priority.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_queuing_discipline_e (was pok_queuing_discipline_t), pok_ret_e (was
 * pok_ret_t).
 */

/**
 * @file
 * The ARINC 653 queuing and sampling ports of a partition: their creation,
 * send and receive (a thread waits, with a timeout, on a full or empty
 * queuing port), status, IDs and clearing. The messages go through the
 * channels of channel.c.
 */

#include <core/port.h>
#include <core/partition_arinc.h>
#include <core/thread.h>
#include "thread_internal.h"
#include <core/uaccess.h>
#include <core/sched_arinc.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the jet_user_to_kernel*()
 * helpers return the kernel address of user memory as a pointer to void,
 * which is converted to the type of the object the user passed.
 */

/*
 * MISRA C:2025 Rule 15.1 deviation (this file): forward gotos to the
 * labels out and err leave a function through its common exit path, which
 * releases what the function has taken.
 */

/**
 * @funcname `find_port_queuing`
 *
 * @description
 * Finds a queuing port of the current partition by name (case insensitive).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_name` | `const char * JET_KUSER` | [in] The name, in kernel memory. |
 *
 * @return The port, or NULL.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kernel_name` | `char[30]` | A copy of the name |
 * | `port_queuing` | `pok_port_queuing_t *` | The port looked at |
 * | `ports_queuing_end` | `pok_port_queuing_t *` | The end of the partition's queuing ports |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `port_queuing` to `current_partition_arinc->ports_queuing`.
 * 2. Set `ports_queuing_end` to
 *    `port_queuing + current_partition_arinc->nports_queuing`.
 * 3. Call `memcpy()` with the arguments `kernel_name`, `k_name` and
 *    `MAX_NAME_LENGTH`.
 * 4. While `port_queuing` is less than `ports_queuing_end`, repeat (after each
 *    pass: increment `port_queuing`):
 *    1. If `pok_compare_names(port_queuing->name, kernel_name)` is equal to
 *       `0`:
 *       1. Return `port_queuing`.
 * 5. Return `NULL`.
 *
 * @calls
 * - `memcpy()`
 * - `pok_compare_names()`
 */
static pok_port_queuing_t* find_port_queuing(const char* JET_KUSER k_name)
{
    char kernel_name[MAX_NAME_LENGTH];

    pok_port_queuing_t* port_queuing =
        current_partition_arinc->ports_queuing;

    pok_port_queuing_t* ports_queuing_end =
        port_queuing + current_partition_arinc->nports_queuing;

    (void) memcpy(kernel_name, k_name, MAX_NAME_LENGTH);

    for(; port_queuing < ports_queuing_end; port_queuing++)
    {
        if(pok_compare_names(port_queuing->name, kernel_name) == 0) {
            return port_queuing;
        }
    }

    return NULL;
}


/**
 * @funcname `get_port_queuing`
 *
 * @description
 * Returns a created queuing port of the current partition by its ID.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 *
 * @return The port, or NULL if the ID is bad or the port isn't created.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `part` | `pok_partition_arinc_t *` | The current partition |
 * | `port_queuing` | `pok_port_queuing_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 * 2. If `id` is greater than or equal to `part->nports_queuing`:
 *    1. Return `NULL`.
 * 3. Set `port_queuing` to the address of `part->ports_queuing[id]`.
 * 4. Return `port_queuing` if `port_queuing->is_created` is true, otherwise
 *    `NULL`.
 *
 * @calls
 * None.
 */
static pok_port_queuing_t* get_port_queuing(pok_port_id_t id)
{
    pok_partition_arinc_t* part = current_partition_arinc;

    pok_port_queuing_t* port_queuing;

    // Port IDs are indexes into ports_queuing[] (0 .. nports_queuing - 1).
    if(id >= part->nports_queuing) {
        return NULL;
    }

    port_queuing = &part->ports_queuing[id];

    return port_queuing->is_created ? port_queuing : NULL;
}


/**
 * @funcname `port_queuing_receive`
 *
 * @description
 * Receives the first message of a queuing port's channel into the buffer a
 * thread gave; the thread's wait result is POK_ERRNO_TOOMANY if messages were
 * discarded (overflow), POK_ERRNO_OK otherwise.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `pok_port_queuing_t *` | [in] The port (with a message). |
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message` | `const char *` | The message |
 * | `message_discarded` | `pok_bool_t` | Whether messages were discarded |
 * | `message_size` | `pok_message_size_t` | The size of the message |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_channel_queuing_r_get_message()` with the arguments
 *    `port->channel`, the address of `message_size` and `FALSE`, and store the
 *    result in `message`.
 * 2. Check that `message != NULL` holds (assert: stop otherwise).
 * 3. Call `memcpy()` with the arguments `t->wait_buffer.dest`, `message` and
 *    `message_size`.
 * 4. Set `t->wait_len` to `message_size`.
 * 5. Call `pok_channel_queuing_r_consume_message()` with the arguments
 *    `port->channel` and the address of `message_discarded`.
 * 6. Set `t->wait_result` to `POK_ERRNO_TOOMANY` if `message_discarded` is
 *    true, otherwise `POK_ERRNO_OK`.
 *
 * @calls
 * - `pok_channel_queuing_r_get_message()`
 * - `pok_assertion_fail()`
 * - `memcpy()`
 * - `pok_channel_queuing_r_consume_message()`
 */
void port_queuing_receive(pok_port_queuing_t* port, pok_thread_t* t)
{
    pok_message_size_t message_size;
    const char* message = pok_channel_queuing_r_get_message(
        port->channel, &message_size, FALSE);

    assert(message != NULL);

    (void) memcpy(t->wait_buffer.dest, message, message_size);
    t->wait_len = message_size;

    pok_bool_t message_discarded;
    pok_channel_queuing_r_consume_message(port->channel, &message_discarded);

    t->wait_result = message_discarded? POK_ERRNO_TOOMANY : POK_ERRNO_OK;
}

/**
 * @funcname `port_queuing_send`
 *
 * @description
 * Sends the message a thread gave into a queuing port's channel (which has
 * room); the thread's wait result is POK_ERRNO_OK.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port` | `pok_port_queuing_t *` | [in] The port. |
 * | `t` | `pok_thread_t *` | [in,out] The thread. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message` | `char *` | The channel's room for the message |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_channel_queuing_s_get_message()` with the arguments
 *    `port->channel` and `FALSE`, and store the result in `message`.
 * 2. Check that `message != NULL` holds (assert: stop otherwise).
 * 3. Call `memcpy()` with the arguments `message`, `t->wait_buffer.src` and
 *    `t->wait_len`.
 * 4. Call `pok_channel_queuing_s_produce_message()` with the arguments
 *    `port->channel` and `t->wait_len`.
 * 5. Set `t->wait_result` to `POK_ERRNO_OK`.
 *
 * @calls
 * - `pok_channel_queuing_s_get_message()`
 * - `pok_assertion_fail()`
 * - `memcpy()`
 * - `pok_channel_queuing_s_produce_message()`
 */
void port_queuing_send(pok_port_queuing_t* port, pok_thread_t* t)
{
    char* message = pok_channel_queuing_s_get_message(port->channel, FALSE);
    assert(message != NULL);

    (void) memcpy(message, t->wait_buffer.src, t->wait_len);

    pok_channel_queuing_s_produce_message(port->channel, t->wait_len);

    t->wait_result = POK_ERRNO_OK;
}

/**
 * @funcname `pok_port_queuing_init`
 *
 * @description
 * Initializes a queuing port at boot: no waiting thread, not created.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port_queuing` | `pok_port_queuing_t *` | [out] The port. |
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
 * 1. Call `pok_thread_wq_init()` with the address of `port_queuing->waiters`.
 * 2. Set `port_queuing->is_created` to `FALSE`.
 *
 * @calls
 * - `pok_thread_wq_init()`
 */
void pok_port_queuing_init(pok_port_queuing_t* port_queuing)
{
    pok_thread_wq_init(&port_queuing->waiters);

    port_queuing->is_created = FALSE;
}


/**
 * @funcname `pok_port_queuing_create`
 *
 * @description
 * Creates a queuing port (CREATE_QUEUING_PORT, outside NORMAL mode): the name
 * must be a port of the configuration, with its message size, number of
 * messages and direction; the channel side is initialized.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char * JET_USER` | [in] Partition address of the name. |
 * | `message_size` | `pok_port_size_t` | [in] The maximum message size. |
 * | `max_nb_message` | `pok_port_size_t` | [in] The maximum number of messages. |
 * | `direction` | `pok_port_direction_t` | [in] The direction. |
 * | `discipline` | `pok_queuing_discipline_e` | [in] FIFO or PRIORITY. |
 * | `id` | `pok_port_id_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EFAULT for a bad address;
 *         POK_ERRNO_UNAVAILABLE for an unknown name; POK_ERRNO_EXISTS if
 *         created already; POK_ERRNO_EINVAL if an attribute differs from the
 *         configuration; POK_ERRNO_MODE in NORMAL mode.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_id` | `pok_port_id_t *` | Kernel address of the result |
 * | `k_name` | `const char *` | Kernel address of the name |
 * | `port_queuing` | `pok_port_queuing_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 * 2. If `k_name` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 * 4. If `k_id` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `find_port_queuing()` with the argument `k_name`, and store the
 *    result in `port_queuing`.
 * 6. If `port_queuing` is NULL:
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 7. If `port_queuing->is_created` is true:
 *    1. Return `POK_ERRNO_EXISTS`.
 * 8. If `message_size` is not equal to
 *    `port_queuing->channel->max_message_size`:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 9. If `direction` is not equal to `port_queuing->direction`:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 10. If `discipline` is not equal to `POK_QUEUING_DISCIPLINE_FIFO` and
 *     `discipline` is not equal to `POK_QUEUING_DISCIPLINE_PRIORITY`:
 *     1. Return `POK_ERRNO_EINVAL`.
 * 11. If `direction` is equal to `POK_PORT_DIRECTION_IN`:
 *     1. If `max_nb_message` is not equal to
 *        `port_queuing->channel->recv.max_nb_message`:
 *        1. Return `POK_ERRNO_EINVAL`.
 * 12. Otherwise:
 *     1. If `max_nb_message` is not equal to
 *        `port_queuing->channel->send.max_nb_message`:
 *        1. Return `POK_ERRNO_EINVAL`.
 * 13. If `current_partition_arinc->mode` is equal to
 *     `POK_PARTITION_MODE_NORMAL`:
 *     1. Return `POK_ERRNO_MODE`.
 * 14. Set `port_queuing->is_created` to `TRUE`.
 * 15. Set `port_queuing->discipline` to `discipline`.
 * 16. If `direction` is equal to `POK_PORT_DIRECTION_IN`:
 *     1. Call `pok_channel_queuing_side_init()` with the arguments
 *        `port_queuing->channel`, the address of `port_queuing->channel->recv`
 *        and `port_queuing - current_partition_arinc->ports_queuing`.
 * 17. Otherwise:
 *     1. Call `pok_channel_queuing_side_init()` with the arguments
 *        `port_queuing->channel`, the address of `port_queuing->channel->send`
 *        and `port_queuing - current_partition_arinc->ports_queuing`.
 * 18. Set `*k_id` to `port_queuing - current_partition_arinc->ports_queuing`.
 * 19. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_user_to_kernel()`
 * - `find_port_queuing()`
 * - `pok_channel_queuing_side_init()`
 */
pok_ret_e pok_port_queuing_create(
    const char* JET_USER              name,
    pok_port_size_t                 message_size,
    pok_port_size_t                 max_nb_message,
    pok_port_direction_t            direction,
    pok_queuing_discipline_e        discipline,
    pok_port_id_t* JET_USER           id)
{
    pok_port_queuing_t* port_queuing;

    const char* JET_KUSER k_name = jet_user_to_kernel_ro(name, MAX_NAME_LENGTH);
    if(k_name == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_port_id_t* JET_KUSER k_id = jet_user_to_kernel_typed(id);
    if(k_id == NULL) {
        return POK_ERRNO_EFAULT;
    }

    port_queuing = find_port_queuing(k_name);

    if(port_queuing == NULL) {
        return POK_ERRNO_UNAVAILABLE;
    }

    if(port_queuing->is_created) {
        return POK_ERRNO_EXISTS;
    }

    if(message_size != port_queuing->channel->max_message_size) {
        return POK_ERRNO_EINVAL;
    }
    if(direction != port_queuing->direction) {
        return POK_ERRNO_EINVAL;
    }
    if((discipline != POK_QUEUING_DISCIPLINE_FIFO)
       && (discipline != POK_QUEUING_DISCIPLINE_PRIORITY)) {
        return POK_ERRNO_EINVAL;
    }

    if(direction == POK_PORT_DIRECTION_IN)
    {
        if(max_nb_message != port_queuing->channel->recv.max_nb_message) {
            return POK_ERRNO_EINVAL;
        }
    }
    else // (direction == POK_PORT_DIRECTION_OUT)
    {
        if(max_nb_message != port_queuing->channel->send.max_nb_message) {
            return POK_ERRNO_EINVAL;
        }
    }


    if(current_partition_arinc->mode == POK_PARTITION_MODE_NORMAL) {
        return POK_ERRNO_MODE;
    }

    port_queuing->is_created = TRUE;
    port_queuing->discipline = discipline;

    if(direction == POK_PORT_DIRECTION_IN) {
        pok_channel_queuing_side_init(port_queuing->channel,
            &port_queuing->channel->recv,
            port_queuing - current_partition_arinc->ports_queuing);
    }
    else {
        /* direction == POK_PORT_DIRECTION_OUT */
        pok_channel_queuing_side_init(port_queuing->channel,
            &port_queuing->channel->send,
            port_queuing - current_partition_arinc->ports_queuing);
    }

    *k_id = port_queuing - current_partition_arinc->ports_queuing;

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_port_queuing_create_packed`
 *
 * @description
 * Creates a queuing port with its attributes in one structure (the syscall has
 * too few arguments for them).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char * JET_USER` | [in] Partition address of the name. |
 * | `arg` | `const pok_port_queuing_create_arg_t * JET_USER` | [in] Partition address of the attributes. |
 * | `id` | `pok_port_id_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return As pok_port_queuing_create(); POK_ERRNO_EFAULT for a bad address.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_arg` | `const pok_port_queuing_create_arg_t *` | Kernel address of the attributes |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `k_arg` to `jet_user_to_kernel_typed_ro(arg)`.
 * 2. If `k_arg` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Call `pok_port_queuing_create()` with the arguments `name`,
 *    `k_arg->message_size`, `k_arg->max_nb_message`, `k_arg->direction`,
 *    `k_arg->discipline` and `id`, and return its result.
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `pok_port_queuing_create()`
 */
pok_ret_e pok_port_queuing_create_packed(
    const char* JET_USER              name,
    const pok_port_queuing_create_arg_t* JET_USER arg,
    pok_port_id_t* JET_USER           id)
{
    const pok_port_queuing_create_arg_t* JET_KUSER k_arg =
        jet_user_to_kernel_typed_ro(arg);
    if(k_arg == NULL) {
        return POK_ERRNO_EFAULT;
    }

    return pok_port_queuing_create(
       name,
       k_arg->message_size,
       k_arg->max_nb_message,
       k_arg->direction,
       k_arg->discipline,
       id);
}


/**
 * @funcname `pok_port_queuing_receive`
 *
 * @description
 * Receives a message from a queuing port (RECEIVE_QUEUING_MESSAGE): at once if
 * one is there and no thread waits before, otherwise the thread waits up to the
 * timeout (in the port's discipline).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 * | `timeout` | `const pok_time_t * JET_USER` | [in] Partition address of the timeout. |
 * | `data` | `void * JET_USER` | [out] Partition address of the buffer. |
 * | `len` | `pok_port_size_t * JET_USER` | [out] Partition address of the length received. |
 *
 * @return The thread's wait result (POK_ERRNO_OK, POK_ERRNO_TOOMANY on an
 *         overflow, POK_ERRNO_TIMEOUT); POK_ERRNO_PORT for a bad ID;
 *         POK_ERRNO_EFAULT for a bad address; POK_ERRNO_MODE for an OUT port or
 *         if the thread may not wait; POK_ERRNO_EMPTY with no message and
 *         timeout 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_data` | `void *` | Kernel address of the buffer |
 * | `k_len` | `pok_port_size_t *` | Kernel address of the length |
 * | `k_timeout` | `const pok_time_t *` | Kernel address of the timeout |
 * | `kernel_timeout` | `pok_time_t` | The timeout |
 * | `message_size` | `pok_message_size_t` | The size of a message (not used) |
 * | `port_queuing` | `pok_port_queuing_t *` | The port |
 * | `ret` | `pok_ret_e` | Whether the thread may wait |
 * | `t` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_port_queuing()` with the argument `id`, and store the result in
 *    `port_queuing`.
 * 2. If `port_queuing` is NULL:
 *    1. Return `POK_ERRNO_PORT`.
 * 3. Call `jet_user_to_kernel()` with the arguments `data` and
 *    `port_queuing->channel->max_message_size`, and store the result in
 *    `k_data`.
 * 4. If `k_data` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `k_len` to `jet_user_to_kernel_typed(len)`.
 * 6. If `k_len` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 7. Set `k_timeout` to `jet_user_to_kernel_typed_ro(timeout)`.
 * 8. If `k_timeout` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 9. Set `kernel_timeout` to the value at `k_timeout`.
 * 10. If `port_queuing->direction` is not equal to `POK_PORT_DIRECTION_IN`:
 *     1. Return `POK_ERRNO_MODE`.
 * 11. Call `pok_preemption_local_disable()`.
 * 12. Set `t` to `current_thread`.
 * 13. If `kernel_timeout` is equal to `0`:
 *     1. Set `ret` to `POK_ERRNO_EMPTY`.
 * 14. Otherwise, if `thread_is_waiting_allowed()` is false:
 *     1. Set `ret` to `POK_ERRNO_MODE`.
 * 15. Otherwise:
 *     1. Set `ret` to `POK_ERRNO_OK`.
 * 16. If `pok_thread_wq_is_empty(&port_queuing->waiters)` is false or
 *     `(pok_channel_queuing_r_get_message(port_queuing->channel,`
 *     `&message_size, ret == POK_ERRNO_OK))` is NULL:
 *     1. If `ret` is not equal to `POK_ERRNO_OK`:
 *        1. Set `*k_len` to `0`.
 *        2. Go to the label `err`.
 *     2. Set `t->wait_buffer.dest` to `k_data`.
 *     3. Call `pok_thread_wq_add_common()` with the arguments the address of
 *        `port_queuing->waiters`, `t` and `port_queuing->discipline`.
 *     4. Call `thread_wait_common()` with the arguments `t` and
 *        `kernel_timeout`.
 *     5. Go to the label `out`.
 * 17. Set `t->wait_buffer.dest` to `k_data`.
 * 18. Call `port_queuing_receive()` with the arguments `port_queuing` and `t`.
 * 19. Label `out`:
 * 20. Call `pok_preemption_local_enable()`.
 * 21. Set `*k_len` to `t->wait_len` if (`t->wait_result` is equal to
 *     `POK_ERRNO_OK` or `t->wait_result` is equal to `POK_ERRNO_TOOMANY`),
 *     otherwise `0U`.
 * 22. Return `t->wait_result`.
 * 23. Label `err`:
 * 24. Call `pok_preemption_local_enable()`.
 * 25. Return `ret`.
 *
 * @calls
 * - `get_port_queuing()`
 * - `jet_user_to_kernel()`
 * - `jet_user_to_kernel_ro()`
 * - `pok_preemption_local_disable()`
 * - `thread_is_waiting_allowed()`
 * - `pok_thread_wq_is_empty()`
 * - `pok_channel_queuing_r_get_message()`
 * - `pok_thread_wq_add_common()`
 * - `thread_wait_common()`
 * - `port_queuing_receive()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_port_queuing_receive(
    pok_port_id_t               id,
    const pok_time_t* JET_USER    timeout,
    void* JET_USER                data,
    pok_port_size_t* JET_USER     len)
{
    pok_port_queuing_t* port_queuing;
    pok_ret_e ret;
    pok_thread_t* t;

    port_queuing = get_port_queuing(id);
    if(port_queuing == NULL) {
        return POK_ERRNO_PORT;
    }

    void* JET_KUSER k_data = jet_user_to_kernel(data,
        port_queuing->channel->max_message_size);
    if(k_data == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_port_size_t* JET_KUSER k_len = jet_user_to_kernel_typed(len);
    if(k_len == NULL) {
        return POK_ERRNO_EFAULT;
    }

    const pok_time_t* JET_KUSER k_timeout =
        jet_user_to_kernel_typed_ro(timeout);
    if(k_timeout == NULL) {
        return POK_ERRNO_EFAULT;
    }
    pok_time_t kernel_timeout = *k_timeout;

    if(port_queuing->direction != POK_PORT_DIRECTION_IN) {
        return POK_ERRNO_MODE;
    }

    pok_preemption_local_disable();

    t = current_thread;

    /*
     * We need to specify notification flag when trying get message
     * from the channel.
     * 
     * One possible way is to not specify flag first time, and,
     * if receive fails, calculate flag for the second receive attempt.
     * 
     * But here we calculate flag for the first (and the only) receive attempt.
     * 
     * If ret is not POK_ERRNO_OK, it contains error code to be returned
     * if channel is currently empty. Otherwise waiting is allowed.
     */

    if(kernel_timeout == 0) {
        ret = POK_ERRNO_EMPTY;
    } else if(!thread_is_waiting_allowed()) {
        ret = POK_ERRNO_MODE;
    } else {
        ret = POK_ERRNO_OK;
    }

    pok_message_size_t message_size; // Only for call r_get_message().
    if(!pok_thread_wq_is_empty(&port_queuing->waiters) ||
        ((pok_channel_queuing_r_get_message(port_queuing->channel,
            &message_size,
            ret == POK_ERRNO_OK)) == NULL))
    {
        if(ret != POK_ERRNO_OK)
        {
            // Waiting is not allowed, but it is needed.
            *k_len = 0;
            goto err;
        }

        // Prepare to wait.
        t->wait_buffer.dest = k_data;

        pok_thread_wq_add_common(&port_queuing->waiters, t,
            port_queuing->discipline);

        thread_wait_common(t, kernel_timeout);

        goto out;
    }

    /* Message is ready in the buffer. */
    t->wait_buffer.dest = k_data;
    port_queuing_receive(port_queuing, t);

out:
    pok_preemption_local_enable(); // Possible wait here

    *k_len = ((t->wait_result == POK_ERRNO_OK)
              || (t->wait_result == POK_ERRNO_TOOMANY))?
        t->wait_len: // Success.
        0U; // Fail

    return t->wait_result;

err:
    pok_preemption_local_enable();

    return ret;
}



/**
 * @funcname `pok_port_queuing_send`
 *
 * @description
 * Sends a message to a queuing port (SEND_QUEUING_MESSAGE): at once if there is
 * room and no thread waits before, otherwise the thread waits up to the timeout
 * (in the port's discipline).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 * | `data` | `const void * JET_USER` | [in] Partition address of the message. |
 * | `len` | `pok_port_size_t` | [in] The message's length. |
 * | `timeout` | `const pok_time_t * JET_USER` | [in] Partition address of the timeout. |
 *
 * @return The thread's wait result (POK_ERRNO_OK, POK_ERRNO_TIMEOUT);
 *         POK_ERRNO_PORT for a bad ID; POK_ERRNO_MODE for an IN port or if the
 *         thread may not wait; POK_ERRNO_EINVAL for a bad length;
 *         POK_ERRNO_EFAULT for a bad address; POK_ERRNO_FULL with no room and
 *         timeout 0.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_data` | `const void *` | Kernel address of the message |
 * | `k_timeout` | `const pok_time_t *` | Kernel address of the timeout |
 * | `kernel_timeout` | `pok_time_t` | The timeout |
 * | `port_queuing` | `pok_port_queuing_t *` | The port |
 * | `ret` | `pok_ret_e` | Whether the thread may wait |
 * | `t` | `pok_thread_t *` | The current thread |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `t` to `current_thread`.
 * 2. Call `get_port_queuing()` with the argument `id`, and store the result in
 *    `port_queuing`.
 * 3. If `port_queuing` is NULL:
 *    1. Return `POK_ERRNO_PORT`.
 * 4. If `port_queuing->direction` is not equal to `POK_PORT_DIRECTION_OUT`:
 *    1. Return `POK_ERRNO_MODE`.
 * 5. If `len` is equal to `0U`:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 6. If `len` is greater than `port_queuing->channel->max_message_size`:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 7. Call `jet_user_to_kernel_ro()` with the arguments `data` and `len`, and
 *    store the result in `k_data`.
 * 8. If `k_data` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 9. Set `k_timeout` to `jet_user_to_kernel_typed_ro(timeout)`.
 * 10. If `k_timeout` is NULL:
 *     1. Return `POK_ERRNO_EFAULT`.
 * 11. Set `kernel_timeout` to the value at `k_timeout`.
 * 12. Call `pok_preemption_local_disable()`.
 * 13. If `kernel_timeout` is equal to `0`:
 *     1. Set `ret` to `POK_ERRNO_FULL`.
 * 14. Otherwise, if `thread_is_waiting_allowed()` is false:
 *     1. Set `ret` to `POK_ERRNO_MODE`.
 * 15. Otherwise:
 *     1. Set `ret` to `POK_ERRNO_OK`.
 * 16. If `pok_thread_wq_is_empty(&port_queuing->waiters)` is false or
 *     `pok_channel_queuing_s_get_message(port_queuing->channel, ret ==`
 *     `POK_ERRNO_OK)` is NULL:
 *     1. If `ret` is not equal to `POK_ERRNO_OK`:
 *        1. Go to the label `err`.
 *     2. Set `t->wait_len` to `len`.
 *     3. Set `t->wait_buffer.src` to `k_data`.
 *     4. Call `pok_thread_wq_add_common()` with the arguments the address of
 *        `port_queuing->waiters`, `t` and `port_queuing->discipline`.
 *     5. Call `thread_wait_common()` with the arguments `t` and
 *        `kernel_timeout`.
 *     6. Go to the label `out`.
 * 17. Set `t->wait_len` to `len`.
 * 18. Set `t->wait_buffer.src` to `k_data`.
 * 19. Call `port_queuing_send()` with the arguments `port_queuing` and `t`.
 * 20. Label `out`:
 * 21. Call `pok_preemption_local_enable()`.
 * 22. Return `t->wait_result`.
 * 23. Label `err`:
 * 24. Call `pok_preemption_local_enable()`.
 * 25. Return `ret`.
 *
 * @calls
 * - `get_port_queuing()`
 * - `jet_user_to_kernel_ro()`
 * - `pok_preemption_local_disable()`
 * - `thread_is_waiting_allowed()`
 * - `pok_thread_wq_is_empty()`
 * - `pok_channel_queuing_s_get_message()`
 * - `pok_thread_wq_add_common()`
 * - `thread_wait_common()`
 * - `port_queuing_send()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_port_queuing_send(
    pok_port_id_t               id,
    const void* JET_USER          data,
    pok_port_size_t             len,
    const pok_time_t* JET_USER    timeout)
{
    pok_port_queuing_t* port_queuing;
    pok_ret_e ret;
    pok_thread_t* t = current_thread;

    port_queuing = get_port_queuing(id);

    if(port_queuing == NULL) {
        return POK_ERRNO_PORT;
    }

    if(port_queuing->direction != POK_PORT_DIRECTION_OUT) {
        return POK_ERRNO_MODE;
    }

    if(len == 0U) {
        return POK_ERRNO_EINVAL;
    }

    // error should be INVALID_CONFIG
    if(len > port_queuing->channel->max_message_size) {
        return POK_ERRNO_EINVAL;
    }

    const void* JET_KUSER k_data = jet_user_to_kernel_ro(data, len);
    if(k_data == NULL) {
        return POK_ERRNO_EFAULT;
    }

    const pok_time_t* JET_KUSER k_timeout =
        jet_user_to_kernel_typed_ro(timeout);
    if(k_timeout == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_time_t kernel_timeout = *k_timeout;

    pok_preemption_local_disable();

    /*
     * We need to specify notification flag when trying to send message
     * into the channel.
     *
     * One possible way is to not specify flag first time, and,
     * if send fails, calculate flag for the second receive attempt.
     * 
     * But here we calculate flag for the first (and the only) receive attempt.
     * 
     * If ret is not POK_ERRNO_OK, it contains error code to be returned
     * if channel is currently full. Otherwise waiting is allowed.
     */

    if(kernel_timeout == 0) {
        ret = POK_ERRNO_FULL;
    } else if(!thread_is_waiting_allowed()) {
        ret = POK_ERRNO_MODE;
    } else {
        ret = POK_ERRNO_OK;
    }

    if(!pok_thread_wq_is_empty(&port_queuing->waiters)
        || (pok_channel_queuing_s_get_message(port_queuing->channel,
                                              ret == POK_ERRNO_OK) == NULL))
    {
        if(ret != POK_ERRNO_OK)
        {
            // Waiting is not allowed, but it is needed.
            goto err;
        }

        // Prepare to wait.
        t->wait_len = len;
        t->wait_buffer.src = k_data;

        pok_thread_wq_add_common(&port_queuing->waiters, t,
            port_queuing->discipline);

        thread_wait_common(t, kernel_timeout);

        goto out;
    }
    /* There is place for message in the buffer. */
    t->wait_len = len;
    t->wait_buffer.src = k_data;

    port_queuing_send(port_queuing, t);

out:
    pok_preemption_local_enable(); // Possible wait here

    return t->wait_result;

err:
    pok_preemption_local_enable();
    return ret;
}


/**
 * @funcname `pok_port_queuing_status`
 *
 * @description
 * Gives the status of a queuing port (GET_QUEUING_PORT_STATUS): message size,
 * direction, waiting threads, maximum and current number of messages.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 * | `status` | `pok_port_queuing_status_t * JET_USER` | [out] Partition address of the status. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EFAULT for a bad address; POK_ERRNO_PORT for
 *         a bad ID.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | Its channel |
 * | `k_status` | `pok_port_queuing_status_t *` | Kernel address of the status |
 * | `port_queuing` | `pok_port_queuing_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 * 2. If `k_status` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Call `get_port_queuing()` with the argument `id`, and store the result in
 *    `port_queuing`.
 * 4. If `port_queuing` is NULL:
 *    1. Return `POK_ERRNO_PORT`.
 * 5. Set `channel` to `port_queuing->channel`.
 * 6. Set `k_status->max_message_size` to `channel->max_message_size`.
 * 7. Set `k_status->direction` to `port_queuing->direction`.
 * 8. Call `pok_preemption_local_disable()`.
 * 9. Call `pok_thread_wq_get_nwaits()` with the address of
 *    `port_queuing->waiters`, and store the result in
 *    `k_status->waiting_processes`.
 * 10. If `port_queuing->direction` is equal to `POK_PORT_DIRECTION_IN`:
 *     1. Set `k_status->max_nb_message` to `channel->recv.max_nb_message`.
 *     2. Call `pok_channel_queuing_r_n_messages()` with the argument `channel`,
 *        and store the result in `k_status->nb_message`.
 * 11. Otherwise:
 *     1. Set `k_status->max_nb_message` to `channel->send.max_nb_message`.
 *     2. Call `pok_channel_queuing_s_n_messages()` with the argument `channel`,
 *        and store the result in `k_status->nb_message`.
 * 12. Call `pok_preemption_local_enable()`.
 * 13. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `get_port_queuing()`
 * - `pok_preemption_local_disable()`
 * - `pok_thread_wq_get_nwaits()`
 * - `pok_channel_queuing_r_n_messages()`
 * - `pok_channel_queuing_s_n_messages()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_port_queuing_status(
    pok_port_id_t               id,
    pok_port_queuing_status_t * JET_USER status)
{
    pok_port_queuing_t* port_queuing;
    pok_channel_queuing_t* channel;

    pok_port_queuing_status_t* JET_KUSER k_status =
        jet_user_to_kernel_typed(status);
    if(k_status == NULL) {
        return POK_ERRNO_EFAULT;
    }

    port_queuing = get_port_queuing(id);

    if(port_queuing == NULL) {
        return POK_ERRNO_PORT;
    }

    channel = port_queuing->channel;

    k_status->max_message_size = channel->max_message_size;
    k_status->direction = port_queuing->direction;

    pok_preemption_local_disable();

    k_status->waiting_processes = pok_thread_wq_get_nwaits(&port_queuing->waiters);

    if(port_queuing->direction == POK_PORT_DIRECTION_IN) {
        k_status->max_nb_message = channel->recv.max_nb_message;
        k_status->nb_message = pok_channel_queuing_r_n_messages(channel);
    }
    else {
        /* port_queuing->direction == POK_PORT_DIRECTION_OUT */
        k_status->max_nb_message = channel->send.max_nb_message;
        k_status->nb_message = pok_channel_queuing_s_n_messages(channel);
    }

    pok_preemption_local_enable();

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_port_queuing_id`
 *
 * @description
 * Gives the ID of a created queuing port by its name (GET_QUEUING_PORT_ID).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char * JET_USER` | [in] Partition address of the name. |
 * | `id` | `pok_port_id_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EFAULT for a bad address;
 *         POK_ERRNO_UNAVAILABLE if there is no such created port.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_id` | `pok_port_id_t *` | Kernel address of the result |
 * | `k_name` | `const char *` | Kernel address of the name |
 * | `port_queuing` | `pok_port_queuing_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 * 2. If `k_name` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 * 4. If `k_id` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `find_port_queuing()` with the argument `k_name`, and store the
 *    result in `port_queuing`.
 * 6. If `port_queuing` is NULL:
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 7. If `port_queuing->is_created` is false:
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 8. Set `*k_id` to `port_queuing - current_partition_arinc->ports_queuing`.
 * 9. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_user_to_kernel()`
 * - `find_port_queuing()`
 */
pok_ret_e pok_port_queuing_id(
    const char* JET_USER name,
    pok_port_id_t* JET_USER id)
{
    pok_port_queuing_t* port_queuing;

    const char* JET_KUSER k_name = jet_user_to_kernel_ro(name, MAX_NAME_LENGTH);
    if(k_name == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_port_id_t* JET_KUSER k_id = jet_user_to_kernel_typed(id);
    if(k_id == NULL) {
        return POK_ERRNO_EFAULT;
    }

    port_queuing = find_port_queuing(k_name);

    if(port_queuing == NULL) {
        return POK_ERRNO_UNAVAILABLE;
    }
    if(!port_queuing->is_created) {
        return POK_ERRNO_UNAVAILABLE;
    }

    *k_id = port_queuing - current_partition_arinc->ports_queuing;

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_port_queuing_clear`
 *
 * @description
 * Discards the messages of a queuing IN port (CLEAR_QUEUING_PORT).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PORT for a bad ID; POK_ERRNO_MODE for an OUT
 *         port.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port_queuing` | `pok_port_queuing_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_port_queuing()` with the argument `id`, and store the result in
 *    `port_queuing`.
 * 2. If `port_queuing` is NULL:
 *    1. Return `POK_ERRNO_PORT`.
 * 3. If `port_queuing->direction` is not equal to `POK_PORT_DIRECTION_IN`:
 *    1. Return `POK_ERRNO_MODE`.
 * 4. Call `pok_preemption_local_disable()`.
 * 5. Call `pok_channel_queuing_side_init()` with the arguments
 *    `port_queuing->channel`, the address of `port_queuing->channel->recv` and
 *    `port_queuing - current_partition_arinc->ports_queuing`.
 * 6. Call `pok_preemption_local_enable()`.
 * 7. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `get_port_queuing()`
 * - `pok_preemption_local_disable()`
 * - `pok_channel_queuing_side_init()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_port_queuing_clear(pok_port_id_t id)
{
    pok_port_queuing_t* port_queuing = get_port_queuing(id);

    if(port_queuing == NULL) {
        return POK_ERRNO_PORT;
    }

    if(port_queuing->direction != POK_PORT_DIRECTION_IN) {
        return POK_ERRNO_MODE;
    }

    pok_preemption_local_disable();
    pok_channel_queuing_side_init(port_queuing->channel,
            &port_queuing->channel->recv,
            port_queuing - current_partition_arinc->ports_queuing);
    pok_preemption_local_enable();

    return POK_ERRNO_OK;
}

/**
 * @funcname `find_port_sampling`
 *
 * @description
 * Finds a sampling port of the current partition by name (case insensitive).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_name` | `const char * JET_KUSER` | [in] The name, in kernel memory. |
 *
 * @return The port, or NULL.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `kernel_name` | `char[30]` | A copy of the name |
 * | `port_sampling` | `pok_port_sampling_t *` | The port looked at |
 * | `ports_sampling_end` | `pok_port_sampling_t *` | The end of the partition's sampling ports |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `port_sampling` to `current_partition_arinc->ports_sampling`.
 * 2. Set `ports_sampling_end` to
 *    `port_sampling + current_partition_arinc->nports_sampling`.
 * 3. Call `memcpy()` with the arguments `kernel_name`, `k_name` and
 *    `MAX_NAME_LENGTH`.
 * 4. While `port_sampling` is less than `ports_sampling_end`, repeat (after
 *    each pass: increment `port_sampling`):
 *    1. If `pok_compare_names(port_sampling->name, kernel_name)` is equal to
 *       `0`:
 *       1. Return `port_sampling`.
 * 5. Return `NULL`.
 *
 * @calls
 * - `memcpy()`
 * - `pok_compare_names()`
 */
static pok_port_sampling_t* find_port_sampling(const char* JET_KUSER k_name)
{
    char kernel_name[MAX_NAME_LENGTH];

    pok_port_sampling_t* port_sampling =
        current_partition_arinc->ports_sampling;

    pok_port_sampling_t* ports_sampling_end =
        port_sampling + current_partition_arinc->nports_sampling;

    (void) memcpy(kernel_name, k_name, MAX_NAME_LENGTH);

    for(; port_sampling < ports_sampling_end; port_sampling++)
    {
        if(pok_compare_names(port_sampling->name, kernel_name) == 0) {
            return port_sampling;
        }
    }

    return NULL;
}


/**
 * @funcname `get_port_sampling`
 *
 * @description
 * Returns a created sampling port of the current partition by its ID.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 *
 * @return The port, or NULL if the ID is bad or the port isn't created.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port_sampling` | `pok_port_sampling_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `id` is greater than or equal to
 *    `current_partition_arinc->nports_sampling`:
 *    1. Return `NULL`.
 * 2. Set `port_sampling` to the address of
 *    `current_partition_arinc->ports_sampling[id]`.
 * 3. Return `port_sampling` if `port_sampling->is_created` is true, otherwise
 *    `NULL`.
 *
 * @calls
 * None.
 */
static pok_port_sampling_t* get_port_sampling(pok_port_id_t id)
{
    pok_port_sampling_t* port_sampling;

    // Port IDs are indexes into ports_sampling[] (0 .. nports_sampling - 1).
    if(id >= current_partition_arinc->nports_sampling) {
        return NULL;
    }

    port_sampling = &current_partition_arinc->ports_sampling[id];

    return port_sampling->is_created ? port_sampling : NULL;
}

/**
 * @funcname `pok_port_sampling_init`
 *
 * @description
 * Initializes a sampling port at boot: not created.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port_sampling` | `pok_port_sampling_t *` | [out] The port. |
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
 * 1. Set `port_sampling->is_created` to `FALSE`.
 *
 * @calls
 * None.
 */
void pok_port_sampling_init(pok_port_sampling_t* port_sampling)
{
    port_sampling->is_created = FALSE;
}


/**
 * @funcname `pok_port_sampling_create`
 *
 * @description
 * Creates a sampling port (CREATE_SAMPLING_PORT, outside NORMAL mode): the name
 * must be a port of the configuration, with its size and direction; the refresh
 * period must be finite and positive; the channel's message is cleared.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char * JET_USER` | [in] Partition address of the name. |
 * | `size` | `pok_port_size_t` | [in] The maximum message size. |
 * | `direction` | `pok_port_direction_t` | [in] The direction. |
 * | `refresh` | `const pok_time_t * JET_USER` | [in] Partition address of the refresh period. |
 * | `id` | `pok_port_id_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_MODE in NORMAL mode; POK_ERRNO_EFAULT for a
 *         bad address; POK_ERRNO_UNAVAILABLE for an unknown name;
 *         POK_ERRNO_EXISTS if created already; POK_ERRNO_EINVAL for a bad
 *         attribute.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_id` | `pok_port_id_t *` | Kernel address of the result |
 * | `k_name` | `const char *` | Kernel address of the name |
 * | `k_refresh` | `const pok_time_t *` | Kernel address of the refresh period |
 * | `kernel_refresh` | `pok_time_t` | The refresh period |
 * | `port_sampling` | `pok_port_sampling_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `current_partition_arinc->mode` is equal to
 *    `POK_PARTITION_MODE_NORMAL`:
 *    1. Return `POK_ERRNO_MODE`.
 * 2. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 * 3. If `k_name` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `k_refresh` to `jet_user_to_kernel_typed_ro(refresh)`.
 * 5. If `k_refresh` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 6. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 * 7. If `k_id` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 8. Set `kernel_refresh` to the value at `k_refresh`.
 * 9. Call `find_port_sampling()` with the argument `k_name`, and store the
 *    result in `port_sampling`.
 * 10. If `port_sampling` is NULL:
 *     1. Return `POK_ERRNO_UNAVAILABLE`.
 * 11. If `port_sampling->is_created` is true:
 *     1. Return `POK_ERRNO_EXISTS`.
 * 12. If `size` is not equal to `port_sampling->channel->max_message_size`:
 *     1. Return `POK_ERRNO_EINVAL`.
 * 13. If `direction` is not equal to `port_sampling->direction`:
 *     1. Return `POK_ERRNO_EINVAL`.
 * 14. If `pok_time_is_infinity(kernel_refresh) || (kernel_refresh == 0)` holds:
 *     1. Return `POK_ERRNO_EINVAL`.
 * 15. Set `port_sampling->is_created` to `TRUE`.
 * 16. Set `port_sampling->refresh_period` to `kernel_refresh`.
 * 17. Set `port_sampling->last_message_validity` to `FALSE`.
 * 18. If `port_sampling->direction` is equal to `POK_PORT_DIRECTION_OUT`:
 *     1. Call `pok_channel_sampling_s_clear_message()` with the argument
 *        `port_sampling->channel`.
 * 19. Otherwise:
 *     1. Call `pok_channel_sampling_r_clear_message()` with the argument
 *        `port_sampling->channel`.
 * 20. Set `*k_id` to `port_sampling - current_partition_arinc->ports_sampling`.
 * 21. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_user_to_kernel()`
 * - `find_port_sampling()`
 * - `pok_time_is_infinity()`
 * - `pok_channel_sampling_s_clear_message()`
 * - `pok_channel_sampling_r_clear_message()`
 */
pok_ret_e pok_port_sampling_create(
    const char* JET_USER          name,
    pok_port_size_t             size,
    pok_port_direction_t        direction,
    const pok_time_t* JET_USER    refresh,
    pok_port_id_t* JET_USER       id
)
{
    pok_port_sampling_t* port_sampling;

    if(current_partition_arinc->mode == POK_PARTITION_MODE_NORMAL) {
        return POK_ERRNO_MODE;
    }

    const char* JET_KUSER k_name = jet_user_to_kernel_ro(name, MAX_NAME_LENGTH);
    if(k_name == NULL) {
        return POK_ERRNO_EFAULT;
    }

    const pok_time_t* JET_KUSER k_refresh =
        jet_user_to_kernel_typed_ro(refresh);
    if(k_refresh == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_port_id_t* JET_KUSER k_id = jet_user_to_kernel_typed(id);
    if(k_id == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_time_t kernel_refresh = *k_refresh;

    port_sampling = find_port_sampling(k_name);

    if(port_sampling == NULL) {
        return POK_ERRNO_UNAVAILABLE;
    }
    if(port_sampling->is_created) {
        return POK_ERRNO_EXISTS;
    }

    if(size != port_sampling->channel->max_message_size) {
        return POK_ERRNO_EINVAL;
    }
    if(direction != port_sampling->direction) {
        return POK_ERRNO_EINVAL;
    }
    // ARINC specifies to check refresh period for any direction.
    if(pok_time_is_infinity(kernel_refresh) || (kernel_refresh == 0)) {
        return POK_ERRNO_EINVAL;
    }

    port_sampling->is_created = TRUE;

    // Useless for OUT port.
    port_sampling->refresh_period = kernel_refresh;
    port_sampling->last_message_validity = FALSE;

    if(port_sampling->direction == POK_PORT_DIRECTION_OUT)
    {
        pok_channel_sampling_s_clear_message(port_sampling->channel);
    }
    else
    {
        pok_channel_sampling_r_clear_message(port_sampling->channel);
    }

    *k_id = port_sampling - current_partition_arinc->ports_sampling;

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_port_sampling_write`
 *
 * @description
 * Writes a message to a sampling OUT port (WRITE_SAMPLING_MESSAGE).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 * | `data` | `const void * JET_USER` | [in] Partition address of the message. |
 * | `len` | `pok_port_size_t` | [in] The message's length. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PORT for a bad ID; POK_ERRNO_EINVAL for a bad
 *         length; POK_ERRNO_MODE for an IN port; POK_ERRNO_EFAULT for a bad
 *         address.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_data` | `const void *` | Kernel address of the message |
 * | `message` | `char *` | The channel's room for the message |
 * | `port_sampling` | `pok_port_sampling_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_port_sampling()` with the argument `id`, and store the result in
 *    `port_sampling`.
 * 2. If `port_sampling` is NULL:
 *    1. Return `POK_ERRNO_PORT`.
 * 3. If `len` is equal to `0U` or `len` is greater than
 *    `port_sampling->channel->max_message_size`:
 *    1. Return `POK_ERRNO_EINVAL`.
 * 4. If `port_sampling->direction` is not equal to `POK_PORT_DIRECTION_OUT`:
 *    1. Return `POK_ERRNO_MODE`.
 * 5. Call `jet_user_to_kernel_ro()` with the arguments `data` and `len`, and
 *    store the result in `k_data`.
 * 6. If `k_data` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 7. Call `pok_preemption_local_disable()`.
 * 8. Call `pok_channel_sampling_s_get_message()` with the argument
 *    `port_sampling->channel`, and store the result in `message`.
 * 9. Call `memcpy()` with the arguments `message`, `k_data` and `len`.
 * 10. Call `pok_channel_sampling_send_message()` with the arguments
 *     `port_sampling->channel` and `len`.
 * 11. Call `pok_preemption_local_enable()`.
 * 12. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `get_port_sampling()`
 * - `jet_user_to_kernel_ro()`
 * - `pok_preemption_local_disable()`
 * - `pok_channel_sampling_s_get_message()`
 * - `memcpy()`
 * - `pok_channel_sampling_send_message()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_port_sampling_write(
    pok_port_id_t           id,
    const void* JET_USER      data,
    pok_port_size_t         len)
{
    pok_port_sampling_t* port_sampling;

    char* message;

    port_sampling = get_port_sampling(id);

    if(port_sampling == NULL) {
        return POK_ERRNO_PORT;
    }

    if((len == 0U) || (len > port_sampling->channel->max_message_size)) {
        return POK_ERRNO_EINVAL;
    }

    if(port_sampling->direction != POK_PORT_DIRECTION_OUT) {
        return POK_ERRNO_MODE;
    }

    const void* JET_KUSER k_data = jet_user_to_kernel_ro(data, len);
    if(k_data == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_preemption_local_disable();

    message = pok_channel_sampling_s_get_message(port_sampling->channel);

    (void) memcpy(message, k_data, len);

    pok_channel_sampling_send_message(port_sampling->channel, len);

    pok_preemption_local_enable();

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_port_sampling_read`
 *
 * @description
 * Reads the message of a sampling IN port (READ_SAMPLING_MESSAGE), and whether
 * it is valid (younger than the refresh period).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 * | `data` | `void * JET_USER` | [out] Partition address of the buffer. |
 * | `len` | `pok_port_size_t * JET_USER` | [out] Partition address of the length read. |
 * | `valid` | `pok_bool_t * JET_USER` | [out] Partition address of the validity. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EMPTY with no message; POK_ERRNO_PORT for a
 *         bad ID; POK_ERRNO_DIRECTION for an OUT port; POK_ERRNO_EFAULT for a
 *         bad address.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_time` | `pok_time_t` | The current time |
 * | `k_data` | `void *` | Kernel address of the buffer |
 * | `k_len` | `pok_port_size_t *` | Kernel address of the length |
 * | `k_valid` | `pok_bool_t *` | Kernel address of the validity |
 * | `message` | `const char *` | The message |
 * | `message_size` | `pok_message_size_t` | The size of the message |
 * | `port_sampling` | `pok_port_sampling_t *` | The port |
 * | `ret` | `pok_ret_e` | The result |
 * | `ts` | `pok_time_t` | The time the message was written |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_port_sampling()` with the argument `id`, and store the result in
 *    `port_sampling`.
 * 2. If `port_sampling` is NULL:
 *    1. Return `POK_ERRNO_PORT`.
 * 3. If `port_sampling->direction` is not equal to `POK_PORT_DIRECTION_IN`:
 *    1. Return `POK_ERRNO_DIRECTION`.
 * 4. Call `jet_user_to_kernel()` with the arguments `data` and
 *    `port_sampling->channel->max_message_size`, and store the result in
 *    `k_data`.
 * 5. If `k_data` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 6. Set `k_len` to `jet_user_to_kernel_typed(len)`.
 * 7. If `k_len` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 8. Set `k_valid` to `jet_user_to_kernel_typed(valid)`.
 * 9. If `k_valid` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 10. Call `pok_preemption_local_disable()`.
 * 11. Call `pok_channel_sampling_r_get_message()` with the arguments
 *     `port_sampling->channel`, the address of `message_size` and the address
 *     of `ts`, and store the result in `message`.
 * 12. If `message` is not NULL:
 *     1. Call `memcpy()` with the arguments `k_data`, `message` and
 *        `message_size`.
 *     2. Set `*k_len` to `(pok_port_size_t)message_size`.
 *     3. Set `current_time` to `jet_system_time()`.
 *     4. Set `port_sampling->last_message_validity` to `TRUE` if
 *        `(current_time - ts)` is less than or equal to
 *        `port_sampling->refresh_period`, otherwise `FALSE`.
 *     5. Set `ret` to `POK_ERRNO_OK`.
 * 13. Otherwise:
 *     1. Set `*k_len` to `0`.
 *     2. Set `port_sampling->last_message_validity` to `FALSE`.
 *     3. Set `ret` to `POK_ERRNO_EMPTY`.
 * 14. Set `*k_valid` to `port_sampling->last_message_validity`.
 * 15. Call `pok_preemption_local_enable()`.
 * 16. Return `ret`.
 *
 * @calls
 * - `get_port_sampling()`
 * - `jet_user_to_kernel()`
 * - `pok_preemption_local_disable()`
 * - `pok_channel_sampling_r_get_message()`
 * - `memcpy()`
 * - `ja_system_time()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_port_sampling_read(
    pok_port_id_t           id,
    void* JET_USER            data,
    pok_port_size_t* JET_USER len,
    pok_bool_t* JET_USER      valid)
{
    pok_port_sampling_t* port_sampling;
    pok_ret_e ret;
    pok_time_t ts;

    const char* message;
    pok_message_size_t message_size;

    port_sampling = get_port_sampling(id);

    if(port_sampling == NULL) {
        return POK_ERRNO_PORT;
    }

    if(port_sampling->direction != POK_PORT_DIRECTION_IN) {
        return POK_ERRNO_DIRECTION;
    }

    void* JET_KUSER k_data = jet_user_to_kernel(data,
        port_sampling->channel->max_message_size);
    if(k_data == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_port_size_t* JET_KUSER k_len = jet_user_to_kernel_typed(len);
    if(k_len == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_bool_t* JET_KUSER k_valid = jet_user_to_kernel_typed(valid);
    if(k_valid == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_preemption_local_disable();

    message = pok_channel_sampling_r_get_message(port_sampling->channel,
        &message_size, &ts);

    if(message != NULL)
    {
        (void) memcpy(k_data, message, message_size);
        *k_len = (pok_port_size_t)message_size;

        pok_time_t current_time = jet_system_time();
        /*
         * Message age is compared with the refresh period instead of
         * computing ts + refresh_period, which overflows for large
         * periods. ts is never later than current_time.
         */
        port_sampling->last_message_validity =
            ((current_time - ts) <= port_sampling->refresh_period)
            ? TRUE: FALSE;
        ret = POK_ERRNO_OK;
    }
    else
    {
        *k_len = 0;
        port_sampling->last_message_validity = FALSE;
        ret = POK_ERRNO_EMPTY;
    }

    *k_valid = port_sampling->last_message_validity;

    pok_preemption_local_enable();

    return ret;
}

/**
 * @funcname `pok_port_sampling_id`
 *
 * @description
 * Gives the ID of a created sampling port by its name (GET_SAMPLING_PORT_ID).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `name` | `const char * JET_USER` | [in] Partition address of the name. |
 * | `id` | `pok_port_id_t * JET_USER` | [out] Partition address of the result. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_EFAULT for a bad address;
 *         POK_ERRNO_UNAVAILABLE if there is no such created port.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_id` | `pok_port_id_t *` | Kernel address of the result |
 * | `k_name` | `const char *` | Kernel address of the name |
 * | `port_sampling` | `pok_port_sampling_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 * 2. If `k_name` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 * 4. If `k_id` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `find_port_sampling()` with the argument `k_name`, and store the
 *    result in `port_sampling`.
 * 6. If `port_sampling` is NULL:
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 7. If `port_sampling->is_created` is false:
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 8. Set `*k_id` to `port_sampling - current_partition_arinc->ports_sampling`.
 * 9. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `jet_user_to_kernel_ro()`
 * - `jet_user_to_kernel()`
 * - `find_port_sampling()`
 */
pok_ret_e pok_port_sampling_id(
    const char* JET_USER     name,
    pok_port_id_t* JET_USER  id
)
{
    pok_port_sampling_t* port_sampling;

    const char* JET_KUSER k_name = jet_user_to_kernel_ro(name, MAX_NAME_LENGTH);
    if(k_name == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_port_id_t* JET_KUSER k_id = jet_user_to_kernel_typed(id);
    if(k_id == NULL) {
        return POK_ERRNO_EFAULT;
    }

    port_sampling = find_port_sampling(k_name);

    if(port_sampling == NULL) {
        return POK_ERRNO_UNAVAILABLE;
    }
    if(!port_sampling->is_created) {
        return POK_ERRNO_UNAVAILABLE;
    }

    *k_id = port_sampling - current_partition_arinc->ports_sampling;

    return POK_ERRNO_OK;
}

/**
 * @funcname `pok_port_sampling_status`
 *
 * @description
 * Gives the status of a sampling port (GET_SAMPLING_PORT_STATUS): size,
 * direction, refresh period and validity of the last message read.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 * | `status` | `pok_port_sampling_status_t * JET_USER` | [out] Partition address of the status. |
 *
 * @return POK_ERRNO_OK; POK_ERRNO_PORT for a bad ID; POK_ERRNO_EFAULT for a bad
 *         address.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `k_status` | `pok_port_sampling_status_t *` | Kernel address of the status |
 * | `port_sampling` | `pok_port_sampling_t *` | The port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_port_sampling()` with the argument `id`, and store the result in
 *    `port_sampling`.
 * 2. If `port_sampling` is NULL:
 *    1. Return `POK_ERRNO_PORT`.
 * 3. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 * 4. If `k_status` is NULL:
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `pok_preemption_local_disable()`.
 * 6. Set `k_status->size` to `port_sampling->channel->max_message_size`.
 * 7. Set `k_status->direction` to `port_sampling->direction`.
 * 8. Set `k_status->refresh` to `port_sampling->refresh_period`.
 * 9. Set `k_status->validity` to `port_sampling->last_message_validity`.
 * 10. Call `pok_preemption_local_enable()`.
 * 11. Return `POK_ERRNO_OK`.
 *
 * @calls
 * - `get_port_sampling()`
 * - `jet_user_to_kernel()`
 * - `pok_preemption_local_disable()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_port_sampling_status (
    pok_port_id_t                       id,
    pok_port_sampling_status_t* JET_USER  status
)
{
    pok_port_sampling_t* port_sampling;

    port_sampling = get_port_sampling(id);

    if(port_sampling == NULL) {
        return POK_ERRNO_PORT;
    }

    pok_port_sampling_status_t* JET_KUSER k_status =
        jet_user_to_kernel_typed(status);
    if(k_status == NULL) {
        return POK_ERRNO_EFAULT;
    }

    pok_preemption_local_disable();

    k_status->size = port_sampling->channel->max_message_size;
    k_status->direction = port_sampling->direction;
    k_status->refresh = port_sampling->refresh_period;
    k_status->validity = port_sampling->last_message_validity;

    pok_preemption_local_enable();

    return POK_ERRNO_OK;
}

// TODO: This should be transformed into READ_UPDATED_SAMPLING_MESSAGE
// eventually.

/**
 * @funcname `pok_port_sampling_check`
 *
 * @description
 * Tells whether a sampling IN port has a message not read yet.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `id` | `pok_port_id_t` | [in] The port ID. |
 *
 * @return POK_ERRNO_OK if it has, POK_ERRNO_EMPTY if not; POK_ERRNO_PORT for a
 *         bad ID; POK_ERRNO_MODE for an OUT port.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `port_sampling` | `pok_port_sampling_t *` | The port |
 * | `ret` | `pok_ret_e` | The result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `get_port_sampling()` with the argument `id`, and store the result in
 *    `port_sampling`.
 * 2. If `port_sampling` is NULL:
 *    1. Return `POK_ERRNO_PORT`.
 * 3. If `port_sampling->direction` is not equal to `POK_PORT_DIRECTION_IN`:
 *    1. Return `POK_ERRNO_MODE`.
 * 4. Call `pok_preemption_local_disable()`.
 * 5. Set `ret` to `POK_ERRNO_OK` if
 *    `pok_channel_sampling_r_check_new_message(port_sampling->channel)` is
 *    true, otherwise `POK_ERRNO_EMPTY`.
 * 6. Call `pok_preemption_local_enable()`.
 * 7. Return `ret`.
 *
 * @calls
 * - `get_port_sampling()`
 * - `pok_preemption_local_disable()`
 * - `pok_channel_sampling_r_check_new_message()`
 * - `pok_preemption_local_enable()`
 */
pok_ret_e pok_port_sampling_check(pok_port_id_t id)
{
    pok_ret_e ret;

    pok_port_sampling_t* port_sampling;

    port_sampling = get_port_sampling(id);

    if(port_sampling == NULL) {
        return POK_ERRNO_PORT;
    }

    if(port_sampling->direction != POK_PORT_DIRECTION_IN) {
        return POK_ERRNO_MODE;
    }


    pok_preemption_local_disable();
    ret = pok_channel_sampling_r_check_new_message(port_sampling->channel)
        ? POK_ERRNO_OK
        : POK_ERRNO_EMPTY;
    pok_preemption_local_enable();

    return ret;
}
