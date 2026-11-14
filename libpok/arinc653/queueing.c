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
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: message length is passed
 * with the type the kernel expects (64-bit support).
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the kernel
 * port ID is converted to the APEX type before the addition.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: name
 * parameters and the message parameter of the send or write service are
 * const (they are only read), so callers can pass string literals and
 * const data.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros MAP_ERROR() and
 * MAP_ERROR_DEFAULT() are moved to libpok/include/arinc653/arincutils.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_queuing_discipline_e (was pok_queuing_discipline_t), pok_ret_e (was
 * pok_ret_t).
 */

/**
 * @file
 * ARINC 653 queuing ports (inter-partition communication):
 * CREATE_QUEUING_PORT, SEND_QUEUING_MESSAGE, RECEIVE_QUEUING_MESSAGE,
 * GET_QUEUING_PORT_ID, GET_QUEUING_PORT_STATUS and CLEAR_QUEUING_PORT.
 */

#include <config.h>

#include <core/dependencies.h>

#ifdef POK_NEEDS_ARINC653_QUEUEING

#include <types.h>
#include <middleware/port.h>
#include <arinc653/types.h>
#include <arinc653/queueing.h>
#include <utils.h>
#include <arinc653/arincutils.h>

/**
 * @funcname `CREATE_QUEUING_PORT`
 *
 * @description
 * Creates a queuing port of the configuration (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `QUEUING_PORT_NAME` | `const QUEUING_PORT_NAME_TYPE` | [in] The name. |
 * | `MAX_MESSAGE_SIZE` | `MESSAGE_SIZE_TYPE` | [in] The maximum size of a message. |
 * | `MAX_NB_MESSAGE` | `MESSAGE_RANGE_TYPE` | [in] The maximum number of messages. |
 * | `PORT_DIRECTION` | `PORT_DIRECTION_TYPE` | [in] SOURCE or DESTINATION. |
 * | `QUEUING_DISCIPLINE` | `QUEUING_DISCIPLINE_TYPE` | [in] The discipline of the waiting processes (FIFO or PRIORITY). |
 * | `QUEUING_PORT_ID` | `QUEUING_PORT_ID_TYPE *` | [out] Where its ID is stored (the kernel's ID plus 1). |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION (it exists), INVALID_MODE or INVALID_CONFIG. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_direction` | `pok_port_direction_t` | The kernel's direction |
 * | `core_discipline` | `pok_queuing_discipline_e` | The kernel's discipline |
 * | `core_id` | `pok_port_id_t` | The kernel's port ID |
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Depending on `QUEUING_DISCIPLINE`:
 *    1. Case `PRIORITY`:
 *       1. Set `core_discipline` to `POK_QUEUING_DISCIPLINE_PRIORITY`.
 *       2. Leave the switch.
 *    2. Case `FIFO`:
 *       1. Set `core_discipline` to `POK_QUEUING_DISCIPLINE_FIFO`.
 *       2. Leave the switch.
 *    3. Otherwise (default):
 *       1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *       2. Return.
 * 2. Depending on `PORT_DIRECTION`:
 *    1. Case `SOURCE`:
 *       1. Set `core_direction` to `POK_PORT_DIRECTION_OUT`.
 *       2. Leave the switch.
 *    2. Case `DESTINATION`:
 *       1. Set `core_direction` to `POK_PORT_DIRECTION_IN`.
 *       2. Leave the switch.
 *    3. Otherwise (default):
 *       1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *       2. Return.
 * 3. Call `pok_port_queuing_create()` with the arguments `QUEUING_PORT_NAME`,
 *    `MAX_MESSAGE_SIZE`, `MAX_NB_MESSAGE`, `core_direction`, `core_discipline`
 *    and the address of `core_id`, and store the result in `core_ret`.
 * 4. Set `*QUEUING_PORT_ID` to `(QUEUING_PORT_ID_TYPE) core_id + 1`.
 * 5. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_EXISTS, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EXISTS`
 *          and `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_MODE` and
 *          `INVALID_MODE`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_port_queuing_create()`
 */
