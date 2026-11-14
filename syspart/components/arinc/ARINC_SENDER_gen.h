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
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; the parameters of the prototypes are
 * named; __<component>_init__() and __<component>_activity__() are
 * declared.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen comments for the file,
 * its types and their members (the descriptions are in the component's
 * configuration: description and state_doc); the function declarations are
 * hidden from Doxygen (\cond).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef ARINC_SENDER_GEN_H
#define ARINC_SENDER_GEN_H

    #include <arinc653/queueing.h>
    #include <arinc653/sampling.h>
    #include <port_info.h>


    #include <interfaces/preallocated_sender_gen.h>

/**
 * @typename `ARINC_SENDER_state`
 *
 * @description
 * The state of an instance of the ARINC_SENDER component
 * (state_struct in its configuration).
 */
typedef struct {
    NAME_TYPE port_name;
    /**< The name of the ARINC 653 port. */
    PORT_DIRECTION_TYPE port_direction;
    /**< The direction of the port (DESTINATION: the partition receives on
     * it). */
    uint32_t overhead;
    /**< The room before a message for the headers (max_backstep). */
    MESSAGE_SIZE_TYPE port_max_message_size;
    /**< The maximum size of a message of the port. */
    int32_t is_queuing_port;
    /**< Non-zero for a queuing port, zero for a sampling port. */
    MESSAGE_RANGE_TYPE q_port_max_nb_messages;
    /**< The capacity of a queuing port, in messages. */
    APEX_INTEGER port_id;
    /**< The identifier of the port (set by arinc_sender_init()). */
    sys_port_data_t * port_buffer;
    /**< The buffer of a message, with its room for the headers (set by
     * arinc_sender_init()). */
}ARINC_SENDER_state;

/**
 * @typename `ARINC_SENDER`
 *
 * @description
 * An instance of the ARINC_SENDER component: its state and its
 * ports.
 * It reads the messages of an ARINC 653 port of the partition (queuing or
 * sampling) in its activity and sends them on (portA), with room before
 * each for the headers of the components below.
 */
typedef struct {
    ARINC_SENDER_state state; /**< The state of the instance. */
    struct {
    } in;
    /**< The in ports, each with the operations of its interface that the
     * component implements: none. */
    struct {
            struct {
                preallocated_sender *ops;
                self_t *owner;
            } portA;
    } out;
    /**< The out ports, each with the operations of the component it is
     * connected to and that component (owner):
     * portA (preallocated_sender). */
} ARINC_SENDER;


/** \cond */


      ret_e
      ARINC_SENDER_call_portA_send(
          ARINC_SENDER *self, char * arg1, size_t arg2, size_t arg3);
      ret_e
      ARINC_SENDER_call_portA_flush(
          ARINC_SENDER *self);



    void arinc_sender_init(ARINC_SENDER *self);

    void arinc_sender_activity(ARINC_SENDER *self);

/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __ARINC_SENDER_init__(ARINC_SENDER *self);
void __ARINC_SENDER_activity__(ARINC_SENDER *self);
/** \endcond */


#endif
