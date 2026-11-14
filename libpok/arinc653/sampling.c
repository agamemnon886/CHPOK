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
 * Modified by Kirill Brazhnikov, 2026-10-31: CREATE_SAMPLING_PORT accepts
 * any positive refresh period, rejects MAX_MESSAGE_SIZE values above
 * SYSTEM_LIMIT_MESSAGE_SIZE (they were truncated before the kernel
 * compared them) and sets SAMPLING_PORT_ID only on success.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: braces around
 * all if/else/loop bodies.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: operands of
 * mixed precedence are parenthesized. Long lines are wrapped at 80 columns.
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
 * pok_ret_e (was pok_ret_t).
 */

/**
 * @file
 * ARINC 653 sampling ports (inter-partition communication):
 * CREATE_SAMPLING_PORT, WRITE_SAMPLING_MESSAGE, READ_SAMPLING_MESSAGE,
 * GET_SAMPLING_PORT_ID, GET_SAMPLING_PORT_STATUS, and a check for new
 * data.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_SAMPLING

#include <types.h>
#include <middleware/port.h>
#include <arinc653/types.h>
#include <arinc653/sampling.h>
#include <arinc653/partition.h>
#include <core/thread.h>
#include <utils.h>
#include <arinc653/arincutils.h>

/**
 * @funcname `CREATE_SAMPLING_PORT`
 *
 * @description
 * Creates a sampling port of the configuration (ARINC 653 service).
 * MAX_MESSAGE_SIZE is wider than the kernel's pok_port_size_t on 64-bit
 * targets, so its range is checked here: a truncated value could match the
 * configured size. Negative refresh periods (INFINITE_TIME_VALUE too) are out
 * of range; the kernel also rejects 0.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SAMPLING_PORT_NAME` | `const SAMPLING_PORT_NAME_TYPE` | [in] The name. |
 * | `MAX_MESSAGE_SIZE` | `MESSAGE_SIZE_TYPE` | [in] The maximum size of a message. |
 * | `PORT_DIRECTION` | `PORT_DIRECTION_TYPE` | [in] SOURCE or DESTINATION. |
 * | `REFRESH_PERIOD` | `SYSTEM_TIME_TYPE` | [in] The refresh period. |
 * | `SAMPLING_PORT_ID` | `SAMPLING_PORT_ID_TYPE *` | [out] Where its ID is stored (the kernel's ID plus 1). |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_CONFIG, NO_ACTION (it exists) or INVALID_MODE. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_direction` | `pok_port_direction_t` | The kernel's direction |
 * | `core_id` | `pok_port_id_t` | The kernel's port ID |
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `MAX_MESSAGE_SIZE` is less than or equal to `0` or `MAX_MESSAGE_SIZE`
 *    is greater than `SYSTEM_LIMIT_MESSAGE_SIZE` or `REFRESH_PERIOD` is less
 *    than `0`:
 *    1. Set `*RETURN_CODE` to `INVALID_CONFIG`.
 *    2. Return.
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
 * 3. Call `pok_port_sampling_create()` with the arguments `SAMPLING_PORT_NAME`,
 *    `MAX_MESSAGE_SIZE`, `core_direction`, the address of `REFRESH_PERIOD` and
 *    the address of `core_id`, and store the result in `core_ret`.
 * 4. If `core_ret` is equal to `POK_ERRNO_OK`:
 *    1. Set `*SAMPLING_PORT_ID` to `(SAMPLING_PORT_ID_TYPE) core_id + 1`.
 * 5. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EINVAL`
 *          and `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    4. Case `MAP_ERROR(POK_ERRNO_EXISTS, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EXISTS`
 *          and `NO_ACTION`.
 *       2. Leave the switch.
 *    5. Case `MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_MODE` and
 *          `INVALID_MODE`.
 *       2. Leave the switch.
 *    6. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_port_sampling_create()`
 */