void CREATE_QUEUING_PORT (
      /*in */ const QUEUING_PORT_NAME_TYPE    QUEUING_PORT_NAME,
      /*in */ MESSAGE_SIZE_TYPE         MAX_MESSAGE_SIZE,
      /*in */ MESSAGE_RANGE_TYPE        MAX_NB_MESSAGE,
      /*in */ PORT_DIRECTION_TYPE       PORT_DIRECTION,
      /*in */ QUEUING_DISCIPLINE_TYPE   QUEUING_DISCIPLINE,
      /*out*/ QUEUING_PORT_ID_TYPE      *QUEUING_PORT_ID,
      /*out*/ RETURN_CODE_TYPE          *RETURN_CODE)
{
   pok_ret_e                        core_ret;
   pok_port_id_t                    core_id;
   pok_queuing_discipline_e         core_discipline;
   pok_port_direction_t             core_direction;

   switch (QUEUING_DISCIPLINE)
   {
     case PRIORITY:
         core_discipline = POK_QUEUING_DISCIPLINE_PRIORITY;
         break;

     case FIFO:
         core_discipline = POK_QUEUING_DISCIPLINE_FIFO;
         break;

      default:
         *RETURN_CODE = INVALID_CONFIG;
         return;
   }

   switch (PORT_DIRECTION)
   {
      case SOURCE:
         core_direction = POK_PORT_DIRECTION_OUT;
         break;
      
      case DESTINATION:
         core_direction = POK_PORT_DIRECTION_IN;
         break;

      default:
         *RETURN_CODE = INVALID_CONFIG;
         return;
   }

   core_ret = pok_port_queuing_create(QUEUING_PORT_NAME,
      MAX_MESSAGE_SIZE, MAX_NB_MESSAGE, core_direction, core_discipline,
      &core_id);

   *QUEUING_PORT_ID = (QUEUING_PORT_ID_TYPE) core_id + 1;

   switch (core_ret) {
      MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
      MAP_ERROR(POK_ERRNO_EXISTS, NO_ACTION);
      MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
      MAP_ERROR_DEFAULT(INVALID_CONFIG);
   }
}

/**
 * @funcname `SEND_QUEUING_MESSAGE`
 *
 * @description
 * Sends a message to a queuing port (ARINC 653 service), waiting up to a
 * time-out if it is full.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `QUEUING_PORT_ID` | `QUEUING_PORT_ID_TYPE` | [in] The port. |
 * | `MESSAGE_ADDR` | `const APEX_BYTE *` | [in] The message. |
 * | `LENGTH` | `MESSAGE_SIZE_TYPE` | [in] Its length. |
 * | `TIME_OUT` | `SYSTEM_TIME_TYPE` | [in] The time-out (0: no wait). |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, INVALID_MODE, NOT_AVAILABLE, TIMED_OUT or INVALID_CONFIG. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `QUEUING_PORT_ID` is less than or equal to `0`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Call `pok_port_queuing_send()` with the arguments `QUEUING_PORT_ID - 1`,
 *    `MESSAGE_ADDR`, `LENGTH` and the address of `TIME_OUT`, and store the
 *    result in `core_ret`.
 * 3. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_DIRECTION, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_DIRECTION`
 *          and `INVALID_MODE`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_MODE` and
 *          `INVALID_MODE`.
 *       2. Leave the switch.
 *    4. Case `MAP_ERROR(POK_ERRNO_FULL, NOT_AVAILABLE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_FULL` and
 *          `NOT_AVAILABLE`.
 *       2. Leave the switch.
 *    5. Case `MAP_ERROR(POK_ERRNO_TIMEOUT, TIMED_OUT)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_TIMEOUT`
 *          and `TIMED_OUT`.
 *       2. Leave the switch.
 *    6. Case `MAP_ERROR(POK_ERRNO_PARAM, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_PARAM` and
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    7. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_port_queuing_send()`
 */
void SEND_QUEUING_MESSAGE (
      /*in */ QUEUING_PORT_ID_TYPE      QUEUING_PORT_ID,
      /*in */ const APEX_BYTE *        MESSAGE_ADDR,       /* by reference */
      /*in */ MESSAGE_SIZE_TYPE         LENGTH,
      /*in */ SYSTEM_TIME_TYPE          TIME_OUT,
      /*out*/ RETURN_CODE_TYPE          *RETURN_CODE)
{
    pok_ret_e core_ret;

    if (QUEUING_PORT_ID <= 0) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }

    core_ret = pok_port_queuing_send (QUEUING_PORT_ID - 1, MESSAGE_ADDR, LENGTH, &TIME_OUT);

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_DIRECTION, INVALID_MODE);
        MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_ERROR(POK_ERRNO_FULL, NOT_AVAILABLE);
        MAP_ERROR(POK_ERRNO_TIMEOUT, TIMED_OUT); 
        MAP_ERROR(POK_ERRNO_PARAM, INVALID_CONFIG); 
        MAP_ERROR_DEFAULT(INVALID_CONFIG);
    }
}

