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
 * Modified by Kirill Brazhnikov, 2026-11-01: added
 * ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE and CONFIGURE_ERROR_HANDLER
 * (ARINC 653P1-4).
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: message
 * parameters that are only read are const (callers can pass const data).
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_ERROR
#ifndef APEX_ERROR
#define APEX_ERROR

#ifndef POK_NEEDS_ARINC653_PROCESS

/* Documented in libpok/include/config.h. */
#define POK_NEEDS_ARINC653_PROCESS 1
#endif

#include <arinc653/process.h>

#include <arinc653/types.h>

/**
 * @macroname `MAX_ERROR_MESSAGE_SIZE`
 *
 * @description
 * The maximum size of an error message.
 */
#define  MAX_ERROR_MESSAGE_SIZE         128
 
/**
 * @typename `ERROR_MESSAGE_SIZE_TYPE`
 *
 * @description
 * The size of an error message.
 */
typedef  APEX_INTEGER   ERROR_MESSAGE_SIZE_TYPE; 
 
/**
 * @typename `ERROR_MESSAGE_TYPE`
 *
 * @description
 * An error message.
 */
typedef  APEX_BYTE      ERROR_MESSAGE_TYPE[MAX_ERROR_MESSAGE_SIZE]; 
 
/**
 * @typename `ERROR_CODE_TYPE`
 *
 * @description
 * The kind of a process level error.
 */
typedef enum {
   DEADLINE_MISSED      = 0, /**< A process missed its deadline. */
   /**
    * The application reported an error (RAISE_APPLICATION_ERROR()).
    */
   APPLICATION_ERROR    = 1,
   NUMERIC_ERROR        = 2, /**< A numeric error (e.g. a division by zero). */
   ILLEGAL_REQUEST      = 3, /**< An illegal request to the operating system. */
   STACK_OVERFLOW       = 4, /**< A stack overflow. */
   MEMORY_VIOLATION     = 5, /**< A memory access violation. */
   HARDWARE_FAULT       = 6, /**< A hardware fault. */
   POWER_FAIL           = 7 /**< A power failure. */
} ERROR_CODE_TYPE;

/**
 * @typename `ERROR_STATUS_TYPE`
 *
 * @description
 * The status of the error being handled (GET_ERROR_STATUS()).
 */
typedef struct{
   ERROR_CODE_TYPE         ERROR_CODE; /**< The kind of the error. */
   MESSAGE_SIZE_TYPE       LENGTH; /**< The length of the message. */
   /**
    * The process that caused the error.
    */
   PROCESS_ID_TYPE         FAILED_PROCESS_ID;
   /**
    * The address where the error occurred.
    */
   SYSTEM_ADDRESS_TYPE     FAILED_ADDRESS;
   ERROR_MESSAGE_TYPE      MESSAGE; /**< The error message. */
} ERROR_STATUS_TYPE;

/**
 * @typename `ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE`
 *
 * @description
 * Whether the processes of the partition run while the error handler runs
 * (CONFIGURE_ERROR_HANDLER()).
 */
typedef enum {
   /**
    * The other processes are paused while the handler runs.
    */
   PROCESSES_PAUSE      = 0,
   /**
    * The other processes are scheduled while the handler runs.
    */
   PROCESSES_SCHEDULED  = 1
} ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE;

/** \cond */
extern void REPORT_APPLICATION_MESSAGE (const APEX_BYTE *   MESSAGE,
                                        MESSAGE_SIZE_TYPE    LENGTH,
                                        RETURN_CODE_TYPE     *RETURN_CODE);

extern void CREATE_ERROR_HANDLER (SYSTEM_ADDRESS_TYPE  ENTRY_POINT,
                                  STACK_SIZE_TYPE      STACK_SIZE,
                                  RETURN_CODE_TYPE     *RETURN_CODE);

extern void GET_ERROR_STATUS (ERROR_STATUS_TYPE  *ERROR_STATUS,
                              RETURN_CODE_TYPE   *RETURN_CODE );

extern void RAISE_APPLICATION_ERROR (ERROR_CODE_TYPE            ERROR_CODE,
                                     const APEX_BYTE *         MESSAGE,
                                     ERROR_MESSAGE_SIZE_TYPE    LENGTH,
                                     RETURN_CODE_TYPE           *RETURN_CODE);

extern void CONFIGURE_ERROR_HANDLER (ERROR_HANDLER_CONCURRENCY_CONTROL_TYPE CONCURRENCY_CONTROL,
                                     PROCESSOR_CORE_ID_TYPE                 PROCESSOR_CORE_ID,
                                     RETURN_CODE_TYPE                       *RETURN_CODE);
/** \endcond */
#endif
#endif 
