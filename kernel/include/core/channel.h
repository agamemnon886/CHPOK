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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: one declarator
 * per declaration.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented; struct pok_channel_queuing_side is the typedef
 * pok_channel_queuing_side_t; Doxygen comments for the file and its types (with
 * their members); enum jet_channel_queuing_overflow_strategy is the typedef
 * jet_queuing_overflow_t; pok_partition_generation_t is renamed
 * pok_part_generation_t (names too long for the tables of the documentation).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * jet_queuing_overflow_e (was jet_queuing_overflow_t); the Doxygen comment of
 * the file is removed (header files are not documented).
 */

/* 
 * Generic channels between partitions.
 */

#ifndef POK_KERNEL_CHANNEL_H
#define POK_KERNEL_CHANNEL_H

#include <types.h>

#include <core/partition.h>

/*********************** Queuing channel ******************************/

/**
 * @typename `pok_channel_queuing_side_t`
 *
 * @description
 * One side of the channel: receiver or sender.
 */
typedef struct {
    /**
     * Maximum number of messages on this side. Set in deployment.c.
     */
    pok_message_range_t max_nb_message;
    /**
     * Next message on this side:
     *
     * For receiver: the next message it will consume. For sender: the next
     * message it will fill.
     */
    pok_message_range_t next_message;

    /**
     * Partition corresponded for this side. Set in deployment.c.
     */
    pok_partition_t* part;

    /**
     * Generation of the side when it has its connection initialized.
     */
    pok_part_generation_t generation;

    /**
     * Whether needs to notify this side about messages/space available.
     */
    pok_bool_t is_notify;

    /**
     * Identificator for use in notification event. Set on port creation.
     */
    uint16_t handler_id;
} pok_channel_queuing_side_t;

/* What to do when receiving buffer is full and new message is sent. */
/**
 * @typename `jet_queuing_overflow_e`
 *
 * @description
 * What to do when receiving buffer is full and new message is sent.
 */
typedef enum {
    /**
     * Fill sender buffer.
     *
     * If it is full, sender should wait.
     */
    JET_CHANNEL_QUEUING_SENDER_BLOCK,
    /**
     * Discard new message.
     *
     * Receiver will be notified about that discarding.
     *
     * With that strategy no reason to have sender buffer to accomodate more
     * than single message.
     */
    JET_CHANNEL_QUEUING_RECEIVER_DISCARD
} jet_queuing_overflow_e;

/**
 * @typename `pok_channel_queuing_t`
 *
 * @description
 * Queuing channel between partitions.
 *
 * Receiver has buffer of received messages. Sender has buffer of messages ready
 * to send.
 *
 * Mesages in that channel are transmitted *instantly* unless receiver is not
 * ready or its buffer is full. In that case messages are accumulated on sender
 * side until it is possible to transmit them.
 */
typedef struct {
    /**
     * Maximum size of single message. Set in deployment.c.
     */
    pok_message_size_t max_message_size;
    /**
     * Distance between messages in the array.
     */
    pok_message_size_t message_stride;

    /* Sides of the channel. */
    pok_channel_queuing_side_t send; /**< Sides of the channel: the sender. */
    pok_channel_queuing_side_t recv; /**< Sides of the channel: the receiver. */

    pok_message_range_t max_nb_message; /**< Total buffer capasity. */

    char* messages; /**< Array of messages. */
    pok_message_size_t* message_sizes; /**< Array of messages sizes. */

    /**
     * Border between receiver and sender buffers.
     *
     * When buffer for a channel side is empty, .next_message field of the side
     * is equal to that border.
     */
    pok_message_size_t border;

    /**
     * Overflow strategy for given channel. Set in deployment.c.
     */
    jet_queuing_overflow_e overflow_strategy;
    /**
     * Flag is set when message is discarded. Flag is cleared after receiver is
     * notified about that.
     */
    pok_bool_t message_discarded;

} pok_channel_queuing_t;

/* 
 * Initialize queuing channel.
 * 
 * Fields should be set before calling this function:
 * 
 *   - max_message_size
 *   - send.max_nb_messages
 *   - recv.max_nb_messages
 */
/** \cond */
void pok_channel_queuing_init(pok_channel_queuing_t* channel);

/* 
 * (Re)initialize given side of the channel.
 * 
 * Should be called before all other functions for that side.
 */
void pok_channel_queuing_side_init(pok_channel_queuing_t* channel,
    pok_channel_queuing_side_t* side,
    uint16_t handler_id);

/** \endcond */


/**** Operations for receiver. Should be serialized wrt themselves ****/
/* (Re)initialize receiver connection to the channel. */
/** \cond */
void pok_channel_queuing_r_init(pok_channel_queuing_t* channel);

/*
 * Return number of messages on the receiver side.
 */
pok_message_range_t pok_channel_queuing_r_n_messages(pok_channel_queuing_t* channel);

/* 
 * Return pointer to the first message at receiver side.
 * 
 * If no message is available, return NULL.
 * 
 * In that case, if 'subscribe' is non-zero, receiver partition will be
 * notified when new message will be available.
 */
const char* pok_channel_queuing_r_get_message(
    pok_channel_queuing_t* channel,
    pok_message_size_t* size,
    pok_bool_t subscribe);

