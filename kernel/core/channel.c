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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies; essentially Boolean conditions; U suffixes for
 * unsigned constants; operands of the same essential type; internal
 * linkage for file-local definitions; if-else-if chains end with else.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: increments and
 * assignments are separate statements; operands of mixed precedence are
 * parenthesized; deviations are documented where a guideline can't be
 * followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: a duplicated semicolon in
 * pok_channel_sampling_r_get_message() is removed. Doxygen headers for the
 * file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: jet_partition_event_type_t,
 * pok_channel_queuing_side_t.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * jet_partition_event_type_e (was jet_partition_event_type_t).
 */

/**
 * @file
 * Channels between ports. A queuing channel is a ring of messages shared by
 * the sender and the receiver, split by a border into the messages the
 * receiver may take and those still with the sender; each side notifies its
 * partition when room or a message becomes available. A sampling channel
 * keeps the last message in one of three buffers, so that writing never
 * disturbs a read.
 */

#include <core/channel.h>
#include <types.h>
#include <assert.h>
#include <core/time.h>

#include <core/sched.h>
#include <alloc.h>

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the allocator returns its
 * memory as a pointer to void, which is converted to the type of the object
 * allocated.
 */

/*********************** Queuing channel ******************************/

/**
 * @funcname `pok_channel_queuing_init`
 *
 * @description
 * Initializes a queuing channel at boot: allocates its ring (the room of both
 * sides) and the message sizes, empty.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in,out] The channel. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message_alignment` | `const uint32_t` | The alignment of each message |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `channel->max_nb_message` to
 *    `channel->recv.max_nb_message + channel->send.max_nb_message`.
 * 2. Set `channel->send.next_message` to `0`.
 * 3. Set `channel->recv.next_message` to `0`.
 * 4. Set `channel->border` to `0`.
 * 5. Set `message_alignment` to `__alignof__(int32_t)`.
 * 6. Call `ALIGN_VAL()` with the arguments `channel->max_message_size` and
 *    `message_alignment`, and store the result in `channel->message_stride`.
 * 7. Call `ja_mem_alloc_aligned()` with the arguments
 *    `channel->message_stride * channel->max_nb_message` and
 *    `message_alignment`, and store the result in `channel->messages`.
 * 8. Call `ja_mem_alloc_aligned()` with the arguments
 *    `sizeof(*channel->message_sizes) * channel->max_nb_message` and
 *    `__alignof__(*channel->message_sizes)`, and store the result in
 *    `channel->message_sizes`.
 * 9. Set `channel->send.generation` to `0`.
 * 10. Set `channel->send.is_notify` to `FALSE`.
 * 11. Set `channel->recv.generation` to `0`.
 * 12. Set `channel->recv.is_notify` to `FALSE`.
 * 13. Set `channel->message_discarded` to `FALSE`.
 *
 * @calls
 * - `ALIGN_VAL()`
 * - `ja_mem_alloc_aligned()`
 */
void pok_channel_queuing_init(pok_channel_queuing_t* channel)
{
    channel->max_nb_message =
        channel->recv.max_nb_message + channel->send.max_nb_message;

    channel->send.next_message = 0;
    channel->recv.next_message = 0;
    channel->border = 0;

    const uint32_t message_alignment = __alignof__(int32_t);

    channel->message_stride = ALIGN_VAL(channel->max_message_size,
        message_alignment);

    channel->messages = ja_mem_alloc_aligned(
        channel->message_stride * channel->max_nb_message,
        message_alignment);

    channel->message_sizes = ja_mem_alloc_aligned(
        sizeof(*channel->message_sizes) * channel->max_nb_message,
        __alignof__(*channel->message_sizes));

    channel->send.generation = 0;
    channel->send.is_notify = FALSE;

    channel->recv.generation = 0;
    channel->recv.is_notify = FALSE;

    channel->message_discarded = FALSE;
}

