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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: name
 * parameters and the message parameter of the send or write service are
 * const (they are only read), so callers can pass string literals and
 * const data.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_BUFFER

/*----------------------------------------------------------------*/ 
/*                                                                */ 
/* BUFFER constant and type definitions and management services   */ 
/*                                                                */ 
/*----------------------------------------------------------------*/ 
 
#ifndef APEX_BUFFER 
#define APEX_BUFFER 

#ifndef POK_NEEDS_ARINC653_PROCESS

/* Documented in libpok/include/config.h. */
#define POK_NEEDS_ARINC653_PROCESS
#endif

#include <arinc653/types.h>
#include <arinc653/process.h>
 
/**
 * @macroname `MAX_NUMBER_OF_BUFFERS`
 *
 * @description
 * The maximum number of buffers of a partition
 * (SYSTEM_LIMIT_NUMBER_OF_BUFFERS).
 */
#define  MAX_NUMBER_OF_BUFFERS    SYSTEM_LIMIT_NUMBER_OF_BUFFERS 
 
/**
 * @typename `BUFFER_NAME_TYPE`
 *
 * @description
 * The name of a buffer.
 */
typedef  NAME_TYPE       BUFFER_NAME_TYPE; 
 
/**
 * @typename `BUFFER_ID_TYPE`
 *
 * @description
 * The identifier of a buffer.
 */
typedef  APEX_INTEGER    BUFFER_ID_TYPE; 
 
/**
 * @typename `BUFFER_STATUS_TYPE`
 *
 * @description
 * The status of a buffer (GET_BUFFER_STATUS()).
 */
typedef struct { 
   /**
    * The number of messages in the buffer.
    */
   MESSAGE_RANGE_TYPE  NB_MESSAGE; 
   /**
    * The capacity of the buffer, in messages.
    */
   MESSAGE_RANGE_TYPE  MAX_NB_MESSAGE; 
   MESSAGE_SIZE_TYPE   MAX_MESSAGE_SIZE; /**< The maximum size of a message. */
   /**
    * The number of processes waiting.
    */
   WAITING_RANGE_TYPE  WAITING_PROCESSES;
} BUFFER_STATUS_TYPE; 
 
 
 
/** \cond */
extern void CREATE_BUFFER ( 
       /*in */ const BUFFER_NAME_TYPE         BUFFER_NAME, 
       /*in */ MESSAGE_SIZE_TYPE        MAX_MESSAGE_SIZE, 
       /*in */ MESSAGE_RANGE_TYPE       MAX_NB_MESSAGE, 
       /*in */ QUEUING_DISCIPLINE_TYPE  QUEUING_DISCIPLINE, 
       /*out*/ BUFFER_ID_TYPE           *BUFFER_ID, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
extern void SEND_BUFFER ( 
       /*in */ BUFFER_ID_TYPE           BUFFER_ID, 
       /*in */ const APEX_BYTE *       MESSAGE_ADDR,       /* by reference */ 
       /*in */ MESSAGE_SIZE_TYPE        LENGTH, 
       /*in */ SYSTEM_TIME_TYPE         TIME_OUT, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
extern void RECEIVE_BUFFER ( 
       /*in */ BUFFER_ID_TYPE           BUFFER_ID, 
       /*in */ SYSTEM_TIME_TYPE         TIME_OUT, 
       /*out*/ MESSAGE_ADDR_TYPE        MESSAGE_ADDR, 
       /*out*/ MESSAGE_SIZE_TYPE        *LENGTH, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
extern void GET_BUFFER_ID ( 
       /*in */ const BUFFER_NAME_TYPE         BUFFER_NAME, 
       /*out*/ BUFFER_ID_TYPE           *BUFFER_ID, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
extern void GET_BUFFER_STATUS ( 
       /*in */ BUFFER_ID_TYPE           BUFFER_ID, 
       /*out*/ BUFFER_STATUS_TYPE       *BUFFER_STATUS, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
/** \endcond */
 
#endif 
#endif