/* 
 * Consume the first message at receiver side.
 * 
 * Set 'message_discarded' parameter to one in the channel field,
 * and reset the field.
 */
void pok_channel_queuing_r_consume_message(
    pok_channel_queuing_t* channel,
    pok_bool_t* message_discarded);

/** \endcond */

/***** Operations for sender. Should be serialized wrt themselves *****/
/* 
 * Return pointer to the message for being filled at sender side.
 * 
 * Return NULL if no space is left in the sender buffer.
 * 
 * If there is no space in the sender buffer and @subscribe parameter
 * is TRUE, subscribe to notifications when the space will be released.
 */
/** \cond */
char* pok_channel_queuing_s_get_message(
    pok_channel_queuing_t* channel,
    pok_bool_t subscribe);

/*
 * Mark (already filled) message as produced.
 * 
 * If possible, the message is sent immediately.
 * Otherwise it will be automatically sent when there will be sufficient
 * space in the receiver buffer.
 */
void pok_channel_queuing_s_produce_message(
    pok_channel_queuing_t* channel,
    pok_message_size_t size);

/*
 * Return number of messages on the sender side.
 * 
 * NOTE: Returning value is always 0 until receiver is not ready or
 * its buffer is full.
 */
pok_message_range_t pok_channel_queuing_s_n_messages(pok_channel_queuing_t* channel);

/** \endcond */

/*********************** Sampling channel *****************************/

/**
 * @typename `pok_channel_sampling_t`
 *
 * @description
 * Sampling channel.
 *
 * Receiver has one message slot for (possibly) currently processed message, and
 * one message slot for newest message received.
 *
 * Sender has one message slot for currently formed message.
 *
 * Mesages in that channel are transmitted *instantly*.
 *
 * Actually, ARINC distinguish message which is *sent* or *received*: message is
 * treated as sent until it is received.
 */
typedef struct {
    /**
     * Maximum size of the message. Set in deployment.c.
     */
    pok_message_size_t max_message_size;
    /**
     * Distance between messages in the array.
     */
    pok_message_size_t message_stride;
    /**
     * Array of 3 messages.
     */
    char* messages;
    /**
     * Array of message sizes.
     */
    pok_message_size_t message_sizes[3];

    // Positions in range 0..2
    /**
     * The slot of the message being read (positions in range 0..2).
     */
    uint8_t read_pos;
    /**
     * The slot of the newest message received (positions in range 0..2).
     */
    uint8_t read_pos_next;
    /**
     * The slot of the message being written (positions in range 0..2).
     */
    uint8_t write_pos;

    /**
     * The simplest implementation: timestamp per message.
     */
    pok_time_t timestamps[3];
} pok_channel_sampling_t;

/* 
 * Initialize sampling channel.
 * 
 * Fields should be set before calling this function:
 * 
 *   - max_message_size
 */
/** \cond */
void pok_channel_sampling_init(pok_channel_sampling_t* channel);

/** \endcond */


/**** Operations for receiver. Should be serialized wrt themselves ****/

/*
 * Get pointer to the message for read it.
 * 
 * `timestamp` will be filled with time, when message has been sent.
 * 
 * If there is no message available for read, return NULL.
 * 
 * NOTE: Every new function's call invalidates previous return value.
 * Using this semantic, receiver doesn't need to mark message as consumed.
 */
/** \cond */
const char* pok_channel_sampling_r_get_message(
    pok_channel_sampling_t* channel,
    pok_message_size_t* size,
    pok_time_t* timestamp);

/*
 * Clear message received.
 */
void pok_channel_sampling_r_clear_message(pok_channel_sampling_t* channel);

/*
 * Return POK_ERRNO_OK if new message has been arrive since we check(read).
 * 
 * Return POK_ERRNO_EMPTY otherwise.
 */
pok_bool_t pok_channel_sampling_r_check_new_message(pok_channel_sampling_t* channel);

/** \endcond */

/***** Operations for sender. Should be serialized wrt themselves *****/

/*
 * Get pointer to the message for fill it.
 */
/** \cond */
char* pok_channel_sampling_s_get_message(
    pok_channel_sampling_t* channel);

/*
 * Send message which has been fully filled.
 */
void pok_channel_sampling_send_message(
    pok_channel_sampling_t* channel,
    pok_message_size_t size);

/*
 * Clear message sent.
 */
void pok_channel_sampling_s_clear_message(pok_channel_sampling_t* channel);

/** \endcond */

/**********************************************************************/
/* 
 * Array of queuing channels.
 * 
 * Should be set in deployment.c.
 */
extern pok_channel_queuing_t pok_channels_queuing[];

/*
 * Number of queuing channels.
 * 
 * Should be set in deployment.c.
 */
extern uint8_t pok_channels_queuing_n;

/* 
 * Array of sampling channels.
 * 
 * Should be set in deployment.c.
 */
extern pok_channel_sampling_t pok_channels_sampling[];

/*
 * Number of sampling channels.
 * 
 * Should be set in deployment.c.
 */
extern uint8_t pok_channels_sampling_n;

/** \cond */
void pok_channels_init_all(void);

/** \endcond */

#endif /* POK_KERNEL_CHANNEL_H */