void CREATE_SAMPLING_PORT (
	 /*in */ const SAMPLING_PORT_NAME_TYPE    SAMPLING_PORT_NAME,
			 /*in */ MESSAGE_SIZE_TYPE          MAX_MESSAGE_SIZE,
			 /*in */ PORT_DIRECTION_TYPE        PORT_DIRECTION,
			 /*in */ SYSTEM_TIME_TYPE           REFRESH_PERIOD,
			 /*out*/ SAMPLING_PORT_ID_TYPE      *SAMPLING_PORT_ID,
			 /*out*/ RETURN_CODE_TYPE           *RETURN_CODE )
{
	 pok_port_direction_t core_direction;
	 pok_port_id_t        core_id;
	 pok_ret_e            core_ret;

	 /*
	  * MAX_MESSAGE_SIZE is wider than the kernel's pok_port_size_t on 64-bit
	  * targets, so check its range here: a truncated value could match the
	  * configured size. INFINITE_TIME_VALUE (-1) and other negative
	  * refresh periods are out of range; the kernel also rejects 0.
	  */
	 if ((MAX_MESSAGE_SIZE <= 0)
             || (MAX_MESSAGE_SIZE > SYSTEM_LIMIT_MESSAGE_SIZE)
		 || (REFRESH_PERIOD < 0))
		 {
			 *RETURN_CODE = INVALID_CONFIG;
			 return ;
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

     // SYSTEM_TIME_TYPE and pok_time_t are both int64_t.
     core_ret = pok_port_sampling_create (SAMPLING_PORT_NAME, MAX_MESSAGE_SIZE, core_direction, &REFRESH_PERIOD, &core_id);

	 if (core_ret == POK_ERRNO_OK) {
		 *SAMPLING_PORT_ID = (SAMPLING_PORT_ID_TYPE) core_id + 1;
	 }

   switch (core_ret) {
      MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
      // For this function any error in parameter is treated as INVALID_CONFIG
      MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG);
      // No sampling port with this name is configured for the partition.
      MAP_ERROR(POK_ERRNO_UNAVAILABLE, INVALID_CONFIG);
      MAP_ERROR(POK_ERRNO_EXISTS, NO_ACTION);
      MAP_ERROR(POK_ERRNO_MODE, INVALID_MODE);
      MAP_ERROR_DEFAULT(INVALID_CONFIG); // random error status, should never happen
   }
}

/**
 * @funcname `WRITE_SAMPLING_MESSAGE`
 *
 * @description
 * Writes a message to a source sampling port (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SAMPLING_PORT_ID` | `SAMPLING_PORT_ID_TYPE` | [in] The port. |
 * | `MESSAGE_ADDR` | `const APEX_BYTE *` | [in] The message. |
 * | `LENGTH` | `MESSAGE_SIZE_TYPE` | [in] Its length. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_PARAM, INVALID_CONFIG (bad length) or INVALID_MODE. |
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
 * 1. If `LENGTH` is less than or equal to `0`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. If `SAMPLING_PORT_ID` is less than or equal to `0`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 3. Call `pok_port_sampling_write()` with the arguments
 *    `SAMPLING_PORT_ID - 1`, `MESSAGE_ADDR` and `LENGTH`, and store the result
 *    in `core_ret`.
 * 4. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_PORT, INVALID_PARAM)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_PORT` and
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EINVAL`
 *          and `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    4. Case `MAP_ERROR(POK_ERRNO_DIRECTION, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_DIRECTION`
 *          and `INVALID_MODE`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_port_sampling_write()`
 */
void WRITE_SAMPLING_MESSAGE (
			 /*in */ SAMPLING_PORT_ID_TYPE      SAMPLING_PORT_ID,
	 /*in */ const APEX_BYTE *         MESSAGE_ADDR,     /* by reference */
			 /*in */ MESSAGE_SIZE_TYPE          LENGTH,
			 /*out*/ RETURN_CODE_TYPE           *RETURN_CODE )
{
    pok_ret_e core_ret;

    if (LENGTH <= 0) {
        *RETURN_CODE = INVALID_PARAM;
	return;
    }
    if (SAMPLING_PORT_ID <= 0) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }
    core_ret = pok_port_sampling_write (SAMPLING_PORT_ID - 1, MESSAGE_ADDR, LENGTH);
    switch (core_ret) {
      MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
      MAP_ERROR(POK_ERRNO_PORT, INVALID_PARAM); // port doesn't exists
      MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG); // incorrect length
      MAP_ERROR(POK_ERRNO_DIRECTION, INVALID_MODE);
      MAP_ERROR_DEFAULT(INVALID_CONFIG); // random error status, should never happen
    }
}

