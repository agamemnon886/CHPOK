/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: REPORT_APPLICATION_MESSAGE
 * passes exactly LENGTH bytes of the message to the health monitor
 * (kernel) instead of printing it as a NUL-terminated string.
 * Modified by Kirill Brazhnikov, 2026-10-31: RAISE_APPLICATION_ERROR
 * returns NO_ERROR only if the kernel accepted the error.
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 * Modified by Kirill Brazhnikov, 2026-11-01: added CONFIGURE_ERROR_HANDLER
 * (ARINC 653P1-4).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: U suffixes for
 * unsigned constants.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: MAP() in
 * error_pok_to_arinc() no longer ends with a ";" of its own, so its
 * switch-clauses don't end with an empty statement; operands of mixed
 * precedence are parenthesized.
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: message
 * parameters that are only read are const (callers can pass const data).
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-13: the macros MAP_ERROR() and
 * MAP_ERROR_DEFAULT() are moved to libpok/include/arinc653/arincutils.h; MAP()
 * of error_pok_to_arinc() is MAP_ERROR_CODE(), in
 * libpok/include/arinc653/arincutils.h.
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
 */

/**
 * @file
 * ARINC 653 health monitoring services of the partition: messages and
 * application errors, the error handler and its error status.
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_ERROR
#include <types.h>
#include <errno.h>
#include <arinc653/types.h>
#include <arinc653/error.h>
#include <core/error.h>
#include <core/syscall.h>
#include <string.h>
#include <arinc653/arincutils.h>

/**
 * @funcname `error_pok_to_arinc`
 *
 * @description
 * Converts a kernel error kind to an ARINC 653 error code.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pok_error` | `int32_t` | [in] The kernel error kind. |
 *
 * @return The error code, or 0 for an unknown kind.
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
 * 1. Depending on `pok_error`:
 *    1. Case `MAP_ERROR_CODE(POK_ERROR_KIND_DEADLINE_MISSED, DEADLINE_MISSED)`:
 *       1. Use the macro `MAP_ERROR_CODE()` with the arguments
 *          `POK_ERROR_KIND_DEADLINE_MISSED` and `DEADLINE_MISSED`.
 *    2. Case
 *       `MAP_ERROR_CODE(POK_ERROR_KIND_APPLICATION_ERROR, APPLICATION_ERROR)`:
 *       1. Use the macro `MAP_ERROR_CODE()` with the arguments
 *          `POK_ERROR_KIND_APPLICATION_ERROR` and `APPLICATION_ERROR`.
 *    3. Case `MAP_ERROR_CODE(POK_ERROR_KIND_NUMERIC_ERROR, NUMERIC_ERROR)`:
 *       1. Use the macro `MAP_ERROR_CODE()` with the arguments
 *          `POK_ERROR_KIND_NUMERIC_ERROR` and `NUMERIC_ERROR`.
 *    4. Case `MAP_ERROR_CODE(POK_ERROR_KIND_ILLEGAL_REQUEST, ILLEGAL_REQUEST)`:
 *       1. Use the macro `MAP_ERROR_CODE()` with the arguments
 *          `POK_ERROR_KIND_ILLEGAL_REQUEST` and `ILLEGAL_REQUEST`.
 *    5. Case `MAP_ERROR_CODE(POK_ERROR_KIND_STACK_OVERFLOW, STACK_OVERFLOW)`:
 *       1. Use the macro `MAP_ERROR_CODE()` with the arguments
 *          `POK_ERROR_KIND_STACK_OVERFLOW` and `STACK_OVERFLOW`.
 *    6. Case
 *       `MAP_ERROR_CODE(POK_ERROR_KIND_MEMORY_VIOLATION, MEMORY_VIOLATION)`:
 *       1. Use the macro `MAP_ERROR_CODE()` with the arguments
 *          `POK_ERROR_KIND_MEMORY_VIOLATION` and `MEMORY_VIOLATION`.
 *    7. Case `MAP_ERROR_CODE(POK_ERROR_KIND_HARDWARE_FAULT, HARDWARE_FAULT)`:
 *       1. Use the macro `MAP_ERROR_CODE()` with the arguments
 *          `POK_ERROR_KIND_HARDWARE_FAULT` and `HARDWARE_FAULT`.
 *    8. Case `MAP_ERROR_CODE(POK_ERROR_KIND_POWER_FAIL, POWER_FAIL)`:
 *       1. Use the macro `MAP_ERROR_CODE()` with the arguments
 *          `POK_ERROR_KIND_POWER_FAIL` and `POWER_FAIL`.
 *    9. Otherwise (default):
 *       1. Return `0`.
 *
 * @calls
 * None.
 */
