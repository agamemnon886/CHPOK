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
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}:
 *   `pok_channel_queuing_init()` shall give a queuing channel, at boot, the
 *   room for the messages of both of its sides, empty.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_009}
 *   - \srqref{SRQ_JET_MEM_010}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}:
 *   `pok_channel_queuing_init()` shall set `max_nb_message` of `channel` to the
 *   sum of `max_nb_message` of `recv` and of `send`, `next_message` of `send`
 *   and of `recv` and `border` to 0, and `message_stride` to `max_message_size`
 *   rounded up to the alignment of `int32_t` (`ALIGN_VAL()`).
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_02}:
 *   `pok_channel_queuing_init()` shall set `messages` of `channel` to the
 *   result of `ja_mem_alloc_aligned()` for `message_stride` times
 *   `max_nb_message` and the alignment of `int32_t`, `message_sizes` to the
 *   result of `ja_mem_alloc_aligned()` for the size of a message size times
 *   `max_nb_message` and its alignment, `generation` of `send` and of `recv` to
 *   0, their `is_notify` to `FALSE` and `message_discarded` to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}
 *
 * @design
 * 1. Set `channel->max_nb_message` to
 *    `channel->recv.max_nb_message + channel->send.max_nb_message`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}
 * 2. Set `channel->send.next_message` to `0`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}
 * 3. Set `channel->recv.next_message` to `0`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}
 * 4. Set `channel->border` to `0`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}
 * 5. Set `message_alignment` to `__alignof__(int32_t)`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}
 * 6. Call `ALIGN_VAL()` with the arguments `channel->max_message_size` and
 *    `message_alignment`, and store the result in `channel->message_stride`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_01}
 * 7. Call `ja_mem_alloc_aligned()` with the arguments
 *    `channel->message_stride * channel->max_nb_message` and
 *    `message_alignment`, and store the result in `channel->messages`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_02}
 * 8. Call `ja_mem_alloc_aligned()` with the arguments
 *    `sizeof(*channel->message_sizes) * channel->max_nb_message` and
 *    `__alignof__(*channel->message_sizes)`, and store the result in
 *    `channel->message_sizes`. \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_02}
 * 9. Set `channel->send.generation` to `0`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_02}
 * 10. Set `channel->send.is_notify` to `FALSE`.
 *     \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_02}
 * 11. Set `channel->recv.generation` to `0`.
 *     \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_02}
 * 12. Set `channel->recv.is_notify` to `FALSE`.
 *     \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_02}
 * 13. Set `channel->message_discarded` to `FALSE`.
 *     \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_INIT_02}
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
 * - \hlrid{HLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_01}:
 *   `channel_queuing_cyclic_add()` shall give the position that follows a
 *   position of the ring of a queuing channel by a number of messages, wrapping
 *   around.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_009}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_01}: If `pos` plus `index` is
 *   less than `max_nb_message` of `channel`, `channel_queuing_cyclic_add()`
 *   shall return `pos` plus `index`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_01}
 * - \llrid{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_02}: If `pos` plus `index` is
 *   not less than `max_nb_message` of `channel`, `channel_queuing_cyclic_add()`
 *   shall return `pos` plus `index` minus `max_nb_message`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_01}
 *
 * @design
 * 1. Set `sum` to `pos + index`.
 *    \llrref{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_01},
 *    \llrref{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_02}
 * 2. If `sum` is greater than or equal to `channel->max_nb_message`:
 *    \llrref{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_02}
 *    1. Subtract `channel->max_nb_message` from `sum`.
 * 3. Return `sum`. \llrref{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_01},
 *    \llrref{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_ADD_02}
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
 * - \hlrid{HLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_SUB_01}:
 *   `channel_queuing_cyclic_sub()` shall give the number of messages from one
 *   position of the ring of a queuing channel to another.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_009}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_SUB_01}: If `pos_end` is not less
 *   than `pos_start`, `channel_queuing_cyclic_sub()` shall return `pos_end`
 *   minus `pos_start`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_SUB_01}
 * - \llrid{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_SUB_02}: If `pos_end` is less
 *   than `pos_start`, `channel_queuing_cyclic_sub()` shall return `pos_end`
 *   plus `max_nb_message` of `channel` minus `pos_start`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_SUB_01}
 *
 * @design
 * 1. Return `(pos_end - pos_start)` if `pos_end` is greater than or equal to
 *    `pos_start`, otherwise `(pos_end + channel->max_nb_message - pos_start)`.
 *    \llrref{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_SUB_01},
 *    \llrref{LLR_CHANNEL_CHANNEL_QUEUING_CYCLIC_SUB_02}
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
 * - \hlrid{HLR_CHANNEL_CHANNEL_QUEUING_MESSAGE_AT_01}:
 *   `channel_queuing_message_at()` shall give the buffer of the message at a
 *   position of the ring of a queuing channel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_009}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_CHANNEL_QUEUING_MESSAGE_AT_01}:
 *   `channel_queuing_message_at()` shall return the address of the element
 *   `message_stride` times `pos` of `messages` of `channel`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_CHANNEL_QUEUING_MESSAGE_AT_01}
 *
 * @design
 * 1. Return the address of `channel->messages[channel->message_stride * pos]`.
 *    \llrref{LLR_CHANNEL_CHANNEL_QUEUING_MESSAGE_AT_01}
 *
 * @calls
 * None.
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
 * - \hlrid{HLR_CHANNEL_CHANNEL_QUEUING_SIDE_NOTIFY_01}:
 *   `channel_queuing_side_notify()` shall tell the partition of a side of a
 *   queuing channel, by an event, that the channel made its port usable, if
 *   that side asked for it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_013}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_CHANNEL_QUEUING_SIDE_NOTIFY_01}: If and only if
 *   `is_notify` of `side` is true, `channel_queuing_side_notify()` shall set it
 *   to `FALSE` and call `pok_partition_add_event()` once with `part` of `side`,
 *   `event_type` and `handler_id` of `side`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_CHANNEL_QUEUING_SIDE_NOTIFY_01}
 *
 * @design
 * 1. If `side->is_notify` is true:
 *    \llrref{LLR_CHANNEL_CHANNEL_QUEUING_SIDE_NOTIFY_01}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_SIDE_INIT_01}:
 *   `pok_channel_queuing_side_init()` shall attach one side of a queuing
 *   channel to the port of the current start of its partition, when the port is
 *   created or cleared, with no message for it to take.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_002}
 *   - \srqref{SRQ_JET_IPC_015}
 *   - \srqref{SRQ_JET_IPC_004}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_SIDE_INIT_01}:
 *   `pok_channel_queuing_side_init()` shall call `pok_preemption_disable()`,
 *   set `next_message` of `side` to `border` of `channel`, its `is_notify` to
 *   `FALSE`, its `generation` to `partition_generation` of its `part` and its
 *   `handler_id` to `handler_id`, and call `__pok_preemption_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_SIDE_INIT_01}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_SIDE_INIT_01}
 * 2. Set `side->next_message` to `channel->border`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_SIDE_INIT_01}
 * 3. Set `side->is_notify` to `FALSE`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_SIDE_INIT_01}
 * 4. Set `side->generation` to `side->part->partition_generation`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_SIDE_INIT_01}
 * 5. Set `side->handler_id` to `handler_id`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_SIDE_INIT_01}
 * 6. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_SIDE_INIT_01}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_N_MESSAGES_01}:
 *   `pok_channel_queuing_r_n_messages()` shall give the number of messages that
 *   the receiver of a queuing channel may take.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_003}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_N_MESSAGES_01}:
 *   `pok_channel_queuing_r_n_messages()` shall call `pok_preemption_disable()`,
 *   then `channel_queuing_cyclic_sub()` with `channel`, `border` and
 *   `recv.next_message`, then `__pok_preemption_enable()`, and return the
 *   result of `channel_queuing_cyclic_sub()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_N_MESSAGES_01}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_N_MESSAGES_01}
 * 2. Call `channel_queuing_cyclic_sub()` with the arguments `channel`,
 *    `channel->border` and `channel->recv.next_message`, and store the result
 *    in `n_messages`. \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_N_MESSAGES_01}
 * 3. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_N_MESSAGES_01}
 * 4. Return `n_messages`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_N_MESSAGES_01}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_01}:
 *   `pok_channel_queuing_r_get_message()` shall give the first message that the
 *   receiver of a queuing channel may take, with its size, without taking it,
 *   or none.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_009}
 *   - \srqref{SRQ_JET_IPC_011}
 *   - \srqref{SRQ_JET_IPC_021}
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_02}:
 *   `pok_channel_queuing_r_get_message()` shall, when there is no message, let
 *   the receiver ask to be told when one comes.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_013}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_01}:
 *   `pok_channel_queuing_r_get_message()` shall call `pok_preemption_disable()`
 *   before it reads the channel and `__pok_preemption_enable()` after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_02}: If
 *   `recv.next_message` of `channel` is not `border`,
 *   `pok_channel_queuing_r_get_message()` shall store
 *   `message_sizes[recv.next_message]` at `size` and return the result of
 *   `channel_queuing_message_at()` for `recv.next_message`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_03}: If
 *   `recv.next_message` is `border` and `subscribe` is true,
 *   `pok_channel_queuing_r_get_message()` shall set `recv.is_notify` to `TRUE`
 *   and return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_02}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_04}: If
 *   `recv.next_message` is `border` and `subscribe` is false,
 *   `pok_channel_queuing_r_get_message()` shall leave `recv.is_notify` as it is
 *   and return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_02}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_01}
 * 2. If `channel->recv.next_message` is not equal to `channel->border`:
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_02}
 *    1. Call `channel_queuing_message_at()` with the arguments `channel` and
 *       `channel->recv.next_message`, and store the result in `message`.
 *    2. Set `*size` to `channel->message_sizes[channel->recv.next_message]`.
 * 3. Otherwise: \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_03},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_04}
 *    1. Set `message` to `NULL`.
 *    2. If `subscribe` is true:
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_03},
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_04}
 *       1. Set `channel->recv.is_notify` to `TRUE`.
 *          \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_03}
 * 4. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_01}
 * 5. Return `message`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_02},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_03},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_GET_MESSAGE_04}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_01}:
 *   `pok_channel_queuing_r_consume_message()` shall remove the first message
 *   that the receiver of a queuing channel may take, so that it is delivered
 *   once.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_009}
 *   - \srqref{SRQ_JET_IPC_011}
 *   - \srqref{SRQ_JET_IPC_021}
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_02}:
 *   `pok_channel_queuing_r_consume_message()` shall give the receiver the next
 *   message that the sender holds and tell the sender that it has room again.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_013}
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_03}:
 *   `pok_channel_queuing_r_consume_message()` shall tell the receiver whether
 *   messages were discarded since its last receive.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_022}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_01}:
 *   `pok_channel_queuing_r_consume_message()` shall call
 *   `pok_preemption_disable()` first, set `recv.next_message` of `channel` to
 *   the result of `channel_queuing_cyclic_add()` for it and 1, and call
 *   `pok_preemption_enable()` at the end.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_02}: If
 *   `partition_generation` of `send.part` is `send.generation` and `border` is
 *   not `send.next_message`, `pok_channel_queuing_r_consume_message()` shall
 *   set `border` to the result of `channel_queuing_cyclic_add()` for it and 1,
 *   and call `channel_queuing_side_notify()` with `send` and
 *   `JET_PARTITION_EVENT_TYPE_PORT_SEND_AVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_02}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_03}: If
 *   `partition_generation` of `send.part` is not `send.generation`, or `border`
 *   is `send.next_message`, `pok_channel_queuing_r_consume_message()` shall
 *   leave `border` as it is and not call `channel_queuing_side_notify()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_02}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_04}:
 *   `pok_channel_queuing_r_consume_message()` shall store `message_discarded`
 *   of `channel` at `message_discarded` and set it to `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_03}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_01}
 * 2. Call `channel_queuing_cyclic_add()` with the arguments `channel`,
 *    `channel->recv.next_message` and `1`, and store the result in
 *    `channel->recv.next_message`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_01}
 * 3. If `channel->send.part->partition_generation` is equal to
 *    `channel->send.generation` and `channel->border` is not equal to
 *    `channel->send.next_message`:
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_02},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_03}
 *    1. Call `channel_queuing_cyclic_add()` with the arguments `channel`,
 *       `channel->border` and `1`, and store the result in `channel->border`.
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_02}
 *    2. Call `channel_queuing_side_notify()` with the arguments the address of
 *       `channel->send` and `JET_PARTITION_EVENT_TYPE_PORT_SEND_AVAILABLE`.
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_02}
 * 4. Set `*message_discarded` to `channel->message_discarded`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_04}
 * 5. Set `channel->message_discarded` to `FALSE`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_04}
 * 6. Call `pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_R_CONSUME_MESSAGE_01}
 *
 * @calls
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_01}:
 *   `pok_channel_queuing_s_get_message()` shall give the sender of a queuing
 *   channel the room for a new message, or none if its side is full.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_010}
 *   - \srqref{SRQ_JET_IPC_020}
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_02}:
 *   `pok_channel_queuing_s_get_message()` shall, when there is no room, let the
 *   sender ask to be told when there is.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_013}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_01}:
 *   `pok_channel_queuing_s_get_message()` shall call `pok_preemption_disable()`
 *   before it reads the channel and `__pok_preemption_enable()` after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_02}: If the number of
 *   messages from `border` to `send.next_message`
 *   (`channel_queuing_cyclic_sub()`) is less than `send.max_nb_message`,
 *   `pok_channel_queuing_s_get_message()` shall return the result of
 *   `channel_queuing_message_at()` for `send.next_message`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_03}: If that number is
 *   not less than `send.max_nb_message` and `subscribe` is true,
 *   `pok_channel_queuing_s_get_message()` shall set `send.is_notify` to `TRUE`
 *   and return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_02}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_04}: If that number is
 *   not less than `send.max_nb_message` and `subscribe` is false,
 *   `pok_channel_queuing_s_get_message()` shall leave `send.is_notify` as it is
 *   and return NULL.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_02}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_01}
 * 2. Call `channel_queuing_cyclic_sub()` with the arguments `channel`,
 *    `channel->send.next_message` and `channel->border`, and store the result
 *    in `n_used`. \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_02},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_03},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_04}
 * 3. If `n_used` is less than `channel->send.max_nb_message`:
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_02}
 *    1. Call `channel_queuing_message_at()` with the arguments `channel` and
 *       `channel->send.next_message`, and store the result in `message`.
 * 4. Otherwise: \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_03},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_04}
 *    1. Set `message` to `NULL`.
 *    2. If `subscribe` is true:
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_03},
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_04}
 *       1. Set `channel->send.is_notify` to `TRUE`.
 *          \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_03}
 * 5. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_01}
 * 6. Return `message`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_02},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_03},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_GET_MESSAGE_04}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_01}:
 *   `pok_channel_queuing_s_produce_message()` shall add the message that the
 *   sender wrote to the queue of a queuing channel, after the messages sent
 *   before, and give it to the receiver when the receiver has room.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_009}
 *   - \srqref{SRQ_JET_IPC_010}
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_02}:
 *   `pok_channel_queuing_s_produce_message()` shall, when the receiver has no
 *   room and the channel discards on overflow, discard the new message and note
 *   it for the receiver.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_014}
 *   - \srqref{SRQ_JET_IPC_022}
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_03}:
 *   `pok_channel_queuing_s_produce_message()` shall tell the receiver that a
 *   message came, if it asked for it.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_013}
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_04}:
 *   `pok_channel_queuing_s_produce_message()` shall not let a receiver of an
 *   old generation of its partition hold the sender back.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_004}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_01}:
 *   `pok_channel_queuing_s_produce_message()` shall call
 *   `pok_preemption_disable()` first, set `message_sizes[send.next_message]` of
 *   `channel` to `size`, set `send.next_message` to the result of
 *   `channel_queuing_cyclic_add()` for it and 1, and call
 *   `pok_preemption_enable()` at the end.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_02}: If
 *   `partition_generation` of `recv.part` is `recv.generation` and the number
 *   of messages from `recv.next_message` to `border`
 *   (`channel_queuing_cyclic_sub()`) is less than `recv.max_nb_message`,
 *   `pok_channel_queuing_s_produce_message()` shall set `border` to
 *   `send.next_message` and call `channel_queuing_side_notify()` with `recv`
 *   and `JET_PARTITION_EVENT_TYPE_PORT_RECEIVE_AVAILABLE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_01}
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_03}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_03}: If
 *   `partition_generation` of `recv.part` is `recv.generation`, that number is
 *   not less than `recv.max_nb_message` and `overflow_strategy` is
 *   `JET_CHANNEL_QUEUING_RECEIVER_DISCARD`,
 *   `pok_channel_queuing_s_produce_message()` shall set `send.next_message` to
 *   `border` and `message_discarded` to `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_02}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_04}: If
 *   `partition_generation` of `recv.part` is `recv.generation`, that number is
 *   not less than `recv.max_nb_message` and `overflow_strategy` is not
 *   `JET_CHANNEL_QUEUING_RECEIVER_DISCARD`,
 *   `pok_channel_queuing_s_produce_message()` shall leave `border`,
 *   `send.next_message` and `message_discarded` as they are after the advance.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_02}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_05}: If
 *   `partition_generation` of `recv.part` is not `recv.generation`,
 *   `pok_channel_queuing_s_produce_message()` shall set `border` to
 *   `send.next_message` without calling `channel_queuing_side_notify()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_04}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_01}
 * 2. Set `channel->message_sizes[channel->send.next_message]` to `size`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_01}
 * 3. Call `channel_queuing_cyclic_add()` with the arguments `channel`,
 *    `channel->send.next_message` and `1`, and store the result in
 *    `channel->send.next_message`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_01}
 * 4. If `channel->recv.part->partition_generation` is equal to
 *    `channel->recv.generation`:
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_02},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_03},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_04},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_05}
 *    1. Call `channel_queuing_cyclic_sub()` with the arguments `channel`,
 *       `channel->border` and `channel->recv.next_message`, and store the
 *       result in `nb_message_receive`.
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_02},
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_03},
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_04}
 *    2. If `nb_message_receive` is less than `channel->recv.max_nb_message`:
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_02}
 *       1. Set `channel->border` to `channel->send.next_message`.
 *       2. Call `channel_queuing_side_notify()` with the arguments the address
 *          of `channel->recv` and
 *          `JET_PARTITION_EVENT_TYPE_PORT_RECEIVE_AVAILABLE`.
 *    3. Otherwise, if `channel->overflow_strategy` is equal to
 *       `JET_CHANNEL_QUEUING_RECEIVER_DISCARD`:
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_03},
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_04}
 *       1. Set `channel->send.next_message` to `channel->border`.
 *          \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_03}
 *       2. Set `channel->message_discarded` to `TRUE`.
 *          \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_03}
 * 5. Otherwise: \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_05}
 *    1. Set `channel->border` to `channel->send.next_message`.
 * 6. Call `pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_PRODUCE_MESSAGE_01}
 *
 * @calls
 * - `channel_queuing_cyclic_sub()`
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_N_MESSAGES_01}:
 *   `pok_channel_queuing_s_n_messages()` shall give the number of messages that
 *   the sender of a queuing channel holds, not yet given to the receiver.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_003}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_N_MESSAGES_01}:
 *   `pok_channel_queuing_s_n_messages()` shall call `pok_preemption_disable()`,
 *   then `channel_queuing_cyclic_sub()` with `channel`, `send.next_message` and
 *   `border`, then `__pok_preemption_enable()`, and return the result of
 *   `channel_queuing_cyclic_sub()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_QUEUING_S_N_MESSAGES_01}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_N_MESSAGES_01}
 * 2. Call `channel_queuing_cyclic_sub()` with the arguments `channel`,
 *    `channel->send.next_message` and `channel->border`, and store the result
 *    in `n_messages`. \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_N_MESSAGES_01}
 * 3. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_N_MESSAGES_01}
 * 4. Return `n_messages`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_QUEUING_S_N_MESSAGES_01}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}:
 *   `pok_channel_sampling_init()` shall give a sampling channel, at boot, three
 *   message buffers and no message.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_006}
 *   - \srqref{SRQ_JET_MEM_010}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}:
 *   `pok_channel_sampling_init()` shall set `message_stride` of `channel` to
 *   `max_message_size` rounded up to the alignment of `int32_t`
 *   (`ALIGN_VAL()`), `messages` to the result of `ja_mem_alloc_aligned()` for 3
 *   times `message_stride` and that alignment, `read_pos_next` and `read_pos`
 *   to 0, `message_sizes[0]` to 0 and `write_pos` to 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}
 *
 * @design
 * 1. Set `message_alignment` to `__alignof__(int32_t)`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}
 * 2. Call `ALIGN_VAL()` with the arguments `channel->max_message_size` and
 *    `message_alignment`, and store the result in `channel->message_stride`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}
 * 3. Call `ja_mem_alloc_aligned()` with the arguments
 *    `3U * channel->message_stride` and `message_alignment`, and store the
 *    result in `channel->messages`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}
 * 4. Set `channel->read_pos_next` to `0`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}
 * 5. Set `channel->read_pos` to `0`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}
 * 6. Set `channel->message_sizes[0]` to `0`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}
 * 7. Set `channel->write_pos` to `1`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_INIT_01}
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
 * - \hlrid{HLR_CHANNEL_CHANNEL_SAMPLING_MESSAGE_AT_01}:
 *   `channel_sampling_message_at()` shall give one of the three buffers of a
 *   sampling channel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_006}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_CHANNEL_SAMPLING_MESSAGE_AT_01}:
 *   `channel_sampling_message_at()` shall return the address of the element
 *   `pos` times `message_stride` of `messages` of `channel`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_CHANNEL_SAMPLING_MESSAGE_AT_01}
 *
 * @design
 * 1. Return the address of
 *    `channel->messages[(size_t)pos * channel->message_stride]`.
 *    \llrref{LLR_CHANNEL_CHANNEL_SAMPLING_MESSAGE_AT_01}
 *
 * @calls
 * None.
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_01}:
 *   `pok_channel_sampling_r_get_message()` shall give the latest message of a
 *   sampling channel, with its size and time of writing, without removing it,
 *   or none.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_007}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_01}:
 *   `pok_channel_sampling_r_get_message()` shall set `read_pos` of `channel` to
 *   `read_pos_next`, between a call of `pok_preemption_disable()` and a call of
 *   `__pok_preemption_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_02}: If
 *   `message_sizes[read_pos]` is 0, `pok_channel_sampling_r_get_message()`
 *   shall return NULL without writing at `size` and `timestamp`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_03}: If
 *   `message_sizes[read_pos]` is not 0, `pok_channel_sampling_r_get_message()`
 *   shall store it at `size` and `timestamps[read_pos]` at `timestamp`, and
 *   return the result of `channel_sampling_message_at()` for `read_pos`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_01}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_01}
 * 2. Set `channel->read_pos` to `channel->read_pos_next`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_01}
 * 3. Set `read_pos` to `channel->read_pos`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_01}
 * 4. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_01}
 * 5. Set `message_size` to `channel->message_sizes[read_pos]`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_02},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_03}
 * 6. If `message_size` is equal to `0U`:
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_02},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_03}
 *    1. Return `NULL`.
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_02}
 * 7. Set `*size` to `message_size`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_03}
 * 8. Set `*timestamp` to `channel->timestamps[read_pos]`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_03}
 * 9. Call `channel_sampling_message_at()` with the arguments `channel` and
 *    `read_pos`, and return its result.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_GET_MESSAGE_03}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CLEAR_MESSAGE_01}:
 *   `pok_channel_sampling_r_clear_message()` shall clear the message being read
 *   of a sampling channel when its destination port is created.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_002}
 *   - \srqref{SRQ_JET_IPC_004}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CLEAR_MESSAGE_01}:
 *   `pok_channel_sampling_r_clear_message()` shall call
 *   `pok_preemption_disable()`, set `message_sizes[read_pos]` of `channel` to
 *   0, and call `__pok_preemption_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CLEAR_MESSAGE_01}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CLEAR_MESSAGE_01}
 * 2. Set `channel->message_sizes[channel->read_pos]` to `0`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CLEAR_MESSAGE_01}
 * 3. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CLEAR_MESSAGE_01}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_01}:
 *   `pok_channel_sampling_r_check_new_message()` shall tell whether a message
 *   was written to a sampling channel since it was last read, and make it the
 *   one being read.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_008}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_01}:
 *   `pok_channel_sampling_r_check_new_message()` shall call
 *   `pok_preemption_disable()` before it reads the positions and
 *   `__pok_preemption_enable()` after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_02}: If
 *   `read_pos` of `channel` is not `read_pos_next`,
 *   `pok_channel_sampling_r_check_new_message()` shall set `read_pos` to
 *   `read_pos_next` and return `TRUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_01}
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_03}: If
 *   `read_pos` of `channel` is `read_pos_next`,
 *   `pok_channel_sampling_r_check_new_message()` shall return `FALSE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_01}
 *
 * @design
 * 1. Set `ret` to `FALSE`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_03}
 * 2. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_01}
 * 3. If `channel->read_pos` is not equal to `channel->read_pos_next`:
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_02},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_03}
 *    1. Set `ret` to `TRUE`.
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_02}
 *    2. Set `channel->read_pos` to `channel->read_pos_next`.
 *       \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_02}
 * 4. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_01}
 * 5. Return `ret`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_02},
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_R_CHECK_NEW_MESSAGE_03}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_SAMPLING_S_GET_MESSAGE_01}:
 *   `pok_channel_sampling_s_get_message()` shall give the buffer that the
 *   sender of a sampling channel writes its next message into.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_018}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_S_GET_MESSAGE_01}:
 *   `pok_channel_sampling_s_get_message()` shall return the result of
 *   `channel_sampling_message_at()` for `channel` and its `write_pos`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_S_GET_MESSAGE_01}
 *
 * @design
 * 1. Call `channel_sampling_message_at()` with the arguments `channel` and
 *    `channel->write_pos`, and return its result.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_S_GET_MESSAGE_01}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}:
 *   `pok_channel_sampling_send_message()` shall publish the message that the
 *   sender wrote, with its size and the present time, as the latest message of
 *   a sampling channel, replacing the one before, and give the sender a buffer
 *   that is neither being read nor published.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_006}
 *   - \srqref{SRQ_JET_IPC_018}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}:
 *   `pok_channel_sampling_send_message()` shall call
 *   `pok_preemption_disable()`, set `read_pos_next` of `channel` to
 *   `write_pos`, set `timestamps` and `message_sizes` of that position to the
 *   result of `jet_system_time()` and `size`, set `write_pos` to 3 minus
 *   `read_pos` minus that position, and call `__pok_preemption_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}
 * 2. Set `read_pos_next` to `channel->write_pos`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}
 * 3. Set `channel->read_pos_next` to `read_pos_next`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}
 * 4. Set `channel->timestamps[read_pos_next]` to `jet_system_time()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}
 * 5. Set `channel->message_sizes[read_pos_next]` to `size`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}
 * 6. Set `channel->write_pos` to `3U - channel->read_pos - read_pos_next`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}
 * 7. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_SEND_MESSAGE_01}
 *
 * @calls
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNEL_SAMPLING_S_CLEAR_MESSAGE_01}:
 *   `pok_channel_sampling_s_clear_message()` shall withdraw the published
 *   message of a sampling channel that was not read yet, when its source port
 *   is created.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_IPC_002}
 *   - \srqref{SRQ_JET_IPC_004}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNEL_SAMPLING_S_CLEAR_MESSAGE_01}:
 *   `pok_channel_sampling_s_clear_message()` shall call
 *   `pok_preemption_disable()`, set `read_pos_next` of `channel` to `read_pos`,
 *   and call `__pok_preemption_enable()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNEL_SAMPLING_S_CLEAR_MESSAGE_01}
 *
 * @design
 * 1. Call `pok_preemption_disable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_S_CLEAR_MESSAGE_01}
 * 2. Set `channel->read_pos_next` to `channel->read_pos`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_S_CLEAR_MESSAGE_01}
 * 3. Call `__pok_preemption_enable()`.
 *    \llrref{LLR_CHANNEL_POK_CHANNEL_SAMPLING_S_CLEAR_MESSAGE_01}
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
 * - \hlrid{HLR_CHANNEL_POK_CHANNELS_INIT_ALL_01}: `pok_channels_init_all()`
 *   shall initialize every queuing and sampling channel of the configuration at
 *   boot.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_BOOT_002}
 *   - \srqref{SRQ_JET_IPC_001}
 *   - \srqref{SRQ_JET_IPC_016}
 *
 * @llr
 * - \llrid{LLR_CHANNEL_POK_CHANNELS_INIT_ALL_01}: `pok_channels_init_all()`
 *   shall call `pok_channel_queuing_init()` for each of the
 *   `pok_channels_queuing_n` queuing channels and `pok_channel_sampling_init()`
 *   for each of the `pok_channels_sampling_n` sampling channels.
 *   <br>Traces to:
 *   - \hlrref{HLR_CHANNEL_POK_CHANNELS_INIT_ALL_01}
 *
 * @design
 * 1. Set `i` to `0U`; then while `i` is less than `pok_channels_queuing_n`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_CHANNEL_POK_CHANNELS_INIT_ALL_01}
 *    1. Call `pok_channel_queuing_init()` with the address of
 *       `pok_channels_queuing[i]`.
 * 2. Set `i` to `0U`; then while `i` is less than `pok_channels_sampling_n`,
 *    repeat (after each pass: increment `i`):
 *    \llrref{LLR_CHANNEL_POK_CHANNELS_INIT_ALL_01}
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