/**
 * @funcname `READ_SAMPLING_MESSAGE`
 *
 * @description
 * Reads the message of a destination sampling port (ARINC 653 service), with
 * its validity (whether it is within the refresh period).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SAMPLING_PORT_ID` | `SAMPLING_PORT_ID_TYPE` | [in] The port. |
 * | `MESSAGE_ADDR` | `MESSAGE_ADDR_TYPE` | [out] Where the message is stored. |
 * | `LENGTH` | `MESSAGE_SIZE_TYPE *` | [in,out] Where its length is stored. |
 * | `VALIDITY` | `VALIDITY_TYPE *` | [out] Where its validity is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION (no message), INVALID_PARAM, INVALID_MODE or INVALID_CONFIG. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `core_validity` | `pok_bool_t` | The kernel's validity |
 * | `length` | `pok_port_size_t` | The length, as the kernel's type |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `SAMPLING_PORT_ID` is less than or equal to `0`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Set `length` to `(pok_port_size_t)*LENGTH`.
 * 3. Call `pok_port_sampling_read()` with the arguments `SAMPLING_PORT_ID - 1`,
 *    `MESSAGE_ADDR`, the address of `length` and the address of
 *    `core_validity`, and store the result in `core_ret`.
 * 4. Set `*LENGTH` to `length`.
 * 5. If `core_ret` is equal to `POK_ERRNO_OK`:
 *    1. Set `*VALIDITY` to `VALID` if `core_validity` is true, otherwise
 *       `INVALID`.
 * 6. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_EMPTY, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EMPTY` and
 *          `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_EINVAL, INVALID_PARAM)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EINVAL`
 *          and `INVALID_PARAM`.
 *       2. Leave the switch.
 *    4. Case `MAP_ERROR(POK_ERRNO_DIRECTION, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_DIRECTION`
 *          and `INVALID_MODE`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_port_sampling_read()`
 */
void READ_SAMPLING_MESSAGE (
			 /*in */ SAMPLING_PORT_ID_TYPE      SAMPLING_PORT_ID,
			 /*out*/ MESSAGE_ADDR_TYPE          MESSAGE_ADDR,
			 /*out*/ MESSAGE_SIZE_TYPE          *LENGTH,
			 /*out*/ VALIDITY_TYPE              *VALIDITY,
			 /*out*/ RETURN_CODE_TYPE           *RETURN_CODE )
{
    pok_ret_e core_ret;
    pok_bool_t core_validity;

    if (SAMPLING_PORT_ID <= 0) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }

    // MESSAGE_SIZE_TYPE may be wider than pok_port_size_t (e.g. on arm64).
    pok_port_size_t length = (pok_port_size_t)*LENGTH;
    core_ret = pok_port_sampling_read (SAMPLING_PORT_ID - 1, MESSAGE_ADDR, &length, &core_validity);
    *LENGTH = length;

    if(core_ret == POK_ERRNO_OK){
        *VALIDITY = core_validity? VALID : INVALID;
    }

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_EMPTY, NO_ACTION);
        MAP_ERROR(POK_ERRNO_EINVAL, INVALID_PARAM);
        MAP_ERROR(POK_ERRNO_DIRECTION, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_CONFIG); // random error status, should never happen
    }
}

/**
 * @funcname `GET_SAMPLING_PORT_ID`
 *
 * @description
 * Returns the ID of a sampling port by name (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SAMPLING_PORT_NAME` | `const SAMPLING_PORT_NAME_TYPE` | [in] The name. |
 * | `SAMPLING_PORT_ID` | `SAMPLING_PORT_ID_TYPE *` | [out] Where the ID is stored. |
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
 * 1. Call `pok_port_sampling_id()` with the arguments `SAMPLING_PORT_NAME` and
 *    the address of `id`, and store the result in `core_ret`.
 * 2. Set `*SAMPLING_PORT_ID` to `(SAMPLING_PORT_ID_TYPE) id + 1`.
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
 * - `pok_port_sampling_id()`
 */