/**
 * @funcname `channel_queuing_cyclic_add`
 *
 * @description
 * Adds to a position of the ring, wrapping around.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in] The channel. |
 * | `pos` | `uint16_t` | [in] The position. |
 * | `index` | `uint16_t` | [in] The number of messages to add. |
 *
 * @return The new position.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `sum` | `uint16_t` | The position, before wrapping |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `sum` to `pos + index`.
 * 2. If `sum` is greater than or equal to `channel->max_nb_message`:
 *    1. Subtract `channel->max_nb_message` from `sum`.
 * 3. Return `sum`.
 *
 * @calls
 * None.
 */
static inline
uint16_t channel_queuing_cyclic_add(pok_channel_queuing_t* channel,
    uint16_t pos, uint16_t index)
{
    uint16_t sum = pos + index;
    if(sum >= channel->max_nb_message) {
        sum -= channel->max_nb_message;
    }

    return sum;
}

/**
 * @funcname `channel_queuing_cyclic_sub`
 *
 * @description
 * Returns the number of messages between two positions of the ring.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in] The channel. |
 * | `pos_end` | `pok_message_range_t` | [in] The end position. |
 * | `pos_start` | `pok_message_range_t` | [in] The start position. |
 *
 * @return The number of messages from pos_start to pos_end.
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
 * 1. Return `(pos_end - pos_start)` if `pos_end` is greater than or equal to
 *    `pos_start`, otherwise `(pos_end + channel->max_nb_message - pos_start)`.
 *
 * @calls
 * None.
 */
static inline
pok_message_range_t channel_queuing_cyclic_sub(pok_channel_queuing_t* channel,
    pok_message_range_t pos_end, pok_message_range_t pos_start)
{
    return (pos_end >= pos_start)
        ? (pos_end - pos_start)
        : (pos_end + channel->max_nb_message - pos_start);
}

/**
 * @funcname `channel_queuing_message_at`
 *
 * @description
 * Returns the message at a position of the ring.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in] The channel. |
 * | `pos` | `pok_message_range_t` | [in] The position. |
 *
 * @return The message's buffer.
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
 * 1. Check that `pos < channel->max_nb_message` holds (assert: stop otherwise).
 * 2. Return the address of `channel->messages[channel->message_stride * pos]`.
 *
 * @calls
 * - `pok_assertion_fail()`
 */
static inline char* channel_queuing_message_at(
    pok_channel_queuing_t* channel, pok_message_range_t pos)
{
    assert(pos < channel->max_nb_message);

    return &channel->messages[channel->message_stride * pos];
}

/**
 * @funcname `channel_queuing_side_notify`
 *
 * @description
 * Notifies a side's partition with an event, if that side asked for it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `side` | `pok_channel_queuing_side_t *` | [in,out] The side. |
 * | `event_type` | `jet_partition_event_type_e` | [in] The event (room or message available). |
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
 * 1. If `side->is_notify` is true:
 *    1. Set `side->is_notify` to `FALSE`.
 *    2. Call `pok_partition_add_event()` with the arguments `side->part`,
 *       `event_type` and `side->handler_id`.
 *
 * @calls
 * - `pok_partition_add_event()`
 */
static inline void channel_queuing_side_notify(pok_channel_queuing_side_t* side,
    jet_partition_event_type_e event_type)
{
    if(side->is_notify)
    {
        side->is_notify = FALSE;
        pok_partition_add_event(side->part, event_type, side->handler_id);
    }
}

/**
 * @funcname `pok_channel_queuing_side_init`
 *
 * @description
 * Initializes one side of a queuing channel when its port is created (or
 * cleared): it starts at the border, belongs to the partition's current
 * generation and notifies the port's handler.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in] The channel. |
 * | `side` | `pok_channel_queuing_side_t *` | [in,out] The sending or receiving side. |
 * | `handler_id` | `uint16_t` | [in] The port's ID, given with the events. |
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
 * 1. Call `pok_preemption_disable()`.
 * 2. Set `side->next_message` to `channel->border`.
 * 3. Set `side->is_notify` to `FALSE`.
 * 4. Set `side->generation` to `side->part->partition_generation`.
 * 5. Set `side->handler_id` to `handler_id`.
 * 6. Call `__pok_preemption_enable()`.
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `__pok_preemption_enable()`
 */
