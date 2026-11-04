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

/* 
 * Find *configured* queuing port by name, which comes from user space.
 * 
 * NOTE: Name should be checked before!
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


/* 
 * Get *created* queuing port by id.
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

void port_queuing_send(pok_port_queuing_t* port, pok_thread_t* t)
{
    char* message = pok_channel_queuing_s_get_message(port->channel, FALSE);
    assert(message != NULL);

    (void) memcpy(message, t->wait_buffer.src, t->wait_len);

    pok_channel_queuing_s_produce_message(port->channel, t->wait_len);

    t->wait_result = POK_ERRNO_OK;
}

void pok_port_queuing_init(pok_port_queuing_t* port_queuing)
{
    pok_thread_wq_init(&port_queuing->waiters);

    port_queuing->is_created = FALSE;
}


pok_ret_t pok_port_queuing_create(
    const char* JET_USER              name,
    pok_port_size_t                 message_size,
    pok_port_size_t                 max_nb_message,
    pok_port_direction_t            direction,
    pok_queuing_discipline_t        discipline,
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

pok_ret_t pok_port_queuing_create_packed(
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


pok_ret_t pok_port_queuing_receive(
    pok_port_id_t               id,
    const pok_time_t* JET_USER    timeout,
    void* JET_USER                data,
    pok_port_size_t* JET_USER     len)
{
    pok_port_queuing_t* port_queuing;
    pok_ret_t ret;
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



pok_ret_t pok_port_queuing_send(
    pok_port_id_t               id,
    const void* JET_USER          data,
    pok_port_size_t             len,
    const pok_time_t* JET_USER    timeout)
{
    pok_port_queuing_t* port_queuing;
    pok_ret_t ret;
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


pok_ret_t pok_port_queuing_status(
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

pok_ret_t pok_port_queuing_id(
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

pok_ret_t pok_port_queuing_clear(pok_port_id_t id)
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

/**********************************************************************/
/* 
 * Find *configured* sampling port by name, which comes from user space.
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


/* 
 * Get *created* sampling port by id.
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

void pok_port_sampling_init(pok_port_sampling_t* port_sampling)
{
    port_sampling->is_created = FALSE;
}


pok_ret_t pok_port_sampling_create(
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

pok_ret_t pok_port_sampling_write(
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

pok_ret_t pok_port_sampling_read(
    pok_port_id_t           id,
    void* JET_USER            data,
    pok_port_size_t* JET_USER len,
    pok_bool_t* JET_USER      valid)
{
    pok_port_sampling_t* port_sampling;
    pok_ret_t ret;
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

pok_ret_t pok_port_sampling_id(
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

pok_ret_t pok_port_sampling_status (
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

// TODO: This should be transformed into READ_UPDATED_SAMPLING_MESSAGE eventually.
pok_ret_t pok_port_sampling_check(pok_port_id_t id)
{
    pok_ret_t ret;

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
