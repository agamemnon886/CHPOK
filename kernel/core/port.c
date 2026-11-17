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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
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
 * - \hlrid{HLR_PORT_FIND_PORT_QUEUING_01}: `find_port_queuing()` shall find a
 *   queuing port of the configuration of the current partition by its name,
 *   without regard to case.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_002}
 *   - \srqref{SRQ_JET_IPC_017}
 *   - \srqref{SRQ_JET_CFG_004}
 *
 * @llr
 * - \llrid{LLR_PORT_FIND_PORT_QUEUING_01}: `find_port_queuing()` shall copy the
 *   `MAX_NAME_LENGTH` bytes at `k_name` to a kernel buffer with `memcpy()`
 *   before it compares them.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_FIND_PORT_QUEUING_01}
 * - \llrid{LLR_PORT_FIND_PORT_QUEUING_02}: If `pok_compare_names()` returns 0
 *   for `name` of one of the `nports_queuing` elements of `ports_queuing` of
 *   the partition and the buffer, `find_port_queuing()` shall return the
 *   address of the first such port.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_FIND_PORT_QUEUING_01}
 * - \llrid{LLR_PORT_FIND_PORT_QUEUING_03}: If `pok_compare_names()` returns a
 *   value other than 0 for each of them, `find_port_queuing()` shall return
 *   NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_FIND_PORT_QUEUING_01}
 *
 * @design
 * 1. Set `port_queuing` to `current_partition_arinc->ports_queuing`.
 *    \llrref{LLR_PORT_FIND_PORT_QUEUING_02},
 *    \llrref{LLR_PORT_FIND_PORT_QUEUING_03}
 * 2. Set `ports_queuing_end` to
 *    `port_queuing + current_partition_arinc->nports_queuing`.
 *    \llrref{LLR_PORT_FIND_PORT_QUEUING_02},
 *    \llrref{LLR_PORT_FIND_PORT_QUEUING_03}
 * 3. Call `memcpy()` with the arguments `kernel_name`, `k_name` and
 *    `MAX_NAME_LENGTH`. \llrref{LLR_PORT_FIND_PORT_QUEUING_01}
 * 4. While `port_queuing` is less than `ports_queuing_end`, repeat (after each
 *    pass: increment `port_queuing`): \llrref{LLR_PORT_FIND_PORT_QUEUING_02},
 *    \llrref{LLR_PORT_FIND_PORT_QUEUING_03}
 *    1. If `pok_compare_names(port_queuing->name, kernel_name)` is equal to
 *       `0`: \llrref{LLR_PORT_FIND_PORT_QUEUING_02}
 *       1. Return `port_queuing`.
 * 5. Return `NULL`. \llrref{LLR_PORT_FIND_PORT_QUEUING_03}
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
 * - \hlrid{HLR_PORT_GET_PORT_QUEUING_01}: `get_port_queuing()` shall give a
 *   created queuing port of the current partition by its identifier, and none
 *   for an identifier of no port of the partition or of a port not created.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_005}
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_PORT_GET_PORT_QUEUING_01}: If `id` is not less than
 *   `nports_queuing` of the partition, `get_port_queuing()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_GET_PORT_QUEUING_01}
 * - \llrid{LLR_PORT_GET_PORT_QUEUING_02}: If `id` is less than `nports_queuing`
 *   and `is_created` of `ports_queuing[id]` is true, `get_port_queuing()` shall
 *   return its address.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_GET_PORT_QUEUING_01}
 * - \llrid{LLR_PORT_GET_PORT_QUEUING_03}: If `id` is less than `nports_queuing`
 *   and `is_created` of `ports_queuing[id]` is false, `get_port_queuing()`
 *   shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_GET_PORT_QUEUING_01}
 *
 * @design
 * 1. Set `part` to `current_partition_arinc`.
 *    \llrref{LLR_PORT_GET_PORT_QUEUING_01},
 *    \llrref{LLR_PORT_GET_PORT_QUEUING_02},
 *    \llrref{LLR_PORT_GET_PORT_QUEUING_03}
 * 2. If `id` is greater than or equal to `part->nports_queuing`:
 *    \llrref{LLR_PORT_GET_PORT_QUEUING_01}
 *    1. Return `NULL`.
 * 3. Set `port_queuing` to the address of `part->ports_queuing[id]`.
 *    \llrref{LLR_PORT_GET_PORT_QUEUING_02},
 *    \llrref{LLR_PORT_GET_PORT_QUEUING_03}
 * 4. Return `port_queuing` if `port_queuing->is_created` is true, otherwise
 *    `NULL`. \llrref{LLR_PORT_GET_PORT_QUEUING_02},
 *    \llrref{LLR_PORT_GET_PORT_QUEUING_03}
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
 * - \hlrid{HLR_PORT_PORT_QUEUING_RECEIVE_01}: `port_queuing_receive()` shall
 *   give a process the first message of the channel of a queuing destination
 *   port, removing it from the queue.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_009}
 *   - \srqref{SRQ_JET_IPC_011}
 *   - \srqref{SRQ_JET_IPC_021}
 * - \hlrid{HLR_PORT_PORT_QUEUING_RECEIVE_02}: `port_queuing_receive()` shall
 *   tell the process that messages were lost if the channel discarded messages.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_022}
 *
 * @llr
 * - \llrid{LLR_PORT_PORT_QUEUING_RECEIVE_01}: `port_queuing_receive()` shall
 *   call `pok_channel_queuing_r_get_message()` once with `channel` of `port`
 *   and `FALSE`, copy the message of the size that it gives to
 *   `wait_buffer.dest` of `t` with `memcpy()`, set `wait_len` of `t` to that
 *   size, and call `pok_channel_queuing_r_consume_message()` once with the
 *   channel.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_PORT_QUEUING_RECEIVE_01}
 * - \llrid{LLR_PORT_PORT_QUEUING_RECEIVE_02}: If
 *   `pok_channel_queuing_r_consume_message()` reports a discarded message,
 *   `port_queuing_receive()` shall set `wait_result` of `t` to
 *   `POK_ERRNO_TOOMANY`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_PORT_QUEUING_RECEIVE_02}
 * - \llrid{LLR_PORT_PORT_QUEUING_RECEIVE_03}: If
 *   `pok_channel_queuing_r_consume_message()` reports no discarded message,
 *   `port_queuing_receive()` shall set `wait_result` of `t` to `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_PORT_QUEUING_RECEIVE_02}
 *
 * @design
 * 1. Call `pok_channel_queuing_r_get_message()` with the arguments
 *    `port->channel`, the address of `message_size` and `FALSE`, and store the
 *    result in `message`. \llrref{LLR_PORT_PORT_QUEUING_RECEIVE_01}
 * 2. Call `memcpy()` with the arguments `t->wait_buffer.dest`, `message` and
 *    `message_size`. \llrref{LLR_PORT_PORT_QUEUING_RECEIVE_01}
 * 3. Set `t->wait_len` to `message_size`.
 *    \llrref{LLR_PORT_PORT_QUEUING_RECEIVE_01}
 * 4. Call `pok_channel_queuing_r_consume_message()` with the arguments
 *    `port->channel` and the address of `message_discarded`.
 *    \llrref{LLR_PORT_PORT_QUEUING_RECEIVE_01}
 * 5. Set `t->wait_result` to `POK_ERRNO_TOOMANY` if `message_discarded` is
 *    true, otherwise `POK_ERRNO_OK`. \llrref{LLR_PORT_PORT_QUEUING_RECEIVE_02},
 *    \llrref{LLR_PORT_PORT_QUEUING_RECEIVE_03}
 *
 * @calls
 * - `pok_channel_queuing_r_get_message()`
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
 * - \hlrid{HLR_PORT_PORT_QUEUING_SEND_01}: `port_queuing_send()` shall put the
 *   message of a process at the end of the queue of the channel of a queuing
 *   source port, which has room.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_009}
 *   - \srqref{SRQ_JET_IPC_010}
 *   - \srqref{SRQ_JET_IPC_020}
 *
 * @llr
 * - \llrid{LLR_PORT_PORT_QUEUING_SEND_01}: `port_queuing_send()` shall call
 *   `pok_channel_queuing_s_get_message()` once with `channel` of `port` and
 *   `FALSE`, copy `wait_len` bytes of `wait_buffer.src` of `t` to the message
 *   that it gives with `memcpy()`, call
 *   `pok_channel_queuing_s_produce_message()` once with the channel and
 *   `wait_len`, and set `wait_result` of `t` to `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_PORT_QUEUING_SEND_01}
 *
 * @design
 * 1. Call `pok_channel_queuing_s_get_message()` with the arguments
 *    `port->channel` and `FALSE`, and store the result in `message`.
 *    \llrref{LLR_PORT_PORT_QUEUING_SEND_01}
 * 2. Call `memcpy()` with the arguments `message`, `t->wait_buffer.src` and
 *    `t->wait_len`. \llrref{LLR_PORT_PORT_QUEUING_SEND_01}
 * 3. Call `pok_channel_queuing_s_produce_message()` with the arguments
 *    `port->channel` and `t->wait_len`. \llrref{LLR_PORT_PORT_QUEUING_SEND_01}
 * 4. Set `t->wait_result` to `POK_ERRNO_OK`.
 *    \llrref{LLR_PORT_PORT_QUEUING_SEND_01}
 *
 * @calls
 * - `pok_channel_queuing_s_get_message()`
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
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_INIT_01}: `pok_port_queuing_init()` shall
 *   make a queuing port one that is not created and that no process waits on.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_004}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_INIT_01}: `pok_port_queuing_init()` shall
 *   call `pok_thread_wq_init()` once with the address of `waiters` of
 *   `port_queuing` and set its `is_created` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_INIT_01}
 *
 * @design
 * 1. Call `pok_thread_wq_init()` with the address of `port_queuing->waiters`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_INIT_01}
 * 2. Set `port_queuing->is_created` to `FALSE`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_INIT_01}
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
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_CREATE_01}: `pok_port_queuing_create()`
 *   shall create a queuing port of the current partition outside `NORMAL` mode,
 *   if its name is that of a queuing port of the partition in the
 *   configuration, not yet created, with the configured maximum message size,
 *   direction and maximum number of messages, and a discipline of FIFO or
 *   priority; and give its identifier.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_002}
 *   - \srqref{SRQ_JET_IPC_017}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_CREATE_02}: `pok_port_queuing_create()`
 *   shall record the discipline of the port for the order of its waiting
 *   processes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_012}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_CREATE_03}: `pok_port_queuing_create()`
 *   shall refuse a name or an identifier address that the partition may not
 *   access with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_01}: If `jet_user_to_kernel_ro()`
 *   returns NULL for `name` and `MAX_NAME_LENGTH`, `pok_port_queuing_create()`
 *   shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_03}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_02}: If the name is readable and
 *   `jet_user_to_kernel_typed()` returns NULL for `id`,
 *   `pok_port_queuing_create()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_03}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_03}: If both conversions give an
 *   address and `find_port_queuing()` returns NULL, `pok_port_queuing_create()`
 *   shall return `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_04}: If `find_port_queuing()`
 *   returns a port whose `is_created` is true, `pok_port_queuing_create()`
 *   shall return `POK_ERRNO_EXISTS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_05}: If the port found is not
 *   created and `message_size` is not `max_message_size` of its channel,
 *   `pok_port_queuing_create()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_06}: If `message_size` is
 *   `max_message_size` of the channel and `direction` is not `direction` of the
 *   port, `pok_port_queuing_create()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_07}: If `direction` is that of the
 *   port and `discipline` is neither `POK_QUEUING_DISCIPLINE_FIFO` nor
 *   `POK_QUEUING_DISCIPLINE_PRIORITY`, `pok_port_queuing_create()` shall return
 *   `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_08}: If the discipline is valid,
 *   `direction` is `POK_PORT_DIRECTION_IN` and `max_nb_message` is not
 *   `recv.max_nb_message` of the channel, `pok_port_queuing_create()` shall
 *   return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_09}: If the discipline is valid,
 *   `direction` is not `POK_PORT_DIRECTION_IN` and `max_nb_message` is not
 *   `send.max_nb_message` of the channel, `pok_port_queuing_create()` shall
 *   return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_10}: If all these checks pass and
 *   `mode` of the partition is `POK_PARTITION_MODE_NORMAL`,
 *   `pok_port_queuing_create()` shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_11}: If all these checks pass,
 *   `mode` is not `POK_PARTITION_MODE_NORMAL` and `direction` is
 *   `POK_PORT_DIRECTION_IN`, `pok_port_queuing_create()` shall set `is_created`
 *   of the port to `TRUE` and its `discipline` to `discipline`, call
 *   `pok_channel_queuing_side_init()` with the channel, its `recv` and the
 *   index of the port in `ports_queuing`, store that index at `id`, and return
 *   `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_02}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_12}: If all these checks pass,
 *   `mode` is not `POK_PARTITION_MODE_NORMAL` and `direction` is not
 *   `POK_PORT_DIRECTION_IN`, `pok_port_queuing_create()` shall do the same with
 *   `send` of the channel in place of `recv`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_01}
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_02}
 *
 * @design
 * 1. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_01}
 * 2. If `k_name` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_02}
 * 4. If `k_id` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `find_port_queuing()` with the argument `k_name`, and store the
 *    result in `port_queuing`. \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_03}
 * 6. If `port_queuing` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_03}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 7. If `port_queuing->is_created` is true:
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_04}
 *    1. Return `POK_ERRNO_EXISTS`.
 * 8. If `message_size` is not equal to
 *    `port_queuing->channel->max_message_size`:
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_05}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 9. If `direction` is not equal to `port_queuing->direction`:
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_06}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 10. If `discipline` is not equal to `POK_QUEUING_DISCIPLINE_FIFO` and
 *     `discipline` is not equal to `POK_QUEUING_DISCIPLINE_PRIORITY`:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_07}
 *     1. Return `POK_ERRNO_EINVAL`.
 * 11. If `direction` is equal to `POK_PORT_DIRECTION_IN`:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_08}
 *     1. If `max_nb_message` is not equal to
 *        `port_queuing->channel->recv.max_nb_message`:
 *        1. Return `POK_ERRNO_EINVAL`.
 * 12. Otherwise: \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_09}
 *     1. If `max_nb_message` is not equal to
 *        `port_queuing->channel->send.max_nb_message`:
 *        1. Return `POK_ERRNO_EINVAL`.
 * 13. If `current_partition_arinc->mode` is equal to
 *     `POK_PARTITION_MODE_NORMAL`: \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_10}
 *     1. Return `POK_ERRNO_MODE`.
 * 14. Set `port_queuing->is_created` to `TRUE`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_11},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_12}
 * 15. Set `port_queuing->discipline` to `discipline`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_11},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_12}
 * 16. If `direction` is equal to `POK_PORT_DIRECTION_IN`:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_11}
 *     1. Call `pok_channel_queuing_side_init()` with the arguments
 *        `port_queuing->channel`, the address of `port_queuing->channel->recv`
 *        and `port_queuing - current_partition_arinc->ports_queuing`.
 * 17. Otherwise: \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_12}
 *     1. Call `pok_channel_queuing_side_init()` with the arguments
 *        `port_queuing->channel`, the address of `port_queuing->channel->send`
 *        and `port_queuing - current_partition_arinc->ports_queuing`.
 * 18. Set `*k_id` to `port_queuing - current_partition_arinc->ports_queuing`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_11},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_12}
 * 19. Return `POK_ERRNO_OK`. \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_11},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_12}
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
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_01}:
 *   `pok_port_queuing_create_packed()` shall create a queuing port with the
 *   attributes given in one structure, as `pok_port_queuing_create()` does.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_002}
 *   - \srqref{SRQ_JET_IPC_017}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_02}:
 *   `pok_port_queuing_create_packed()` shall refuse an attribute address that
 *   the partition may not read with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_01}: If
 *   `jet_user_to_kernel_typed_ro()` returns NULL for `arg`,
 *   `pok_port_queuing_create_packed()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_02}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_02}: If
 *   `jet_user_to_kernel_typed_ro()` returns an address for `arg`,
 *   `pok_port_queuing_create_packed()` shall return the result of
 *   `pok_port_queuing_create()` for `name`, `message_size`, `max_nb_message`,
 *   `direction` and `discipline` of `arg`, and `id`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_01}
 *
 * @design
 * 1. Set `k_arg` to `jet_user_to_kernel_typed_ro(arg)`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_01},
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_02}
 * 2. If `k_arg` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Call `pok_port_queuing_create()` with the arguments `name`,
 *    `k_arg->message_size`, `k_arg->max_nb_message`, `k_arg->direction`,
 *    `k_arg->discipline` and `id`, and return its result.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CREATE_PACKED_02}
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
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_RECEIVE_01}: `pok_port_queuing_receive()`
 *   shall give the current process the first message of a created queuing
 *   destination port at once if there is one and no process waits before it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_011}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_RECEIVE_02}: `pok_port_queuing_receive()`
 *   shall make the current process wait, up to its time-out, when no message
 *   can be given to it at once, and give the result of the wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_021}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_RECEIVE_03}: `pok_port_queuing_receive()`
 *   shall queue a waiting process in the discipline of the port.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_012}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_RECEIVE_04}: `pok_port_queuing_receive()`
 *   shall refuse an identifier of no created port, a source port, a wait where
 *   the process may not wait, and give an empty result without waiting for a
 *   time-out of 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_007}
 *   - \srqref{SRQ_JET_SYS_005}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_RECEIVE_05}: `pok_port_queuing_receive()`
 *   shall refuse buffer, length or time-out addresses that the partition may
 *   not access with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_01}: If `get_port_queuing()`
 *   returns NULL for `id`, `pok_port_queuing_receive()` shall return
 *   `POK_ERRNO_PORT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_02}: If the port exists and
 *   `jet_user_to_kernel()` returns NULL for `data` and `max_message_size` of
 *   its channel, `pok_port_queuing_receive()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_05}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_03}: If `data` is writable and
 *   `jet_user_to_kernel_typed()` returns NULL for `len`,
 *   `pok_port_queuing_receive()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_05}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_04}: If `len` is writable and
 *   `jet_user_to_kernel_typed_ro()` returns NULL for `timeout`,
 *   `pok_port_queuing_receive()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_05}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_05}: If these checks pass and
 *   `direction` of the port is not `POK_PORT_DIRECTION_IN`,
 *   `pok_port_queuing_receive()` shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_06}: If `direction` is
 *   `POK_PORT_DIRECTION_IN`, `pok_port_queuing_receive()` shall call
 *   `pok_preemption_local_disable()` before it reads the port and
 *   `pok_preemption_local_enable()` before it returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_07}: If no message is available
 *   (`pok_thread_wq_is_empty()` returns false for `waiters` of the port, or
 *   `pok_channel_queuing_r_get_message()` returns NULL for its channel, with
 *   `subscribe` true if and only if the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns true) and the time-out (the value at
 *   `timeout`) is 0, `pok_port_queuing_receive()` shall store 0 at `len` and
 *   return `POK_ERRNO_EMPTY`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_08}: If no message is available
 *   (`pok_thread_wq_is_empty()` returns false for `waiters` of the port, or
 *   `pok_channel_queuing_r_get_message()` returns NULL for its channel, with
 *   `subscribe` true if and only if the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns true), the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns false, `pok_port_queuing_receive()`
 *   shall store 0 at `len` and return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09}: If no message is available
 *   (`pok_thread_wq_is_empty()` returns false for `waiters` of the port, or
 *   `pok_channel_queuing_r_get_message()` returns NULL for its channel, with
 *   `subscribe` true if and only if the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns true), the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns true, `pok_port_queuing_receive()`
 *   shall set `wait_buffer.dest` of the current thread to the kernel address of
 *   `data`, call `pok_thread_wq_add_common()` with the waiters of the port, the
 *   thread and `discipline` of the port, and then `thread_wait_common()` with
 *   the thread and the time-out.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_02}
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_03}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_10}: If `pok_thread_wq_is_empty()`
 *   returns true for the waiters and `pok_channel_queuing_r_get_message()`
 *   returns a message, `pok_port_queuing_receive()` shall set
 *   `wait_buffer.dest` of the current thread to the kernel address of `data`
 *   and call `port_queuing_receive()` with the port and the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_RECEIVE_11}: After the wait or the
 *   receive, `pok_port_queuing_receive()` shall store at `len` `wait_len` of
 *   the thread if its `wait_result` is `POK_ERRNO_OK` or `POK_ERRNO_TOOMANY`,
 *   and 0 if it is another value, and return `wait_result`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_01}
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_RECEIVE_02}
 *
 * @design
 * 1. Call `get_port_queuing()` with the argument `id`, and store the result in
 *    `port_queuing`. \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_01},
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_02},
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_05}
 * 2. If `port_queuing` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_01}
 *    1. Return `POK_ERRNO_PORT`.
 * 3. Call `jet_user_to_kernel()` with the arguments `data` and
 *    `port_queuing->channel->max_message_size`, and store the result in
 *    `k_data`. \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_02}
 * 4. If `k_data` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Set `k_len` to `jet_user_to_kernel_typed(len)`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_03}
 * 6. If `k_len` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_03}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 7. Set `k_timeout` to `jet_user_to_kernel_typed_ro(timeout)`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_04}
 * 8. If `k_timeout` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_04}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 9. Set `kernel_timeout` to the value at `k_timeout`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_07},
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_08},
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09}
 * 10. If `port_queuing->direction` is not equal to `POK_PORT_DIRECTION_IN`:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_05}
 *     1. Return `POK_ERRNO_MODE`.
 * 11. Call `pok_preemption_local_disable()`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_06}
 * 12. Set `t` to `current_thread`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_10},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_11}
 * 13. If `kernel_timeout` is equal to `0`:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_07}
 *     1. Set `ret` to `POK_ERRNO_EMPTY`.
 * 14. Otherwise, if `thread_is_waiting_allowed()` is false:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_08}
 *     1. Set `ret` to `POK_ERRNO_MODE`.
 * 15. Otherwise: \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09}
 *     1. Set `ret` to `POK_ERRNO_OK`.
 * 16. If `pok_thread_wq_is_empty(&port_queuing->waiters)` is false or
 *     `(pok_channel_queuing_r_get_message(port_queuing->channel,`
 *     `&message_size, ret == POK_ERRNO_OK))` is NULL:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_07},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_08},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_10}
 *     1. If `ret` is not equal to `POK_ERRNO_OK`:
 *        \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_07},
 *        \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_08}
 *        1. Set `*k_len` to `0`.
 *        2. Go to the label `err`.
 *     2. Set `t->wait_buffer.dest` to `k_data`.
 *        \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09}
 *     3. Call `pok_thread_wq_add_common()` with the arguments the address of
 *        `port_queuing->waiters`, `t` and `port_queuing->discipline`.
 *        \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09}
 *     4. Call `thread_wait_common()` with the arguments `t` and
 *        `kernel_timeout`. \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09}
 *     5. Go to the label `out`. \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09}
 * 17. Set `t->wait_buffer.dest` to `k_data`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_10}
 * 18. Call `port_queuing_receive()` with the arguments `port_queuing` and `t`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_10}
 * 19. Label `out`: \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_09},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_10},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_11}
 * 20. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_06}
 * 21. Set `*k_len` to `t->wait_len` if (`t->wait_result` is equal to
 *     `POK_ERRNO_OK` or `t->wait_result` is equal to `POK_ERRNO_TOOMANY`),
 *     otherwise `0U`. \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_11}
 * 22. Return `t->wait_result`. \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_11}
 * 23. Label `err`: \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_07},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_08}
 * 24. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_06}
 * 25. Return `ret`. \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_07},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_RECEIVE_08}
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
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_SEND_01}: `pok_port_queuing_send()` shall
 *   put the message of the current process into a created queuing source port
 *   at once if there is room and no process waits before it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_010}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_SEND_02}: `pok_port_queuing_send()` shall
 *   make the current process wait, up to its time-out, when its message cannot
 *   be put into the port at once, and give the result of the wait.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_020}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_SEND_03}: `pok_port_queuing_send()` shall
 *   queue a waiting process in the discipline of the port.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_012}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_SEND_04}: `pok_port_queuing_send()` shall
 *   refuse an identifier of no created port, a destination port, a length of 0
 *   or above the maximum message size, a wait where the process may not wait,
 *   and report a full queue without waiting for a time-out of 0.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_THR_007}
 *   - \srqref{SRQ_JET_SYS_005}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_SEND_05}: `pok_port_queuing_send()` shall
 *   refuse message or time-out addresses that the partition may not read with
 *   `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_01}: If `get_port_queuing()` returns
 *   NULL for `id`, `pok_port_queuing_send()` shall return `POK_ERRNO_PORT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_02}: If the port exists and its
 *   `direction` is not `POK_PORT_DIRECTION_OUT`, `pok_port_queuing_send()`
 *   shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_03}: If `direction` is
 *   `POK_PORT_DIRECTION_OUT` and `len` is 0, `pok_port_queuing_send()` shall
 *   return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_04}: If `direction` is
 *   `POK_PORT_DIRECTION_OUT` and `len` is greater than `max_message_size` of
 *   the channel, `pok_port_queuing_send()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_05}: If these checks pass and
 *   `jet_user_to_kernel_ro()` returns NULL for `data` and `len`,
 *   `pok_port_queuing_send()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_05}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_06}: If `data` is readable and
 *   `jet_user_to_kernel_typed_ro()` returns NULL for `timeout`,
 *   `pok_port_queuing_send()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_05}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_07}: If these checks pass,
 *   `pok_port_queuing_send()` shall call `pok_preemption_local_disable()`
 *   before it reads the port and `pok_preemption_local_enable()` before it
 *   returns.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_08}: If there is no room
 *   (`pok_thread_wq_is_empty()` returns false for `waiters` of the port, or
 *   `pok_channel_queuing_s_get_message()` returns NULL for its channel, with
 *   `subscribe` true if and only if the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns true) and the time-out (the value at
 *   `timeout`) is 0, `pok_port_queuing_send()` shall return `POK_ERRNO_FULL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_09}: If there is no room
 *   (`pok_thread_wq_is_empty()` returns false for `waiters` of the port, or
 *   `pok_channel_queuing_s_get_message()` returns NULL for its channel, with
 *   `subscribe` true if and only if the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns true), the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns false, `pok_port_queuing_send()`
 *   shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_04}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_10}: If there is no room
 *   (`pok_thread_wq_is_empty()` returns false for `waiters` of the port, or
 *   `pok_channel_queuing_s_get_message()` returns NULL for its channel, with
 *   `subscribe` true if and only if the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns true), the time-out is not 0 and
 *   `thread_is_waiting_allowed()` returns true, `pok_port_queuing_send()` shall
 *   set `wait_len` of the current thread to `len` and its `wait_buffer.src` to
 *   the kernel address of `data`, call `pok_thread_wq_add_common()` with the
 *   waiters of the port, the thread and `discipline` of the port, then
 *   `thread_wait_common()` with the thread and the time-out, and return
 *   `wait_result` of the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_02}
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_03}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_SEND_11}: If `pok_thread_wq_is_empty()`
 *   returns true for the waiters and `pok_channel_queuing_s_get_message()`
 *   returns a message, `pok_port_queuing_send()` shall set `wait_len` of the
 *   current thread to `len` and its `wait_buffer.src` to the kernel address of
 *   `data`, call `port_queuing_send()` with the port and the thread, and return
 *   `wait_result` of the thread.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_SEND_01}
 *
 * @design
 * 1. Set `t` to `current_thread`. \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10},
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_11}
 * 2. Call `get_port_queuing()` with the argument `id`, and store the result in
 *    `port_queuing`. \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_01}
 * 3. If `port_queuing` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_01}
 *    1. Return `POK_ERRNO_PORT`.
 * 4. If `port_queuing->direction` is not equal to `POK_PORT_DIRECTION_OUT`:
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_02}
 *    1. Return `POK_ERRNO_MODE`.
 * 5. If `len` is equal to `0U`: \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_03}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 6. If `len` is greater than `port_queuing->channel->max_message_size`:
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_04}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 7. Call `jet_user_to_kernel_ro()` with the arguments `data` and `len`, and
 *    store the result in `k_data`. \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_05}
 * 8. If `k_data` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_05}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 9. Set `k_timeout` to `jet_user_to_kernel_typed_ro(timeout)`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_06}
 * 10. If `k_timeout` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_06}
 *     1. Return `POK_ERRNO_EFAULT`.
 * 11. Set `kernel_timeout` to the value at `k_timeout`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_08},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_09},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10}
 * 12. Call `pok_preemption_local_disable()`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_07}
 * 13. If `kernel_timeout` is equal to `0`:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_08}
 *     1. Set `ret` to `POK_ERRNO_FULL`.
 * 14. Otherwise, if `thread_is_waiting_allowed()` is false:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_09}
 *     1. Set `ret` to `POK_ERRNO_MODE`.
 * 15. Otherwise: \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10}
 *     1. Set `ret` to `POK_ERRNO_OK`.
 * 16. If `pok_thread_wq_is_empty(&port_queuing->waiters)` is false or
 *     `pok_channel_queuing_s_get_message(port_queuing->channel, ret ==`
 *     `POK_ERRNO_OK)` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_08},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_09},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10}
 *     1. If `ret` is not equal to `POK_ERRNO_OK`:
 *        \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_08},
 *        \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_09}
 *        1. Go to the label `err`.
 *     2. Set `t->wait_len` to `len`. \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10}
 *     3. Set `t->wait_buffer.src` to `k_data`.
 *        \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10}
 *     4. Call `pok_thread_wq_add_common()` with the arguments the address of
 *        `port_queuing->waiters`, `t` and `port_queuing->discipline`.
 *        \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10}
 *     5. Call `thread_wait_common()` with the arguments `t` and
 *        `kernel_timeout`. \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10}
 *     6. Go to the label `out`. \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10}
 * 17. Set `t->wait_len` to `len`. \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_11}
 * 18. Set `t->wait_buffer.src` to `k_data`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_11}
 * 19. Call `port_queuing_send()` with the arguments `port_queuing` and `t`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_11}
 * 20. Label `out`: \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_11}
 * 21. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_07}
 * 22. Return `t->wait_result`. \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_10},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_11}
 * 23. Label `err`: \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_08},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_09}
 * 24. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_07}
 * 25. Return `ret`. \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_08},
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_SEND_09}
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
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_STATUS_01}: `pok_port_queuing_status()`
 *   shall give the maximum message size, the direction, the number of waiting
 *   processes, and the maximum and present number of messages of a created
 *   queuing port of the current partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_003}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_STATUS_02}: `pok_port_queuing_status()`
 *   shall refuse a status address that the partition may not write and an
 *   identifier of no created port.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_STATUS_01}: If
 *   `jet_user_to_kernel_typed()` returns NULL for `status`,
 *   `pok_port_queuing_status()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_STATUS_02}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_STATUS_02}: If `status` is writable and
 *   `get_port_queuing()` returns NULL for `id`, `pok_port_queuing_status()`
 *   shall return `POK_ERRNO_PORT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_STATUS_02}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_STATUS_03}: If both give an address,
 *   `pok_port_queuing_status()` shall set `max_message_size` of the status to
 *   that of the channel and `direction` to that of the port, then call
 *   `pok_preemption_local_disable()`, set `waiting_processes` to the result of
 *   `pok_thread_wq_get_nwaits()` for the waiters of the port, set the numbers
 *   of messages as below, call `pok_preemption_local_enable()` and return
 *   `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_STATUS_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_STATUS_04}: If `direction` of the port is
 *   `POK_PORT_DIRECTION_IN`, `pok_port_queuing_status()` shall set
 *   `max_nb_message` of the status to `recv.max_nb_message` of the channel and
 *   `nb_message` to the result of `pok_channel_queuing_r_n_messages()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_STATUS_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_STATUS_05}: If `direction` of the port is
 *   not `POK_PORT_DIRECTION_IN`, `pok_port_queuing_status()` shall set
 *   `max_nb_message` of the status to `send.max_nb_message` of the channel and
 *   `nb_message` to the result of `pok_channel_queuing_s_n_messages()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_STATUS_01}
 *
 * @design
 * 1. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_01}
 * 2. If `k_status` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Call `get_port_queuing()` with the argument `id`, and store the result in
 *    `port_queuing`. \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_02}
 * 4. If `port_queuing` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_02}
 *    1. Return `POK_ERRNO_PORT`.
 * 5. Set `channel` to `port_queuing->channel`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_03}
 * 6. Set `k_status->max_message_size` to `channel->max_message_size`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_03}
 * 7. Set `k_status->direction` to `port_queuing->direction`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_03}
 * 8. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_03}
 * 9. Call `pok_thread_wq_get_nwaits()` with the address of
 *    `port_queuing->waiters`, and store the result in
 *    `k_status->waiting_processes`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_03}
 * 10. If `port_queuing->direction` is equal to `POK_PORT_DIRECTION_IN`:
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_04}
 *     1. Set `k_status->max_nb_message` to `channel->recv.max_nb_message`.
 *     2. Call `pok_channel_queuing_r_n_messages()` with the argument `channel`,
 *        and store the result in `k_status->nb_message`.
 * 11. Otherwise: \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_05}
 *     1. Set `k_status->max_nb_message` to `channel->send.max_nb_message`.
 *     2. Call `pok_channel_queuing_s_n_messages()` with the argument `channel`,
 *        and store the result in `k_status->nb_message`.
 * 12. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_03}
 * 13. Return `POK_ERRNO_OK`. \llrref{LLR_PORT_POK_PORT_QUEUING_STATUS_03}
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
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_ID_01}: `pok_port_queuing_id()` shall give
 *   the identifier of a created queuing port of the current partition found by
 *   its name.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_003}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_ID_02}: `pok_port_queuing_id()` shall
 *   refuse a name or an identifier address that the partition may not access
 *   with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_ID_01}: If `jet_user_to_kernel_ro()`
 *   returns NULL for `name` and `MAX_NAME_LENGTH`, `pok_port_queuing_id()`
 *   shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_ID_02}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_ID_02}: If the name is readable and
 *   `jet_user_to_kernel_typed()` returns NULL for `id`, `pok_port_queuing_id()`
 *   shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_ID_02}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_ID_03}: If both conversions give an
 *   address and `find_port_queuing()` returns NULL, `pok_port_queuing_id()`
 *   shall return `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_ID_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_ID_04}: If `find_port_queuing()` returns a
 *   port whose `is_created` is false, `pok_port_queuing_id()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_ID_01}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_ID_05}: If `find_port_queuing()` returns a
 *   port whose `is_created` is true, `pok_port_queuing_id()` shall store its
 *   index in `ports_queuing` of the partition at `id` and return
 *   `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_ID_01}
 *
 * @design
 * 1. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_ID_01}
 * 2. If `k_name` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_ID_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_ID_02}
 * 4. If `k_id` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_ID_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `find_port_queuing()` with the argument `k_name`, and store the
 *    result in `port_queuing`. \llrref{LLR_PORT_POK_PORT_QUEUING_ID_03}
 * 6. If `port_queuing` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_ID_03}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 7. If `port_queuing->is_created` is false:
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_ID_04}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 8. Set `*k_id` to `port_queuing - current_partition_arinc->ports_queuing`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_ID_05}
 * 9. Return `POK_ERRNO_OK`. \llrref{LLR_PORT_POK_PORT_QUEUING_ID_05}
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
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_CLEAR_01}: `pok_port_queuing_clear()`
 *   shall discard the messages of a created queuing destination port of the
 *   current partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_015}
 * - \hlrid{HLR_PORT_POK_PORT_QUEUING_CLEAR_02}: `pok_port_queuing_clear()`
 *   shall refuse an identifier of no created port and a source port.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CLEAR_01}: If `get_port_queuing()` returns
 *   NULL for `id`, `pok_port_queuing_clear()` shall return `POK_ERRNO_PORT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CLEAR_02}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CLEAR_02}: If the port exists and its
 *   `direction` is not `POK_PORT_DIRECTION_IN`, `pok_port_queuing_clear()`
 *   shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CLEAR_02}
 * - \llrid{LLR_PORT_POK_PORT_QUEUING_CLEAR_03}: If `direction` is
 *   `POK_PORT_DIRECTION_IN`, `pok_port_queuing_clear()` shall call
 *   `pok_preemption_local_disable()`, `pok_channel_queuing_side_init()` with
 *   the channel, its `recv` and the index of the port, and
 *   `pok_preemption_local_enable()`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_QUEUING_CLEAR_01}
 *
 * @design
 * 1. Call `get_port_queuing()` with the argument `id`, and store the result in
 *    `port_queuing`. \llrref{LLR_PORT_POK_PORT_QUEUING_CLEAR_01}
 * 2. If `port_queuing` is NULL: \llrref{LLR_PORT_POK_PORT_QUEUING_CLEAR_01}
 *    1. Return `POK_ERRNO_PORT`.
 * 3. If `port_queuing->direction` is not equal to `POK_PORT_DIRECTION_IN`:
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CLEAR_02}
 *    1. Return `POK_ERRNO_MODE`.
 * 4. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CLEAR_03}
 * 5. Call `pok_channel_queuing_side_init()` with the arguments
 *    `port_queuing->channel`, the address of `port_queuing->channel->recv` and
 *    `port_queuing - current_partition_arinc->ports_queuing`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CLEAR_03}
 * 6. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_PORT_POK_PORT_QUEUING_CLEAR_03}
 * 7. Return `POK_ERRNO_OK`. \llrref{LLR_PORT_POK_PORT_QUEUING_CLEAR_03}
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
 * - \hlrid{HLR_PORT_FIND_PORT_SAMPLING_01}: `find_port_sampling()` shall find a
 *   sampling port of the configuration of the current partition by its name,
 *   without regard to case.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_002}
 *   - \srqref{SRQ_JET_IPC_017}
 *   - \srqref{SRQ_JET_CFG_004}
 *
 * @llr
 * - \llrid{LLR_PORT_FIND_PORT_SAMPLING_01}: `find_port_sampling()` shall copy
 *   the `MAX_NAME_LENGTH` bytes at `k_name` to a kernel buffer with `memcpy()`
 *   before it compares them.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_FIND_PORT_SAMPLING_01}
 * - \llrid{LLR_PORT_FIND_PORT_SAMPLING_02}: If `pok_compare_names()` returns 0
 *   for `name` of one of the `nports_sampling` elements of `ports_sampling` of
 *   the partition and the buffer, `find_port_sampling()` shall return the
 *   address of the first such port.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_FIND_PORT_SAMPLING_01}
 * - \llrid{LLR_PORT_FIND_PORT_SAMPLING_03}: If `pok_compare_names()` returns a
 *   value other than 0 for each of them, `find_port_sampling()` shall return
 *   NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_FIND_PORT_SAMPLING_01}
 *
 * @design
 * 1. Set `port_sampling` to `current_partition_arinc->ports_sampling`.
 *    \llrref{LLR_PORT_FIND_PORT_SAMPLING_02},
 *    \llrref{LLR_PORT_FIND_PORT_SAMPLING_03}
 * 2. Set `ports_sampling_end` to
 *    `port_sampling + current_partition_arinc->nports_sampling`.
 *    \llrref{LLR_PORT_FIND_PORT_SAMPLING_02},
 *    \llrref{LLR_PORT_FIND_PORT_SAMPLING_03}
 * 3. Call `memcpy()` with the arguments `kernel_name`, `k_name` and
 *    `MAX_NAME_LENGTH`. \llrref{LLR_PORT_FIND_PORT_SAMPLING_01}
 * 4. While `port_sampling` is less than `ports_sampling_end`, repeat (after
 *    each pass: increment `port_sampling`):
 *    \llrref{LLR_PORT_FIND_PORT_SAMPLING_02},
 *    \llrref{LLR_PORT_FIND_PORT_SAMPLING_03}
 *    1. If `pok_compare_names(port_sampling->name, kernel_name)` is equal to
 *       `0`: \llrref{LLR_PORT_FIND_PORT_SAMPLING_02}
 *       1. Return `port_sampling`.
 * 5. Return `NULL`. \llrref{LLR_PORT_FIND_PORT_SAMPLING_03}
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
 * - \hlrid{HLR_PORT_GET_PORT_SAMPLING_01}: `get_port_sampling()` shall give a
 *   created sampling port of the current partition by its identifier, and none
 *   for an identifier of no port of the partition or of a port not created.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_005}
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_PORT_GET_PORT_SAMPLING_01}: If `id` is not less than
 *   `nports_sampling` of the partition, `get_port_sampling()` shall return
 *   NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_GET_PORT_SAMPLING_01}
 * - \llrid{LLR_PORT_GET_PORT_SAMPLING_02}: If `id` is less than
 *   `nports_sampling` and `is_created` of `ports_sampling[id]` is true,
 *   `get_port_sampling()` shall return its address.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_GET_PORT_SAMPLING_01}
 * - \llrid{LLR_PORT_GET_PORT_SAMPLING_03}: If `id` is less than
 *   `nports_sampling` and `is_created` of `ports_sampling[id]` is false,
 *   `get_port_sampling()` shall return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_GET_PORT_SAMPLING_01}
 *
 * @design
 * 1. If `id` is greater than or equal to
 *    `current_partition_arinc->nports_sampling`:
 *    \llrref{LLR_PORT_GET_PORT_SAMPLING_01}
 *    1. Return `NULL`.
 * 2. Set `port_sampling` to the address of
 *    `current_partition_arinc->ports_sampling[id]`.
 *    \llrref{LLR_PORT_GET_PORT_SAMPLING_02},
 *    \llrref{LLR_PORT_GET_PORT_SAMPLING_03}
 * 3. Return `port_sampling` if `port_sampling->is_created` is true, otherwise
 *    `NULL`. \llrref{LLR_PORT_GET_PORT_SAMPLING_02},
 *    \llrref{LLR_PORT_GET_PORT_SAMPLING_03}
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
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_INIT_01}: `pok_port_sampling_init()`
 *   shall make a sampling port one that is not created.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_004}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_INIT_01}: `pok_port_sampling_init()`
 *   shall set `is_created` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_INIT_01}
 *
 * @design
 * 1. Set `port_sampling->is_created` to `FALSE`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_INIT_01}
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
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_CREATE_01}: `pok_port_sampling_create()`
 *   shall create a sampling port of the current partition outside `NORMAL`
 *   mode, if its name is that of a sampling port of the partition in the
 *   configuration, not yet created, with the configured maximum message size
 *   and direction and a finite, positive refresh period; and give its
 *   identifier.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_002}
 *   - \srqref{SRQ_JET_IPC_017}
 *   - \srqref{SRQ_JET_IPC_019}
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_CREATE_02}: `pok_port_sampling_create()`
 *   shall refuse name, refresh period or identifier addresses that the
 *   partition may not access with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_01}: If `mode` of the partition is
 *   `POK_PARTITION_MODE_NORMAL`, `pok_port_sampling_create()` shall return
 *   `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_02}: If `mode` is not
 *   `POK_PARTITION_MODE_NORMAL` and `jet_user_to_kernel_ro()` returns NULL for
 *   `name` and `MAX_NAME_LENGTH`, `pok_port_sampling_create()` shall return
 *   `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_03}: If the name is readable and
 *   `jet_user_to_kernel_typed_ro()` returns NULL for `refresh`,
 *   `pok_port_sampling_create()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_04}: If `refresh` is readable and
 *   `jet_user_to_kernel_typed()` returns NULL for `id`,
 *   `pok_port_sampling_create()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_05}: If these conversions give
 *   addresses and `find_port_sampling()` returns NULL,
 *   `pok_port_sampling_create()` shall return `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_06}: If `find_port_sampling()`
 *   returns a port whose `is_created` is true, `pok_port_sampling_create()`
 *   shall return `POK_ERRNO_EXISTS`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_07}: If the port found is not
 *   created and `size` is not `max_message_size` of its channel,
 *   `pok_port_sampling_create()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_08}: If `size` is
 *   `max_message_size` of the channel and `direction` is not `direction` of the
 *   port, `pok_port_sampling_create()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_09}: If `direction` is that of the
 *   port and `pok_time_is_infinity()` returns true for the refresh period (the
 *   value at `refresh`) or the period is 0, `pok_port_sampling_create()` shall
 *   return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_10}: If all these checks pass and
 *   `direction` of the port is `POK_PORT_DIRECTION_OUT`,
 *   `pok_port_sampling_create()` shall set `is_created` of the port to `TRUE`,
 *   its `refresh_period` to the period and `last_message_validity` to `FALSE`,
 *   call `pok_channel_sampling_s_clear_message()` with the channel, store the
 *   index of the port in `ports_sampling` at `id`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CREATE_11}: If all these checks pass and
 *   `direction` of the port is not `POK_PORT_DIRECTION_OUT`,
 *   `pok_port_sampling_create()` shall do the same with
 *   `pok_channel_sampling_r_clear_message()` in place of
 *   `pok_channel_sampling_s_clear_message()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CREATE_01}
 *
 * @design
 * 1. If `current_partition_arinc->mode` is equal to
 *    `POK_PARTITION_MODE_NORMAL`: \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_01}
 *    1. Return `POK_ERRNO_MODE`.
 * 2. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_02}
 * 3. If `k_name` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 4. Set `k_refresh` to `jet_user_to_kernel_typed_ro(refresh)`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_03}
 * 5. If `k_refresh` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_03}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 6. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_04}
 * 7. If `k_id` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_04}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 8. Set `kernel_refresh` to the value at `k_refresh`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_09},
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_10},
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_11}
 * 9. Call `find_port_sampling()` with the argument `k_name`, and store the
 *    result in `port_sampling`. \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_05}
 * 10. If `port_sampling` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_05}
 *     1. Return `POK_ERRNO_UNAVAILABLE`.
 * 11. If `port_sampling->is_created` is true:
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_06}
 *     1. Return `POK_ERRNO_EXISTS`.
 * 12. If `size` is not equal to `port_sampling->channel->max_message_size`:
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_07}
 *     1. Return `POK_ERRNO_EINVAL`.
 * 13. If `direction` is not equal to `port_sampling->direction`:
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_08}
 *     1. Return `POK_ERRNO_EINVAL`.
 * 14. If `pok_time_is_infinity(kernel_refresh) || (kernel_refresh == 0)` holds:
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_09}
 *     1. Return `POK_ERRNO_EINVAL`.
 * 15. Set `port_sampling->is_created` to `TRUE`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_10},
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_11}
 * 16. Set `port_sampling->refresh_period` to `kernel_refresh`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_10},
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_11}
 * 17. Set `port_sampling->last_message_validity` to `FALSE`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_10},
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_11}
 * 18. If `port_sampling->direction` is equal to `POK_PORT_DIRECTION_OUT`:
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_10}
 *     1. Call `pok_channel_sampling_s_clear_message()` with the argument
 *        `port_sampling->channel`.
 * 19. Otherwise: \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_11}
 *     1. Call `pok_channel_sampling_r_clear_message()` with the argument
 *        `port_sampling->channel`.
 * 20. Set `*k_id` to `port_sampling - current_partition_arinc->ports_sampling`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_10},
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_11}
 * 21. Return `POK_ERRNO_OK`. \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_10},
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_CREATE_11}
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
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_WRITE_01}: `pok_port_sampling_write()`
 *   shall write a message to a created sampling source port of the current
 *   partition, replacing the message of the channel, with the time of writing.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_018}
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_WRITE_02}: `pok_port_sampling_write()`
 *   shall refuse an identifier of no created port, a length of 0 or above the
 *   maximum message size, a destination port, and a message that the partition
 *   may not read.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_WRITE_01}: If `get_port_sampling()`
 *   returns NULL for `id`, `pok_port_sampling_write()` shall return
 *   `POK_ERRNO_PORT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_WRITE_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_WRITE_02}: If the port exists and `len`
 *   is 0 or greater than `max_message_size` of its channel,
 *   `pok_port_sampling_write()` shall return `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_WRITE_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_WRITE_03}: If `len` is valid and
 *   `direction` of the port is not `POK_PORT_DIRECTION_OUT`,
 *   `pok_port_sampling_write()` shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_WRITE_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_WRITE_04}: If `direction` is
 *   `POK_PORT_DIRECTION_OUT` and `jet_user_to_kernel_ro()` returns NULL for
 *   `data` and `len`, `pok_port_sampling_write()` shall return
 *   `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_WRITE_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_WRITE_05}: If these checks pass,
 *   `pok_port_sampling_write()` shall call `pok_preemption_local_disable()`,
 *   copy `len` bytes of the data to the result of
 *   `pok_channel_sampling_s_get_message()` for the channel with `memcpy()`,
 *   call `pok_channel_sampling_send_message()` with the channel and `len`, then
 *   `pok_preemption_local_enable()`, and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_WRITE_01}
 *
 * @design
 * 1. Call `get_port_sampling()` with the argument `id`, and store the result in
 *    `port_sampling`. \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_01}
 * 2. If `port_sampling` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_01}
 *    1. Return `POK_ERRNO_PORT`.
 * 3. If `len` is equal to `0U` or `len` is greater than
 *    `port_sampling->channel->max_message_size`:
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_02}
 *    1. Return `POK_ERRNO_EINVAL`.
 * 4. If `port_sampling->direction` is not equal to `POK_PORT_DIRECTION_OUT`:
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_03}
 *    1. Return `POK_ERRNO_MODE`.
 * 5. Call `jet_user_to_kernel_ro()` with the arguments `data` and `len`, and
 *    store the result in `k_data`. \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_04}
 * 6. If `k_data` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_04}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 7. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_05}
 * 8. Call `pok_channel_sampling_s_get_message()` with the argument
 *    `port_sampling->channel`, and store the result in `message`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_05}
 * 9. Call `memcpy()` with the arguments `message`, `k_data` and `len`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_05}
 * 10. Call `pok_channel_sampling_send_message()` with the arguments
 *     `port_sampling->channel` and `len`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_05}
 * 11. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_05}
 * 12. Return `POK_ERRNO_OK`. \llrref{LLR_PORT_POK_PORT_SAMPLING_WRITE_05}
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
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_READ_01}: `pok_port_sampling_read()`
 *   shall give the current process the latest message of a created sampling
 *   destination port, with its length, without removing it, and whether it is
 *   valid: not older than the refresh period of the port.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_007}
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_READ_02}: `pok_port_sampling_read()`
 *   shall report an empty port, with a length of 0 and an invalid message.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_007}
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_READ_03}: `pok_port_sampling_read()`
 *   shall refuse an identifier of no created port, a source port, and buffer,
 *   length or validity addresses that the partition may not write.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_READ_01}: If `get_port_sampling()`
 *   returns NULL for `id`, `pok_port_sampling_read()` shall return
 *   `POK_ERRNO_PORT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_READ_03}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_READ_02}: If the port exists and its
 *   `direction` is not `POK_PORT_DIRECTION_IN`, `pok_port_sampling_read()`
 *   shall return `POK_ERRNO_DIRECTION`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_READ_03}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_READ_03}: If `direction` is
 *   `POK_PORT_DIRECTION_IN` and `jet_user_to_kernel()` returns NULL for `data`
 *   and `max_message_size` of the channel, `pok_port_sampling_read()` shall
 *   return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_READ_03}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_READ_04}: If `data` is writable and
 *   `jet_user_to_kernel_typed()` returns NULL for `len`,
 *   `pok_port_sampling_read()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_READ_03}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_READ_05}: If `len` is writable and
 *   `jet_user_to_kernel_typed()` returns NULL for `valid`,
 *   `pok_port_sampling_read()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_READ_03}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_READ_06}: If these checks pass,
 *   `pok_port_sampling_read()` shall call `pok_preemption_local_disable()`
 *   before it reads the channel and `pok_preemption_local_enable()` before it
 *   returns, and store `last_message_validity` of the port at `valid` after it
 *   has set it.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_READ_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_READ_07}: If
 *   `pok_channel_sampling_r_get_message()` returns a message and the result of
 *   `jet_system_time()` minus its time is not greater than `refresh_period` of
 *   the port, `pok_port_sampling_read()` shall copy the message to `data` with
 *   `memcpy()`, store its size at `len`, set `last_message_validity` to `TRUE`
 *   and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_READ_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_READ_08}: If
 *   `pok_channel_sampling_r_get_message()` returns a message and the result of
 *   `jet_system_time()` minus its time is greater than `refresh_period`,
 *   `pok_port_sampling_read()` shall copy the message to `data` with
 *   `memcpy()`, store its size at `len`, set `last_message_validity` to `FALSE`
 *   and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_READ_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_READ_09}: If
 *   `pok_channel_sampling_r_get_message()` returns NULL,
 *   `pok_port_sampling_read()` shall store 0 at `len`, set
 *   `last_message_validity` to `FALSE` and return `POK_ERRNO_EMPTY`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_READ_02}
 *
 * @design
 * 1. Call `get_port_sampling()` with the argument `id`, and store the result in
 *    `port_sampling`. \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_01}
 * 2. If `port_sampling` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_01}
 *    1. Return `POK_ERRNO_PORT`.
 * 3. If `port_sampling->direction` is not equal to `POK_PORT_DIRECTION_IN`:
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_02}
 *    1. Return `POK_ERRNO_DIRECTION`.
 * 4. Call `jet_user_to_kernel()` with the arguments `data` and
 *    `port_sampling->channel->max_message_size`, and store the result in
 *    `k_data`. \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_03}
 * 5. If `k_data` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_03}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 6. Set `k_len` to `jet_user_to_kernel_typed(len)`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_04}
 * 7. If `k_len` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_04}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 8. Set `k_valid` to `jet_user_to_kernel_typed(valid)`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_05}
 * 9. If `k_valid` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_05}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 10. Call `pok_preemption_local_disable()`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_06}
 * 11. Call `pok_channel_sampling_r_get_message()` with the arguments
 *     `port_sampling->channel`, the address of `message_size` and the address
 *     of `ts`, and store the result in `message`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_07},
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_08},
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_09}
 * 12. If `message` is not NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_07},
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_08}
 *     1. Call `memcpy()` with the arguments `k_data`, `message` and
 *        `message_size`.
 *     2. Set `*k_len` to `(pok_port_size_t)message_size`.
 *     3. Set `current_time` to `jet_system_time()`.
 *     4. Set `port_sampling->last_message_validity` to `TRUE` if
 *        `(current_time - ts)` is less than or equal to
 *        `port_sampling->refresh_period`, otherwise `FALSE`.
 *     5. Set `ret` to `POK_ERRNO_OK`.
 * 13. Otherwise: \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_09}
 *     1. Set `*k_len` to `0`.
 *     2. Set `port_sampling->last_message_validity` to `FALSE`.
 *     3. Set `ret` to `POK_ERRNO_EMPTY`.
 * 14. Set `*k_valid` to `port_sampling->last_message_validity`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_06}
 * 15. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_06}
 * 16. Return `ret`. \llrref{LLR_PORT_POK_PORT_SAMPLING_READ_06}
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
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_ID_01}: `pok_port_sampling_id()` shall
 *   give the identifier of a created sampling port of the current partition
 *   found by its name.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_003}
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_ID_02}: `pok_port_sampling_id()` shall
 *   refuse a name or an identifier address that the partition may not access
 *   with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_ID_01}: If `jet_user_to_kernel_ro()`
 *   returns NULL for `name` and `MAX_NAME_LENGTH`, `pok_port_sampling_id()`
 *   shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_ID_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_ID_02}: If the name is readable and
 *   `jet_user_to_kernel_typed()` returns NULL for `id`,
 *   `pok_port_sampling_id()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_ID_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_ID_03}: If both conversions give an
 *   address and `find_port_sampling()` returns NULL, `pok_port_sampling_id()`
 *   shall return `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_ID_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_ID_04}: If `find_port_sampling()` returns
 *   a port whose `is_created` is false, `pok_port_sampling_id()` shall return
 *   `POK_ERRNO_UNAVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_ID_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_ID_05}: If `find_port_sampling()` returns
 *   a port whose `is_created` is true, `pok_port_sampling_id()` shall store its
 *   index in `ports_sampling` of the partition at `id` and return
 *   `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_ID_01}
 *
 * @design
 * 1. Call `jet_user_to_kernel_ro()` with the arguments `name` and
 *    `MAX_NAME_LENGTH`, and store the result in `k_name`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_ID_01}
 * 2. If `k_name` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_ID_01}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 3. Set `k_id` to `jet_user_to_kernel_typed(id)`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_ID_02}
 * 4. If `k_id` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_ID_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `find_port_sampling()` with the argument `k_name`, and store the
 *    result in `port_sampling`. \llrref{LLR_PORT_POK_PORT_SAMPLING_ID_03}
 * 6. If `port_sampling` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_ID_03}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 7. If `port_sampling->is_created` is false:
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_ID_04}
 *    1. Return `POK_ERRNO_UNAVAILABLE`.
 * 8. Set `*k_id` to `port_sampling - current_partition_arinc->ports_sampling`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_ID_05}
 * 9. Return `POK_ERRNO_OK`. \llrref{LLR_PORT_POK_PORT_SAMPLING_ID_05}
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
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_STATUS_01}: `pok_port_sampling_status()`
 *   shall give the maximum message size, the direction, the refresh period and
 *   the validity of the last message read of a created sampling port of the
 *   current partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_003}
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_STATUS_02}: `pok_port_sampling_status()`
 *   shall refuse an identifier of no created port and a status address that the
 *   partition may not write.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_STATUS_01}: If `get_port_sampling()`
 *   returns NULL for `id`, `pok_port_sampling_status()` shall return
 *   `POK_ERRNO_PORT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_STATUS_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_STATUS_02}: If the port exists and
 *   `jet_user_to_kernel_typed()` returns NULL for `status`,
 *   `pok_port_sampling_status()` shall return `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_STATUS_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_STATUS_03}: If both give an address,
 *   `pok_port_sampling_status()` shall call `pok_preemption_local_disable()`,
 *   set `size` of the status to `max_message_size` of the channel, `direction`
 *   to that of the port, `refresh` to its `refresh_period` and `validity` to
 *   its `last_message_validity`, call `pok_preemption_local_enable()`, and
 *   return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_STATUS_01}
 *
 * @design
 * 1. Call `get_port_sampling()` with the argument `id`, and store the result in
 *    `port_sampling`. \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_01}
 * 2. If `port_sampling` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_01}
 *    1. Return `POK_ERRNO_PORT`.
 * 3. Set `k_status` to `jet_user_to_kernel_typed(status)`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_02}
 * 4. If `k_status` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_02}
 *    1. Return `POK_ERRNO_EFAULT`.
 * 5. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_03}
 * 6. Set `k_status->size` to `port_sampling->channel->max_message_size`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_03}
 * 7. Set `k_status->direction` to `port_sampling->direction`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_03}
 * 8. Set `k_status->refresh` to `port_sampling->refresh_period`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_03}
 * 9. Set `k_status->validity` to `port_sampling->last_message_validity`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_03}
 * 10. Call `pok_preemption_local_enable()`.
 *     \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_03}
 * 11. Return `POK_ERRNO_OK`. \llrref{LLR_PORT_POK_PORT_SAMPLING_STATUS_03}
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
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_CHECK_01}: `pok_port_sampling_check()`
 *   shall tell whether a created sampling destination port of the current
 *   partition has a message that has not been read yet.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_008}
 * - \hlrid{HLR_PORT_POK_PORT_SAMPLING_CHECK_02}: `pok_port_sampling_check()`
 *   shall refuse an identifier of no created port and a source port.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *
 * @llr
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CHECK_01}: If `get_port_sampling()`
 *   returns NULL for `id`, `pok_port_sampling_check()` shall return
 *   `POK_ERRNO_PORT`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CHECK_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CHECK_02}: If the port exists and its
 *   `direction` is not `POK_PORT_DIRECTION_IN`, `pok_port_sampling_check()`
 *   shall return `POK_ERRNO_MODE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CHECK_02}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CHECK_03}: If `direction` is
 *   `POK_PORT_DIRECTION_IN`, `pok_port_sampling_check()` shall call
 *   `pok_preemption_local_disable()` before and `pok_preemption_local_enable()`
 *   after the call of `pok_channel_sampling_r_check_new_message()` for the
 *   channel.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CHECK_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CHECK_04}: If
 *   `pok_channel_sampling_r_check_new_message()` returns true,
 *   `pok_port_sampling_check()` shall return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CHECK_01}
 * - \llrid{LLR_PORT_POK_PORT_SAMPLING_CHECK_05}: If
 *   `pok_channel_sampling_r_check_new_message()` returns false,
 *   `pok_port_sampling_check()` shall return `POK_ERRNO_EMPTY`.
 *   <br>Traces to:
 *   - \hlrref{HLR_PORT_POK_PORT_SAMPLING_CHECK_01}
 *
 * @design
 * 1. Call `get_port_sampling()` with the argument `id`, and store the result in
 *    `port_sampling`. \llrref{LLR_PORT_POK_PORT_SAMPLING_CHECK_01}
 * 2. If `port_sampling` is NULL: \llrref{LLR_PORT_POK_PORT_SAMPLING_CHECK_01}
 *    1. Return `POK_ERRNO_PORT`.
 * 3. If `port_sampling->direction` is not equal to `POK_PORT_DIRECTION_IN`:
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CHECK_02}
 *    1. Return `POK_ERRNO_MODE`.
 * 4. Call `pok_preemption_local_disable()`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CHECK_03}
 * 5. Set `ret` to `POK_ERRNO_OK` if
 *    `pok_channel_sampling_r_check_new_message(port_sampling->channel)` is
 *    true, otherwise `POK_ERRNO_EMPTY`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CHECK_04},
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CHECK_05}
 * 6. Call `pok_preemption_local_enable()`.
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CHECK_03}
 * 7. Return `ret`. \llrref{LLR_PORT_POK_PORT_SAMPLING_CHECK_04},
 *    \llrref{LLR_PORT_POK_PORT_SAMPLING_CHECK_05}
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