void pok_channel_queuing_side_init(pok_channel_queuing_t* channel,
    pok_channel_queuing_side_t* side,
    uint16_t handler_id)
{
    pok_preemption_disable();
    side->next_message = channel->border;
    side->is_notify = FALSE;
    side->generation = side->part->partition_generation;
    side->handler_id = handler_id;

    __pok_preemption_enable();
}

/**
 * @funcname `pok_channel_queuing_r_n_messages`
 *
 * @description
 * Returns the number of messages the receiver may take.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in] The channel. |
 *
 * @return The number of messages.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n_messages` | `size_t` | The number of messages |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 * 2. Call `channel_queuing_cyclic_sub()` with the arguments `channel`,
 *    `channel->border` and `channel->recv.next_message`, and store the result
 *    in `n_messages`.
 * 3. Call `__pok_preemption_enable()`.
 * 4. Return `n_messages`.
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `channel_queuing_cyclic_sub()`
 * - `__pok_preemption_enable()`
 */
pok_message_range_t pok_channel_queuing_r_n_messages(pok_channel_queuing_t* channel)
{
    pok_preemption_disable();

    size_t n_messages = channel_queuing_cyclic_sub(channel,
        channel->border,
        channel->recv.next_message);

    __pok_preemption_enable();

    return n_messages;
}

/**
 * @funcname `pok_channel_queuing_r_get_message`
 *
 * @description
 * Returns the next message for the receiver, without taking it; without a
 * message, the receiver may ask to be notified.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in,out] The channel. |
 * | `size` | `pok_message_size_t *` | [out] Where the message's size is stored. |
 * | `subscribe` | `pok_bool_t` | [in] TRUE to be notified when a message comes. |
 *
 * @return The message, or NULL if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message` | `const char *` | The message, or NULL |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 * 2. If `channel->recv.next_message` is not equal to `channel->border`:
 *    1. Call `channel_queuing_message_at()` with the arguments `channel` and
 *       `channel->recv.next_message`, and store the result in `message`.
 *    2. Set `*size` to `channel->message_sizes[channel->recv.next_message]`.
 * 3. Otherwise:
 *    1. Set `message` to `NULL`.
 *    2. If `subscribe` is true:
 *       1. Set `channel->recv.is_notify` to `TRUE`.
 * 4. Call `__pok_preemption_enable()`.
 * 5. Return `message`.
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `channel_queuing_message_at()`
 * - `__pok_preemption_enable()`
 */
const char* pok_channel_queuing_r_get_message(
    pok_channel_queuing_t* channel,
    pok_message_size_t* size,
    pok_bool_t subscribe)
{
    const char* message;

    pok_preemption_disable();

    if(channel->recv.next_message != channel->border)
    {
        message = channel_queuing_message_at(channel, channel->recv.next_message);
        *size = channel->message_sizes[channel->recv.next_message];
    }
    else
    {
        message = NULL;
        if(subscribe) {
            channel->recv.is_notify = TRUE;
        }
    }

    __pok_preemption_enable();

    return message;
}