static ERROR_CODE_TYPE error_pok_to_arinc(int32_t pok_error)
{
    switch (pok_error) {
        MAP_ERROR_CODE(POK_ERROR_KIND_DEADLINE_MISSED, DEADLINE_MISSED);
        MAP_ERROR_CODE(POK_ERROR_KIND_APPLICATION_ERROR, APPLICATION_ERROR);
        MAP_ERROR_CODE(POK_ERROR_KIND_NUMERIC_ERROR, NUMERIC_ERROR);
        MAP_ERROR_CODE(POK_ERROR_KIND_ILLEGAL_REQUEST, ILLEGAL_REQUEST);
        MAP_ERROR_CODE(POK_ERROR_KIND_STACK_OVERFLOW, STACK_OVERFLOW);
        MAP_ERROR_CODE(POK_ERROR_KIND_MEMORY_VIOLATION, MEMORY_VIOLATION);
        MAP_ERROR_CODE(POK_ERROR_KIND_HARDWARE_FAULT, HARDWARE_FAULT);
        MAP_ERROR_CODE(POK_ERROR_KIND_POWER_FAIL, POWER_FAIL);
        default: return 0;
    }
}

/**
 * @funcname `REPORT_APPLICATION_MESSAGE`
 *
 * @description
 * Transmits a message to the health monitor (HM) function (ARINC 653 service).
 * The message is LENGTH bytes long; it need not be NUL-terminated and may be
 * empty (LENGTH = 0). The kernel writes it to the console with the name of the
 * reporting partition. No error is raised: the calling process continues.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `MESSAGE` | `const APEX_BYTE *` | [in] The message. |
 * | `LENGTH` | `MESSAGE_SIZE_TYPE` | [in] Its length. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
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
 * 1. If `LENGTH` is less than `0` or `LENGTH` is greater than
 *    `MAX_ERROR_MESSAGE_SIZE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. Call `pok_error_report_application_message()` with the arguments
 *    `(const char*) MESSAGE` and `(size_t) LENGTH`, and store the result in
 *    `core_ret`.
 * 3. Depending on `core_ret`:
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
 * - `pok_error_report_application_message()`
 */
void REPORT_APPLICATION_MESSAGE (const APEX_BYTE *   MESSAGE,
                                 MESSAGE_SIZE_TYPE    LENGTH,
                                 RETURN_CODE_TYPE     *RETURN_CODE )
{
   pok_ret_e core_ret;

   if ((LENGTH < 0) || (LENGTH > MAX_ERROR_MESSAGE_SIZE)) {
       *RETURN_CODE = INVALID_PARAM;
       return;
   }

   core_ret = pok_error_report_application_message ((const char*) MESSAGE,
                                                    (size_t) LENGTH);

   switch (core_ret) {
       MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
       // Bad message address or length.
       MAP_ERROR_DEFAULT(INVALID_PARAM);
   }
}

/**
 * @funcname `CREATE_ERROR_HANDLER`
 *
 * @description
 * Creates the error handler process of the partition (ARINC 653 service).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ENTRY_POINT` | `SYSTEM_ADDRESS_TYPE` | [in] Its entry point. |
 * | `STACK_SIZE` | `STACK_SIZE_TYPE` | [in] Its stack size. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, NO_ACTION (it exists), INVALID_MODE or INVALID_CONFIG. |
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
 * 1. Call `pok_error_thread_create()` with the arguments `STACK_SIZE` and
 *    `(void*)ENTRY_POINT`, and store the result in `core_ret`.
 * 2. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_EXISTS, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EXISTS`
 *          and `NO_ACTION`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_PARTITION_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_PARTITION_MODE` and `INVALID_MODE`.
 *       2. Leave the switch.
 *    4. Case `MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EINVAL`
 *          and `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    5. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_error_thread_create()`
 */