void GET_SAMPLING_PORT_ID (
	 /*in */ const SAMPLING_PORT_NAME_TYPE    SAMPLING_PORT_NAME,
			 /*out*/ SAMPLING_PORT_ID_TYPE      *SAMPLING_PORT_ID,
			 /*out*/ RETURN_CODE_TYPE           *RETURN_CODE )
{
    pok_ret_e core_ret;
    pok_port_id_t id;

    core_ret = pok_port_sampling_id(SAMPLING_PORT_NAME, &id);
    *SAMPLING_PORT_ID = (SAMPLING_PORT_ID_TYPE) id + 1;
    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR_DEFAULT(INVALID_CONFIG);
    }
}

/**
 * @funcname `GET_SAMPLING_PORT_STATUS`
 *
 * @description
 * Returns the status of a sampling port (ARINC 653 service): the refresh
 * period, maximum message size, direction and validity of the last message.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SAMPLING_PORT_ID` | `SAMPLING_PORT_ID_TYPE` | [in] The port. |
 * | `SAMPLING_PORT_STATUS` | `SAMPLING_PORT_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `status` | `pok_port_sampling_status_t` | The kernel's status |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `SAMPLING_PORT_ID` is less than or equal to `0`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Call `pok_port_sampling_status()` with the arguments
 *    `SAMPLING_PORT_ID - 1` and the address of `status`, and store the result
 *    in `core_ret`.
 * 3. If `core_ret` is equal to `POK_ERRNO_OK`:
 *    1. Set `SAMPLING_PORT_STATUS->REFRESH_PERIOD` to `status.refresh`.
 *    2. Set `SAMPLING_PORT_STATUS->MAX_MESSAGE_SIZE` to `status.size`.
 *    3. Set `SAMPLING_PORT_STATUS->PORT_DIRECTION` to `SOURCE` if
 *       `status.direction` is equal to `POK_PORT_DIRECTION_OUT`, otherwise
 *       `DESTINATION`.
 *    4. Set `SAMPLING_PORT_STATUS->LAST_MSG_VALIDITY` to `VALID` if
 *       `status.validity` is true, otherwise `INVALID`.
 * 4. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_PARAM`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_port_sampling_status()`
 */
void GET_SAMPLING_PORT_STATUS (
			 /*in */ SAMPLING_PORT_ID_TYPE      SAMPLING_PORT_ID,
			 /*out*/ SAMPLING_PORT_STATUS_TYPE  *SAMPLING_PORT_STATUS,
			 /*out*/ RETURN_CODE_TYPE           *RETURN_CODE )
{
    pok_ret_e core_ret;
    pok_port_sampling_status_t status;

    if (SAMPLING_PORT_ID <= 0) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }

    core_ret = pok_port_sampling_status(SAMPLING_PORT_ID - 1, &status);

    if (core_ret == POK_ERRNO_OK) {
        SAMPLING_PORT_STATUS->REFRESH_PERIOD = status.refresh;
        SAMPLING_PORT_STATUS->MAX_MESSAGE_SIZE = status.size;
        SAMPLING_PORT_STATUS->PORT_DIRECTION = (status.direction == POK_PORT_DIRECTION_OUT) ? SOURCE : DESTINATION;
        SAMPLING_PORT_STATUS->LAST_MSG_VALIDITY = status.validity? VALID : INVALID;
    }

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

/**
 * @funcname `SYS_SAMPLING_PORT_CHECK_IS_NEW_DATA`
 *
 * @description
 * Tells whether a destination sampling port has a message that hasn't been read
 * yet (JetOS extension).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `SAMPLING_PORT_ID` | `SAMPLING_PORT_ID_TYPE` | [in] The port. |
 *
 * @return The kernel's result converted to pok_bool_t: FALSE (POK_ERRNO_OK) if
 *         there is new data, TRUE if there is none or on an error.
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
 * 1. Call `pok_port_sampling_check()` with the argument `SAMPLING_PORT_ID - 1`,
 *    and return its result.
 *
 * @calls
 * - `pok_port_sampling_check()`
 */
pok_bool_t SYS_SAMPLING_PORT_CHECK_IS_NEW_DATA(
        /*in */ SAMPLING_PORT_ID_TYPE      SAMPLING_PORT_ID)
{
    return pok_port_sampling_check(SAMPLING_PORT_ID - 1);
}

#endif