/**
 * @funcname `pok_channel_queuing_r_consume_message`
 *
 * @description
 * Takes the receiver's next message: the receiver moves on, and if the sender
 * (of the same generation) has messages beyond the border, the border moves and
 * the sender is notified of the room; tells whether messages were discarded
 * meanwhile.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in,out] The channel. |
 * | `message_discarded` | `pok_bool_t *` | [out] Where the overflow flag is stored. |
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
 * 1. Check that `channel->recv.next_message != channel->border` holds (assert:
 *    stop otherwise).
 * 2. Call `pok_preemption_disable()`.
 * 3. Call `channel_queuing_cyclic_add()` with the arguments `channel`,
 *    `channel->recv.next_message` and `1`, and store the result in
 *    `channel->recv.next_message`.
 * 4. If `channel->send.part->partition_generation` is equal to
 *    `channel->send.generation` and `channel->border` is not equal to
 *    `channel->send.next_message`:
 *    1. Call `channel_queuing_cyclic_add()` with the arguments `channel`,
 *       `channel->border` and `1`, and store the result in `channel->border`.
 *    2. Call `channel_queuing_side_notify()` with the arguments the address of
 *       `channel->send` and `JET_PARTITION_EVENT_TYPE_PORT_SEND_AVAILABLE`.
 * 5. Set `*message_discarded` to `channel->message_discarded`.
 * 6. Set `channel->message_discarded` to `FALSE`.
 * 7. Call `pok_preemption_enable()`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `pok_preemption_disable()`
 * - `channel_queuing_cyclic_add()`
 * - `channel_queuing_side_notify()`
 * - `pok_preemption_enable()`
 */
void pok_channel_queuing_r_consume_message(
    pok_channel_queuing_t* channel,
    pok_bool_t* message_discarded)
{
    assert(channel->recv.next_message != channel->border);

    pok_preemption_disable();

    channel->recv.next_message = channel_queuing_cyclic_add(channel,
        channel->recv.next_message, 1);

    if((channel->send.part->partition_generation == channel->send.generation)
        && (channel->border != channel->send.next_message))
    {
        channel->border = channel_queuing_cyclic_add(channel,
            channel->border, 1);
        channel_queuing_side_notify(&channel->send,
            JET_PARTITION_EVENT_TYPE_PORT_SEND_AVAILABLE);
    }

    *message_discarded = channel->message_discarded;
    channel->message_discarded = FALSE;
    pok_preemption_enable();
}

/**
 * @funcname `pok_channel_queuing_s_get_message`
 *
 * @description
 * Returns the sender's room for a new message, if it has room; without room,
 * the sender may ask to be notified.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in,out] The channel. |
 * | `subscribe` | `pok_bool_t` | [in] TRUE to be notified when room comes. |
 *
 * @return The room for the message, or NULL if the sender's side is full.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message` | `char *` | The room, or NULL |
 * | `n_used` | `pok_message_range_t` | The messages the sender holds |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 * 2. Call `channel_queuing_cyclic_sub()` with the arguments `channel`,
 *    `channel->send.next_message` and `channel->border`, and store the result
 *    in `n_used`.
 * 3. If `n_used` is less than `channel->send.max_nb_message`:
 *    1. Call `channel_queuing_message_at()` with the arguments `channel` and
 *       `channel->send.next_message`, and store the result in `message`.
 * 4. Otherwise:
 *    1. Set `message` to `NULL`.
 *    2. If `subscribe` is true:
 *       1. Set `channel->send.is_notify` to `TRUE`.
 * 5. Call `__pok_preemption_enable()`.
 * 6. Return `message`.
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `channel_queuing_cyclic_sub()`
 * - `channel_queuing_message_at()`
 * - `__pok_preemption_enable()`
 */
char* pok_channel_queuing_s_get_message(
    pok_channel_queuing_t* channel,
    pok_bool_t subscribe)
{
    pok_message_range_t n_used;
    char* message;

    pok_preemption_disable();

    n_used = channel_queuing_cyclic_sub(channel,
        channel->send.next_message, channel->border);

    if(n_used < channel->send.max_nb_message)
    {
        message = channel_queuing_message_at(channel, channel->send.next_message);
    }
    else
    {
        message = NULL;
        if(subscribe) {
            channel->send.is_notify = TRUE;
        }
    }

    __pok_preemption_enable();

    return message;
}