void CREATE_ERROR_HANDLER (SYSTEM_ADDRESS_TYPE  ENTRY_POINT,
                           STACK_SIZE_TYPE      STACK_SIZE,
                           RETURN_CODE_TYPE     *RETURN_CODE)
{
    pok_ret_e core_ret = pok_error_thread_create(STACK_SIZE,
        (void*)ENTRY_POINT);
    //core_ret = pok_syscall2 (POK_SYSCALL_ERROR_HANDLER_CREATE, (uint32_t)STACK_SIZE, (uint32_t)ENTRY_POINT);

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_EXISTS, NO_ACTION);
        MAP_ERROR(POK_ERRNO_PARTITION_MODE, INVALID_MODE);
        MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG);
        MAP_ERROR_DEFAULT(INVALID_CONFIG);
    }
}

/**
 * @funcname `GET_ERROR_STATUS`
 *
 * @description
 * Returns the status of the error that the error handler handles (ARINC 653
 * service): its code, message, failed process and address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ERROR_STATUS` | `ERROR_STATUS_TYPE *` | [out] Where the status is stored. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_CONFIG (not the error handler), NO_ACTION (no error) or NOT_AVAILABLE. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `core_ret` | `pok_ret_e` | The kernel's result |
 * | `core_status` | `pok_error_status_t` | The kernel's error status |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `pok_error_get()` with the arguments the address of `core_status` and
 *    `ERROR_STATUS->MESSAGE`, and store the result in `core_ret`.
 * 2. If `core_ret` is equal to `POK_ERRNO_OK`:
 *    1. Call `error_pok_to_arinc()` with the argument `core_status.error_kind`,
 *       and store the result in `ERROR_STATUS->ERROR_CODE`.
 *    2. Set `ERROR_STATUS->LENGTH` to `core_status.msg_size`.
 *    3. Set `ERROR_STATUS->FAILED_PROCESS_ID` to
 *       `core_status.failed_thread + 1U`.
 *    4. Set `ERROR_STATUS->FAILED_ADDRESS` to
 *       `(SYSTEM_ADDRESS_TYPE)core_status.failed_addr`.
 * 3. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_THREAD, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_THREAD`
 *          and `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `NO_ACTION`.
 *       2. Leave the switch.
 *    4. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `NOT_AVAILABLE`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_error_get()`
 * - `error_pok_to_arinc()`
 */
void GET_ERROR_STATUS (ERROR_STATUS_TYPE  *ERROR_STATUS,
                       RETURN_CODE_TYPE   *RETURN_CODE )
{
    pok_error_status_t   core_status;
    pok_ret_e            core_ret;

    core_ret = pok_error_get (&core_status, ERROR_STATUS->MESSAGE);

    if (core_ret == POK_ERRNO_OK) {
        ERROR_STATUS->ERROR_CODE = error_pok_to_arinc(core_status.error_kind);
        ERROR_STATUS->LENGTH = core_status.msg_size;
        // ARINC process IDs are one higher
        ERROR_STATUS->FAILED_PROCESS_ID = core_status.failed_thread + 1U;
        ERROR_STATUS->FAILED_ADDRESS = (SYSTEM_ADDRESS_TYPE)core_status.failed_addr;
    }

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_THREAD, INVALID_CONFIG);
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, NO_ACTION);
        MAP_ERROR_DEFAULT(NOT_AVAILABLE);
    }

}

/**
 * @funcname `RAISE_APPLICATION_ERROR`
 *
 * @description
 * Raises an application error with a message (ARINC 653 service); only
 * APPLICATION_ERROR may be raised.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ERROR_CODE` | `ERROR_CODE_TYPE` | [in] The error code. |
 * | `MESSAGE` | `const APEX_BYTE *` | [in] The message. |
 * | `LENGTH` | `ERROR_MESSAGE_SIZE_TYPE` | [in] Its length. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR or INVALID_PARAM. |
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
 * 1. If `LENGTH` is less than `0` or `LENGTH` is greater than
 *    `MAX_ERROR_MESSAGE_SIZE`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 2. If `ERROR_CODE` is not equal to `APPLICATION_ERROR`:
 *    1. Set `*RETURN_CODE` to `INVALID_PARAM`.
 *    2. Return.
 * 3. Call `pok_error_raise_application_error()` with the arguments
 *    `(const char*) MESSAGE` and `(size_t) LENGTH`, and store the result in
 *    `core_ret`.
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
 * - `pok_error_raise_application_error()`
 */
