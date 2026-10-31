/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (syspart/components/arinc/config.yaml).
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
 */

#ifndef __ARINC_SENDER_GEN_H__
#define __ARINC_SENDER_GEN_H__

    #include <arinc653/queueing.h>
    #include <arinc653/sampling.h>
    #include <port_info.h>


    #include <interfaces/preallocated_sender_gen.h>

typedef struct ARINC_SENDER_state {
    NAME_TYPE port_name;
    PORT_DIRECTION_TYPE port_direction;
    uint32_t overhead;
    MESSAGE_SIZE_TYPE port_max_message_size;
    int32_t is_queuing_port;
    MESSAGE_RANGE_TYPE q_port_max_nb_messages;
    APEX_INTEGER port_id;
    sys_port_data_t * port_buffer;
}ARINC_SENDER_state;

typedef struct {
    ARINC_SENDER_state state;
    struct {
    } in;
    struct {
            struct {
                preallocated_sender *ops;
                self_t *owner;
            } portA;
    } out;
} ARINC_SENDER;




      ret_t ARINC_SENDER_call_portA_send(ARINC_SENDER *, char *, size_t, size_t);
      ret_t ARINC_SENDER_call_portA_flush(ARINC_SENDER *);



    void arinc_sender_init(ARINC_SENDER *);

    void arinc_sender_activity(ARINC_SENDER *);


#endif