/**
 * @funcname `pok_channel_queuing_s_produce_message`
 *
 * @description
 * Adds the message written in the sender's room: if the receiver (of the same
 * generation) has room it may take it (the border moves, the receiver is
 * notified); if not, it is discarded with the receiver-discard overflow
 * strategy, or stays with the sender. A receiver of an old generation doesn't
 * hold the sender back.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in,out] The channel. |
 * | `size` | `pok_message_size_t` | [in] The message's size. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `nb_message_receive` | `pok_message_range_t` | The messages the receiver may take |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Check that
 *    `channel_queuing_cyclic_sub(channel, channel->send.next_message,`
 *    `channel->border) < channel->send.max_nb_message` holds (assert: stop
 *    otherwise).
 * 2. Check that `size > 0U` holds (assert: stop otherwise).
 * 3. Check that `size <= channel->max_message_size` holds (assert: stop
 *    otherwise).
 * 4. Call `pok_preemption_disable()`.
 * 5. Set `channel->message_sizes[channel->send.next_message]` to `size`.
 * 6. Call `channel_queuing_cyclic_add()` with the arguments `channel`,
 *    `channel->send.next_message` and `1`, and store the result in
 *    `channel->send.next_message`.
 * 7. If `channel->recv.part->partition_generation` is equal to
 *    `channel->recv.generation`:
 *    1. Call `channel_queuing_cyclic_sub()` with the arguments `channel`,
 *       `channel->border` and `channel->recv.next_message`, and store the
 *       result in `nb_message_receive`.
 *    2. If `nb_message_receive` is less than `channel->recv.max_nb_message`:
 *       1. Set `channel->border` to `channel->send.next_message`.
 *       2. Call `channel_queuing_side_notify()` with the arguments the address
 *          of `channel->recv` and
 *          `JET_PARTITION_EVENT_TYPE_PORT_RECEIVE_AVAILABLE`.
 *    3. Otherwise, if `channel->overflow_strategy` is equal to
 *       `JET_CHANNEL_QUEUING_RECEIVER_DISCARD`:
 *       1. Set `channel->send.next_message` to `channel->border`.
 *       2. Set `channel->message_discarded` to `TRUE`.
 *    4. Otherwise:
 *       1. Do nothing.
 * 8. Otherwise:
 *    1. Set `channel->border` to `channel->send.next_message`.
 * 9. Call `pok_preemption_enable()`.
 *
 * @calls
 * - `channel_queuing_cyclic_sub()`
 * - `pok_assertion_fail()`
 * - `pok_preemption_disable()`
 * - `channel_queuing_cyclic_add()`
 * - `channel_queuing_side_notify()`
 * - `pok_preemption_enable()`
 */
void pok_channel_queuing_s_produce_message(
    pok_channel_queuing_t* channel,
    pok_message_size_t size)
{
    assert(channel_queuing_cyclic_sub(channel, channel->send.next_message,
        channel->border) < channel->send.max_nb_message);

    assert(size > 0U);
    assert(size <= channel->max_message_size);

    pok_preemption_disable();

    channel->message_sizes[channel->send.next_message] = size;
    channel->send.next_message = channel_queuing_cyclic_add(channel,
        channel->send.next_message, 1);

    if(channel->recv.part->partition_generation == channel->recv.generation)
    {
        // Receiver is ready.
        pok_message_range_t nb_message_receive = channel_queuing_cyclic_sub(
            channel, channel->border, channel->recv.next_message);

        if(nb_message_receive < channel->recv.max_nb_message)
        {
            // Move message to the receiver buffer
            channel->border = channel->send.next_message;
            // And notify receiver, if requested.
            channel_queuing_side_notify(&channel->recv,
                JET_PARTITION_EVENT_TYPE_PORT_RECEIVE_AVAILABLE);
        }
        else if(channel->overflow_strategy == JET_CHANNEL_QUEUING_RECEIVER_DISCARD)
        {
            /* Discard message and store note about that.*/
            channel->send.next_message = channel->border;
            channel->message_discarded = TRUE;
        }
        else
        {
            // JET_CHANNEL_QUEUING_SENDER_BLOCK: the sender waits for space.
        }
    }
    else
    {
        // Just drop the message and all previous ones which have been received.
        channel->border = channel->send.next_message;
    }

    pok_preemption_enable();
}

