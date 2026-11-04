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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
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

/*
 * Each MAP_ERROR()/MAP_ERROR_DEFAULT() expands to a whole switch-clause ending
 * with break (MISRA C:2025 Rule 16.3).
 */
#define MAP_ERROR(from, to) case (from): *RETURN_CODE = (to); break
#define MAP_ERROR_DEFAULT(to) default: *RETURN_CODE = (to); break

static ERROR_CODE_TYPE error_pok_to_arinc(int32_t pok_error)
{
    #define MAP(from, to) case (from): return (to)
    switch (pok_error) {
        MAP(POK_ERROR_KIND_DEADLINE_MISSED, DEADLINE_MISSED);
        MAP(POK_ERROR_KIND_APPLICATION_ERROR, APPLICATION_ERROR);
        MAP(POK_ERROR_KIND_NUMERIC_ERROR, NUMERIC_ERROR);
        MAP(POK_ERROR_KIND_ILLEGAL_REQUEST, ILLEGAL_REQUEST);
        MAP(POK_ERROR_KIND_STACK_OVERFLOW, STACK_OVERFLOW);
        MAP(POK_ERROR_KIND_MEMORY_VIOLATION, MEMORY_VIOLATION);
        MAP(POK_ERROR_KIND_HARDWARE_FAULT, HARDWARE_FAULT);
        MAP(POK_ERROR_KIND_POWER_FAIL, POWER_FAIL);
        default: return 0;
    }
    #undef MAP
}

/**
 * Transmits a message to the health monitor (HM) function.
 *
 * The message is LENGTH bytes long; it need not be NUL-terminated and
 * may be empty (LENGTH = 0). The kernel writes it to the console with the
 * name of the reporting partition. No error is raised: the calling
 * process continues.
 */
void REPORT_APPLICATION_MESSAGE (const APEX_BYTE *   MESSAGE,
                                 MESSAGE_SIZE_TYPE    LENGTH,
                                 RETURN_CODE_TYPE     *RETURN_CODE )
{
   pok_ret_t core_ret;

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

void CREATE_ERROR_HANDLER (SYSTEM_ADDRESS_TYPE  ENTRY_POINT,
                           STACK_SIZE_TYPE      STACK_SIZE,
                           RETURN_CODE_TYPE     *RETURN_CODE)
{
    pok_ret_t core_ret = pok_error_thread_create(STACK_SIZE, (void*)ENTRY_POINT);
    //core_ret = pok_syscall2 (POK_SYSCALL_ERROR_HANDLER_CREATE, (uint32_t)STACK_SIZE, (uint32_t)ENTRY_POINT);

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        MAP_ERROR(POK_ERRNO_EXISTS, NO_ACTION);
        MAP_ERROR(POK_ERRNO_PARTITION_MODE, INVALID_MODE);
        MAP_ERROR(POK_ERRNO_EINVAL, INVALID_CONFIG);
        MAP_ERROR_DEFAULT(INVALID_CONFIG);
    }
}

void GET_ERROR_STATUS (ERROR_STATUS_TYPE  *ERROR_STATUS,
                       RETURN_CODE_TYPE   *RETURN_CODE )
{
    pok_error_status_t   core_status;
    pok_ret_t            core_ret;

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

    pok_ret_t core_ret = pok_error_raise_application_error ((const char*) MESSAGE,
                                                            (size_t) LENGTH);

    switch (core_ret) {
        MAP_ERROR(POK_ERRNO_OK, NO_ERROR);
        // Bad message address or length: the error was not raised.
        MAP_ERROR_DEFAULT(INVALID_PARAM);
    }
}

/**
 * Sets the concurrency control and the processor core of the error
 * handler (ARINC 653P1-4).
 *
 * Allowed only during partition initialization, after
 * CREATE_ERROR_HANDLER. JetOS runs each partition on a single processor
 * core (ID 0), so PROCESSOR_CORE_ID must be 0 or
 * CORE_AFFINITY_NO_PREFERENCE. On one core, PROCESSES_PAUSE and
 * PROCESSES_SCHEDULED behave the same: no other process of the partition
 * runs while the error handler runs.
 */
void CONFIGURE_ERROR_HANDLER (ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE CONCURRENCY_CONTROL,
                              PROCESSOR_CORE_ID_TYPE                 PROCESSOR_CORE_ID,
                              RETURN_CODE_TYPE                       *RETURN_CODE)
{
    // The kernel checks CONCURRENCY_CONTROL; any out-of-range value,
    // negative ones too, stays out of range as uint32_t.
    pok_ret_t core_ret = pok_error_thread_configure ((uint32_t) CONCURRENCY_CONTROL,
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
