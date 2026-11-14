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
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: msection_t, msection_wq_t.
 */

#ifndef LIBJET_ARINC_BUFFER_H
#define LIBJET_ARINC_BUFFER_H

#include <config.h>

#ifdef POK_NEEDS_ARINC653_BUFFER
#include <arinc653/types.h>
#include <arinc653/buffer.h>
#include <msection.h>
#include <types.h>

/**
 * @typename `arinc_buffer_t`
 *
 * @description
 * A buffer of the partition (CREATE_BUFFER()): a ring of messages, in the
 * preallocated array arinc_buffers.
 */
typedef struct {
    BUFFER_NAME_TYPE buffer_name; /**< The name of the buffer. */
    
    MESSAGE_SIZE_TYPE max_message_size; /**< The maximum size of a message. */
    /**
     * The capacity of the buffer, in messages.
     */
    MESSAGE_RANGE_TYPE max_nb_message;
    MESSAGE_RANGE_TYPE nb_message; /**< The number of messages in the buffer. */
    
    /**
     * The distance between two messages in messages: max_message_size, aligned
     * for int32_t.
     */
    MESSAGE_SIZE_TYPE message_stride;
    
    /**
     * The messages (max_nb_message of message_stride bytes).
     */
    char* messages;
    /**
     * The size of each message (max_nb_message entries).
     */
    MESSAGE_SIZE_TYPE* messages_size;
    
    /**
     * The index of the oldest message in the ring.
     */
    MESSAGE_RANGE_TYPE base_offset;
    
    /**
     * The order of the waiting processes (FIFO or PRIORITY).
     */
    QUEUING_DISCIPLINE_TYPE discipline;
    
    /**
     * Protects the buffer (a critical section of the partition's processes).
     */
    msection_t section;
    /**
     * The processes waiting to send or receive.
     */
    msection_wq_t process_queue;
} arinc_buffer_t;

/* Preallocated array of buffers. */
extern arinc_buffer_t* arinc_buffers;

#endif /* POK_NEEDS_ARINC653_BUFFER */

#endif /* LIBJET_ARINC_BUFFER_H */