/**
 * @funcname `pok_channel_queuing_s_n_messages`
 *
 * @description
 * Returns the number of messages that the sender holds (not yet given to the
 * receiver).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_queuing_t *` | [in] The channel. |
 *
 * @return The number of messages.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n_messages` | `pok_message_range_t` | The number of messages |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 * 2. Call `channel_queuing_cyclic_sub()` with the arguments `channel`,
 *    `channel->send.next_message` and `channel->border`, and store the result
 *    in `n_messages`.
 * 3. Call `__pok_preemption_enable()`.
 * 4. Return `n_messages`.
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `channel_queuing_cyclic_sub()`
 * - `__pok_preemption_enable()`
 */
pok_message_range_t pok_channel_queuing_s_n_messages(pok_channel_queuing_t* channel)
{
    pok_preemption_disable();

    pok_message_range_t n_messages = channel_queuing_cyclic_sub(channel,
        channel->send.next_message, channel->border);

    __pok_preemption_enable();

    return n_messages;
}

/*********************** Sampling channel *****************************/

/**
 * @funcname `pok_channel_sampling_init`
 *
 * @description
 * Initializes a sampling channel at boot: allocates its three buffers; no
 * message.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_sampling_t *` | [in,out] The channel. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message_alignment` | `const uint32_t` | The alignment of each buffer |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `message_alignment` to `__alignof__(int32_t)`.
 * 2. Call `ALIGN_VAL()` with the arguments `channel->max_message_size` and
 *    `message_alignment`, and store the result in `channel->message_stride`.
 * 3. Call `ja_mem_alloc_aligned()` with the arguments
 *    `3U * channel->message_stride` and `message_alignment`, and store the
 *    result in `channel->messages`.
 * 4. Set `channel->read_pos_next` to `0`.
 * 5. Set `channel->read_pos` to `0`.
 * 6. Set `channel->message_sizes[0]` to `0`.
 * 7. Set `channel->write_pos` to `1`.
 *
 * @calls
 * - `ALIGN_VAL()`
 * - `ja_mem_alloc_aligned()`
 */
void pok_channel_sampling_init(pok_channel_sampling_t* channel)
{
    const uint32_t message_alignment = __alignof__(int32_t);

    channel->message_stride = ALIGN_VAL(channel->max_message_size,
        message_alignment);
    channel->messages = ja_mem_alloc_aligned(3U * channel->message_stride,
        message_alignment);

    channel->read_pos_next = 0;
    channel->read_pos = 0;

    // Mark the first message as empty.
    channel->message_sizes[0] = 0;

    channel->write_pos = 1;
}

/**
 * @funcname `channel_sampling_message_at`
 *
 * @description
 * Returns one of the three buffers of a sampling channel.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_sampling_t *` | [in] The channel. |
 * | `pos` | `int32_t` | [in] The buffer, 0 to 2. |
 *
 * @return The buffer.
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
 * 1. Check that `(pos >= 0) && (pos <= 2)` holds (assert: stop otherwise).
 * 2. Return the address of
 *    `channel->messages[(size_t)pos * channel->message_stride]`.
 *
 * @calls
 * - `pok_assertion_fail()`
 */
static char* channel_sampling_message_at(
    pok_channel_sampling_t* channel, int32_t pos)
{
    assert((pos >= 0) && (pos <= 2));

    return &channel->messages[(size_t)pos * channel->message_stride];
}