/**
 * @funcname `RECEIVE_QUEUING_MESSAGE`
 *
 * @description
 * Receives a message from a queuing port (ARINC 653 service), waiting up to a
 * time-out if it is empty.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `QUEUING_PORT_ID` | `QUEUING_PORT_ID_TYPE` | [in] The port. |
 * | `TIME_OUT` | `SYSTEM_TIME_TYPE` | [in] The time-out (0: no wait). |
 * | `MESSAGE_ADDR` | `MESSAGE_ADDR_TYPE` | [out] Where the message is stored. |
 * | `LENGTH` | `MESSAGE_SIZE_TYPE *` | [in,out] Where its length is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, INVALID_MODE, NOT_AVAILABLE, TIMED_OUT or INVALID_CONFIG (overflow). |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `length` | `pok_port_size_t` | The length, as the kernel's type |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `QUEUING_PORT_ID` is less than or equal to `0`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `length` to `(pok_port_size_t)*LENGTH`.
 * 3. Call `pok_port_queuing_receive()` with the arguments
 *    `QUEUING_PORT_ID - 1`, the address of `TIME_OUT`, `MESSAGE_ADDR` and the
 *    address of `length`, and store the result in `core_ret`.
 * 4. Set `*LENGTH` to `length`.
 * 5. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_DIRECTION, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_DIRECTION`
 *          and `INVALID_MODE`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_MODE` and
 *          `INVALID_MODE`.
 *       2. Leave the switch.
 *    4. Case `MAP_ERROR(POK_ERRNO_EMPTY, NOT_AVAILABLE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EMPTY` and
 *          `NOT_AVAILABLE`.
 *       2. Leave the switch.
 *    5. Case `MAP_ERROR(POK_ERRNO_TIMEOUT, TIMED_OUT)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_TIMEOUT`
 *          and `TIMED_OUT`.
 *       2. Leave the switch.
 *    6. Case `MAP_ERROR(POK_ERRNO_TOOMANY, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_TOOMANY`
 *          and `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    7. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_port_queuing_receive()`
 */
void RECEIVE_QUEUING_MESSAGE (
      /*in */ QUEUING_PORT_ID_TYPE      QUEUING_PORT_ID,
      /*in */ SYSTEM_TIME_TYPE          TIME_OUT,
      /*out*/ MESSAGE_ADDR_TYPE         MESSAGE_ADDR,
      /*out*/ MESSAGE_SIZE_TYPE         *LENGTH,
      /*out*/ RETURN_CODE_TYPE          *RETURN_CODE )
{
   pok_ret_e core_ret;

   if (QUEUING_PORT_ID <= 0) {
       *RETURN_CODE = INVALID_PARAM;
       return;
   }
   
   // MESSAGE_SIZE_TYPE may be wider than pok_port_size_t (e.g. on arm64).
   pok_port_size_t length = (pok_port_size_t)*LENGTH;
   core_ret = pok_port_queuing_receive (QUEUING_PORT_ID - 1, &TIME_OUT, MESSAGE_ADDR, &length);
   *LENGTH = length;

   switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_DIRECTION, INVALID_MODE);
        MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_ERROR(POK_ERRNO_EMPTY, NOT_AVAILABLE);
        MAP_ERROR(POK_ERRNO_TIMEOUT, TIMED_OUT);
        MAP_ERROR(POK_ERRNO_TOOMANY, INVALID_CONFIG);
        MAP_ERROR_DEFAULT(INVALID_CONFIG);
    }
}

/**
 * @funcname `GET_QUEUING_PORT_ID`
 *
 * @description
 * Returns the ID of a queuing port by name (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `QUEUING_PORT_NAME` | `const QUEUING_PORT_NAME_TYPE` | [in] The name. |
 * | `QUEUING_PORT_ID` | `QUEUING_PORT_ID_TYPE *` | [out] Where the ID is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_CONFIG. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `id` | `pok_port_id_t` | The kernel's port ID |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_port_queuing_id()` with the arguments `QUEUING_PORT_NAME` and
 *    the address of `id`, and store the result in `core_ret`.
 * 2. If `core_ret` is equal to `POK_ERRNO_OK`:
 *    1. Set `*QUEUING_PORT_ID` to `(QUEUING_PORT_ID_TYPE) id + 1`.
 * 3. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_port_queuing_id()`
 */
void GET_QUEUING_PORT_ID (
      /*in */ const QUEUING_PORT_NAME_TYPE    QUEUING_PORT_NAME,
      /*out*/ QUEUING_PORT_ID_TYPE      *QUEUING_PORT_ID,
      /*out*/ RETURN_CODE_TYPE          *RETURN_CODE)
{
    pok_ret_e core_ret;
    pok_port_id_t id;

    core_ret = pok_port_queuing_id(QUEUING_PORT_NAME, &id);

    if (core_ret == POK_ERRNO_OK) {
        *QUEUING_PORT_ID = (QUEUING_PORT_ID_TYPE) id + 1;
    }
   
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR_DEFAULT(INVALID_CONFIG);
    }
}