void RAISE_APPLICATION_ERROR (ERROR_CODE_TYPE            ERROR_CODE,
                              const APEX_BYTE *         MESSAGE,
                              ERROR_MESSAGE_SIZE_TYPE    LENGTH,
                              RETURN_CODE_TYPE           *RETURN_CODE)
{
    if ((LENGTH < 0) || (LENGTH > MAX_ERROR_MESSAGE_SIZE)) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }
    
    if ((ERROR_CODE != APPLICATION_ERROR)) {
        *RETURN_CODE = INVALID_PARAM;
        return;
    }

    pok_ret_e core_ret =
        pok_error_raise_application_error ((const char*) MESSAGE,
                                           (size_t) LENGTH);

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        // Bad message address or length: the error was not raised.
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

/**
 * @funcname `CONFIGURE_ERROR_HANDLER`
 *
 * @description
 * Sets the concurrency control and the processor core of the error handler
 * (ARINC 653P1-4). Allowed only during partition initialization, after
 * CREATE_ERROR_HANDLER. JetOS runs each partition on a single processor core
 * (ID 0), so PROCESSOR_CORE_ID must be 0 or CORE_AFFINITY_NO_PREFERENCE. On one
 * core, PROCESSES_PAUSE and PROCESSES_SCHEDULED behave the same: no other
 * process of the partition runs while the error handler runs.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `CONCURRENCY_CONTROL` | `ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE` | [in] The concurrency control. |
 * | `PROCESSOR_CORE_ID` | `PROCESSOR_CORE_ID_TYPE` | [in] The processor core. |
 * | `RETURN_CODE` | `RETURN_CODE_TYPE *` | [out] Where the result is stored: NO_ERROR, INVALID_CONFIG, INVALID_PARAM or INVALID_MODE. |
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
 * 1. Call `pok_error_thread_configure()` with the arguments
 *    `(uint32_t) CONCURRENCY_CONTROL` and `(int32_t) PROCESSOR_CORE_ID`, and
 *    store the result in `core_ret`.
 * 2. Depending on `core_ret`:
 *    1. Case `MAP_ERROR(POK_ERRNO_OK, NO_ERROR)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_OK` and
 *          `NO_ERROR`.
 *       2. Leave the switch.
 *    2. Case `MAP_ERROR(POK_ERRNO_UNAVAILABLE, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_UNAVAILABLE` and `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    3. Case `MAP_ERROR(POK_ERRNO_EINVAL, INVALID_PARAM)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_EINVAL`
 *          and `INVALID_PARAM`.
 *       2. Leave the switch.
 *    4. Case `MAP_ERROR(POK_ERRNO_PARAM, INVALID_CONFIG)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments `POK_ERRNO_PARAM` and
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *    5. Case `MAP_ERROR(POK_ERRNO_PARTITION_MODE, INVALID_MODE)`:
 *       1. Use the macro `MAP_ERROR()` with the arguments
 *          `POK_ERRNO_PARTITION_MODE` and `INVALID_MODE`.
 *       2. Leave the switch.
 *    6. Otherwise (default):
 *       1. Use the macro `MAP_ERROR_DEFAULT()` with the argument
 *          `INVALID_CONFIG`.
 *       2. Leave the switch.
 *
 * @calls
 * - `pok_error_thread_configure()`
 */
void CONFIGURE_ERROR_HANDLER (ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE CONCURRENCY_CONTROL,
                              PROCESSOR_CORE_ID_TYPE                 PROCESSOR_CORE_ID,
                              RETURN_CODE_TYPE                       *RETURN_CODE)
{
    // The kernel checks CONCURRENCY_CONTROL; any out-of-range value,
    // negative ones too, stays out of range as uint32_t.
    pok_ret_e core_ret =
        pok_error_thread_configure ((uint32_t) CONCURRENCY_CONTROL,
                                    (int32_t) PROCESSOR_CORE_ID);

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        // The error handler is not created.
        MAP_ERROR(POK_ERRNO_UNAVAILABLE, INVALID_CONFIG);
        // Unknown CONCURRENCY_CONTROL.
        MAP_ERROR(POK_ERRNO_EINVAL, INVALID_PARAM);
        // PROCESSOR_CORE_ID is not a core of the partition.
        MAP_ERROR(POK_ERRNO_PARAM, INVALID_CONFIG);
        MAP_ERROR(POK_ERRNO_PARTITION_MODE, INVALID_MODE);
        MAP_ERROR_DEFAULT(INVALID_CONFIG);
    }
}

#endif