/**
 * @funcname `pok_channel_sampling_r_get_message`
 *
 * @description
 * Returns the last message of a sampling channel, with its size and time, and
 * makes it the one being read.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_sampling_t *` | [in,out] The channel. |
 * | `size` | `pok_message_size_t *` | [out] Where the message's size is stored. |
 * | `timestamp` | `pok_time_t *` | [out] Where the message's time is stored. |
 *
 * @return The message, or NULL if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `message_size` | `pok_message_size_t` | The message's size (0: none) |
 * | `read_pos` | `uint8_t` | The buffer of the message |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 * 2. Set `channel->read_pos` to `channel->read_pos_next`.
 * 3. Set `read_pos` to `channel->read_pos`.
 * 4. Call `__pok_preemption_enable()`.
 * 5. Set `message_size` to `channel->message_sizes[read_pos]`.
 * 6. If `message_size` is equal to `0U`:
 *    1. Return `NULL`.
 * 7. Set `*size` to `message_size`.
 * 8. Set `*timestamp` to `channel->timestamps[read_pos]`.
 * 9. Call `channel_sampling_message_at()` with the arguments `channel` and
 *    `read_pos`, and return its result.
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `__pok_preemption_enable()`
 * - `channel_sampling_message_at()`
 */
const char* pok_channel_sampling_r_get_message(
    pok_channel_sampling_t* channel,
    pok_message_size_t* size,
    pok_time_t* timestamp)
{
    uint8_t read_pos;

    pok_preemption_disable();
    channel->read_pos = channel->read_pos_next;
    read_pos = channel->read_pos;
    __pok_preemption_enable();

    pok_message_size_t message_size = channel->message_sizes[read_pos];
    if(message_size == 0U) {
        return NULL;
    }

    *size = message_size;
    *timestamp = channel->timestamps[read_pos];

    return channel_sampling_message_at(channel, read_pos);
}

/**
 * @funcname `pok_channel_sampling_r_clear_message`
 *
 * @description
 * Clears the message being read (when the IN port is created).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_sampling_t *` | [in,out] The channel. |
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
 * 1. Call `pok_preemption_disable()`.
 * 2. Set `channel->message_sizes[channel->read_pos]` to `0`.
 * 3. Call `__pok_preemption_enable()`.
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `__pok_preemption_enable()`
 */
void pok_channel_sampling_r_clear_message(pok_channel_sampling_t* channel)
{
    pok_preemption_disable();
    channel->message_sizes[channel->read_pos] = 0;
    __pok_preemption_enable();
}

/**
 * @funcname `pok_channel_sampling_r_check_new_message`
 *
 * @description
 * Tells whether a message came since the last read, and makes it the one being
 * read.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_sampling_t *` | [in,out] The channel. |
 *
 * @return TRUE if there is a new message.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `pok_bool_t` | The result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ret` to `FALSE`.
 * 2. Call `pok_preemption_disable()`.
 * 3. If `channel->read_pos` is not equal to `channel->read_pos_next`:
 *    1. Set `ret` to `TRUE`.
 *    2. Set `channel->read_pos` to `channel->read_pos_next`.
 * 4. Call `__pok_preemption_enable()`.
 * 5. Return `ret`.
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `__pok_preemption_enable()`
 */
pok_bool_t pok_channel_sampling_r_check_new_message(pok_channel_sampling_t* channel)
{
    pok_bool_t ret = FALSE;

    pok_preemption_disable();
    if(channel->read_pos != channel->read_pos_next)
    {
        ret = TRUE;
        // TODO: This mark message as consumed. Do we need that?
        channel->read_pos = channel->read_pos_next;
    }
    __pok_preemption_enable();

    return ret;
}

/**
 * @funcname `pok_channel_sampling_s_get_message`
 *
 * @description
 * Returns the buffer the sender writes its next message into.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_sampling_t *` | [in] The channel. |
 *
 * @return The buffer.
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
 * 1. Call `channel_sampling_message_at()` with the arguments `channel` and
 *    `channel->write_pos`, and return its result.
 *
 * @calls
 * - `channel_sampling_message_at()`
 */
char* pok_channel_sampling_s_get_message(
    pok_channel_sampling_t* channel)
{
    return channel_sampling_message_at(channel, channel->write_pos);
}