/**
 * @funcname `GET_QUEUING_PORT_STATUS`
 *
 * @description
 * Returns the status of a queuing port (ARINC 653 service): the number of
 * messages, the maximum number and size, the direction and the number of
 * waiting processes.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `QUEUING_PORT_ID` | `QUEUING_PORT_ID_TYPE` | [in] The port. |
 * | `QUEUING_PORT_STATUS` | `QUEUING_PORT_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `status` | `pok_port_queuing_status_t` | The kernel's status |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `QUEUING_PORT_ID` is less than or equal to `0`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Call `pok_port_queuing_status()` with the arguments `QUEUING_PORT_ID - 1`
 *    and the address of `status`, and store the result in `core_ret`.
 * 3. If `core_ret` is equal to `POK_ERRNO_OK`:
 *    1. Set `QUEUING_PORT_STATUS->NB_MESSAGE` to `status.nb_message`.
 *    2. Set `QUEUING_PORT_STATUS->MAX_NB_MESSAGE` to `status.max_nb_message`.
 *    3. Set `QUEUING_PORT_STATUS->MAX_MESSAGE_SIZE` to
 *       `status.max_message_size`.
 *    4. Depending on `status.direction`:
 *       1. Case `POK_PORT_DIRECTION_OUT`:
 *          1. Set `QUEUING_PORT_STATUS->PORT_DIRECTION` to `SOURCE`.
 *          2. Leave the switch.
 *       2. Case `POK_PORT_DIRECTION_IN`:
 *          1. Set `QUEUING_PORT_STATUS->PORT_DIRECTION` to `DESTINATION`.
 *          2. Leave the switch.
 *       3. Otherwise (default):
 *          1. Leave the switch.
 *    5. Set `QUEUING_PORT_STATUS->WAITING_PROCESSES` to
 *       `status.waiting_processes`.
 *    6. Set `*RETURN_CODE` to `NO_ERROR`.
 * 4. Otherwise:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *
 * @calls
 * - `pok_port_queuing_status()`
 */
void GET_QUEUING_PORT_STATUS (
      /*in */ QUEUING_PORT_ID_TYPE      QUEUING_PORT_ID,
      /*out*/ QUEUING_PORT_STATUS_TYPE *QUEUING_PORT_STATUS,
      /*out*/ RETURN_CODE_TYPE          *RETURN_CODE)
{
  pok_ret_e core_ret;
  pok_port_queuing_status_t status;
  
    if (QUEUING_PORT_ID <= 0) {
       *RETURN_CODE = INVALID_PARAM;
       return;
    }

   core_ret = pok_port_queuing_status(QUEUING_PORT_ID - 1, &status);
   if (core_ret == POK_ERRNO_OK) {
       QUEUING_PORT_STATUS->NB_MESSAGE = status.nb_message;
       QUEUING_PORT_STATUS->MAX_NB_MESSAGE = status.max_nb_message;
       QUEUING_PORT_STATUS->MAX_MESSAGE_SIZE = status.max_message_size;
       switch (status.direction) {
         case POK_PORT_DIRECTION_OUT:
          QUEUING_PORT_STATUS->PORT_DIRECTION = SOURCE;
          break;
         case POK_PORT_DIRECTION_IN:
          QUEUING_PORT_STATUS->PORT_DIRECTION = DESTINATION;
          break;
         default:
          break; // XXX assert(0)
       }
       QUEUING_PORT_STATUS->WAITING_PROCESSES = status.waiting_processes;
       *RETURN_CODE = NO_ERROR;
   } else {
       *RETURN_CODE = INVALID_PARAM;
   }
}

/**
 * @funcname `CLEAR_QUEUING_PORT`
 *
 * @description
 * Removes the messages of a destination queuing port (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `QUEUING_PORT_ID` | `QUEUING_PORT_ID_TYPE` | [in] The port. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, INVALID_MODE or INVALID_CONFIG. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_port_queuing_clear()` with the argument `QUEUING_PORT_ID - 1`,
 *    and store the result in `core_ret`.
 * 2. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_PORT, INVALID_PARAM)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_PORT` and
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_MODE` and
 *          `INVALID_MODE`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_port_queuing_clear()`
 */
void CLEAR_QUEUING_PORT (
      /*in */ QUEUING_PORT_ID_TYPE      QUEUING_PORT_ID,
      /*out*/ RETURN_CODE_TYPE          *RETURN_CODE)
{
   pok_ret_e core_ret = pok_port_queuing_clear(QUEUING_PORT_ID - 1);

   switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_PORT, INVALID_PARAM);
        MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_CONFIG);
    }
}

#endif