/**
 * @funcname `pok_channel_sampling_send_message`
 *
 * @description
 * Publishes the message written in the sender's buffer, with its size and the
 * current time, and gives the sender the buffer that is neither being read nor
 * published.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_sampling_t *` | [in,out] The channel. |
 * | `size` | `pok_message_size_t` | [in] The message's size (not 0). |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `read_pos_next` | `uint8_t` | The buffer just published |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Check that `size != 0U` holds (assert: stop otherwise).
 * 2. Call `pok_preemption_disable()`.
 * 3. Set `read_pos_next` to `channel->write_pos`.
 * 4. Set `channel->read_pos_next` to `read_pos_next`.
 * 5. Set `channel->timestamps[read_pos_next]` to `jet_system_time()`.
 * 6. Set `channel->message_sizes[read_pos_next]` to `size`.
 * 7. Set `channel->write_pos` to `3U - channel->read_pos - read_pos_next`.
 * 8. Call `__pok_preemption_enable()`.
 *
 * @calls
 * - `pok_assertion_fail()`
 * - `pok_preemption_disable()`
 * - `ja_system_time()`
 * - `__pok_preemption_enable()`
 */
void pok_channel_sampling_send_message(
    pok_channel_sampling_t* channel,
    pok_message_size_t size)
{
    uint8_t read_pos_next;

    assert(size != 0U);

    pok_preemption_disable();
    read_pos_next = channel->write_pos;
    channel->read_pos_next = read_pos_next;
    channel->timestamps[read_pos_next] = jet_system_time();
    channel->message_sizes[read_pos_next] = size;

    /*
     * The only choice for `write_pos`, which can differ from both
     * `read_pos` and `read_pos_next`.
     * 
     * (read_pos + read_pos_next + write_pos = 0 + 1 + 2 = 3).
     */
    channel->write_pos = 3U - channel->read_pos - read_pos_next;
    __pok_preemption_enable();
}

/**
 * @funcname `pok_channel_sampling_s_clear_message`
 *
 * @description
 * Withdraws the published message (when the OUT port is created): the next
 * message is the one being read.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `channel` | `pok_channel_sampling_t *` | [in,out] The channel. |
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
 * 1. Call `pok_preemption_disable()`.
 * 2. Set `channel->read_pos_next` to `channel->read_pos`.
 * 3. Call `__pok_preemption_enable()`.
 *
 * @calls
 * - `pok_preemption_disable()`
 * - `__pok_preemption_enable()`
 */
void pok_channel_sampling_s_clear_message(pok_channel_sampling_t* channel)
{
    pok_preemption_disable();
    channel->read_pos_next = channel->read_pos;
    __pok_preemption_enable();
}

/**
 * @funcname `pok_channels_init_all`
 *
 * @description
 * Initializes every queuing and sampling channel of the configuration at boot.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pok_channels_queuing` | `pok_channel_queuing_t[]` | The queuing channels (generated) |
 * | `pok_channels_queuing_n` | `uint8_t` | The number of queuing channels |
 * | `pok_channels_sampling` | `pok_channel_sampling_t[]` | The sampling channels (generated) |
 * | `pok_channels_sampling_n` | `uint8_t` | The number of sampling channels |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `uint32_t` | The index of a channel |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `pok_channels_queuing_n`,
 *    repeat (after each pass: increment `i`):
 *    1. Call `pok_channel_queuing_init()` with the address of
 *       `pok_channels_queuing[i]`.
 * 2. Set `i` to `0U`; then while `i` is less than `pok_channels_sampling_n`,
 *    repeat (after each pass: increment `i`):
 *    1. Call `pok_channel_sampling_init()` with the address of
 *       `pok_channels_sampling[i]`.
 *
 * @calls
 * - `pok_channel_queuing_init()`
 * - `pok_channel_sampling_init()`
 */
void pok_channels_init_all(void)
{
    uint32_t i;

    for(i = 0U; i < pok_channels_queuing_n; i++)
    {
        pok_channel_queuing_init(&pok_channels_queuing[i]);
    }

    for(i = 0U; i < pok_channels_sampling_n; i++)
    {
        pok_channel_sampling_init(&pok_channels_sampling[i]);
    }
}
