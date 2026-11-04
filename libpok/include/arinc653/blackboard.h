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
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_BLACKBOARD

/*-------------------------------------------------------------------*/ 
/*                                                                   */ 
/* BLACKBOARD constant and type definitions and management services  */ 
/*                                                                   */ 
/*-------------------------------------------------------------------*/ 
 
#ifndef APEX_BLACKBOARD 
#define APEX_BLACKBOARD 

#ifndef POK_NEEDS_ARINC653_PROCESS
#define POK_NEEDS_ARINC653_PROCESS
#endif

#include <arinc653/types.h>
#include <arinc653/process.h>
 
#define  MAX_NUMBER_OF_BLACKBOARDS      SYSTEM_LIMIT_NUMBER_OF_BLACKBOARDS 
 
typedef  NAME_TYPE       BLACKBOARD_NAME_TYPE; 
 
typedef  APEX_INTEGER    BLACKBOARD_ID_TYPE; 
 
typedef  enum { EMPTY = 0, OCCUPIED = 1 } EMPTY_INDICATOR_TYPE; 
 
typedef struct { 
   EMPTY_INDICATOR_TYPE  EMPTY_INDICATOR; 
   MESSAGE_SIZE_TYPE     MAX_MESSAGE_SIZE; 
   WAITING_RANGE_TYPE    WAITING_PROCESSES; 
} BLACKBOARD_STATUS_TYPE; 
 
extern void CREATE_BLACKBOARD ( 
       /*in */ const BLACKBOARD_NAME_TYPE     BLACKBOARD_NAME, 
       /*in */ MESSAGE_SIZE_TYPE        MAX_MESSAGE_SIZE, 
       /*out*/ BLACKBOARD_ID_TYPE       *BLACKBOARD_ID, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
extern void DISPLAY_BLACKBOARD ( 
       /*in */ BLACKBOARD_ID_TYPE       BLACKBOARD_ID, 
       /*in */ const APEX_BYTE *       MESSAGE_ADDR,       /* by reference */ 
       /*in */ MESSAGE_SIZE_TYPE        LENGTH, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
extern void READ_BLACKBOARD ( 
       /*in */ BLACKBOARD_ID_TYPE       BLACKBOARD_ID, 
       /*in */ SYSTEM_TIME_TYPE         TIME_OUT, 
       /*out*/ MESSAGE_ADDR_TYPE        MESSAGE_ADDR, 
       /*out*/ MESSAGE_SIZE_TYPE        *LENGTH, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
extern void CLEAR_BLACKBOARD ( 
       /*in */ BLACKBOARD_ID_TYPE       BLACKBOARD_ID, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
extern void GET_BLACKBOARD_ID ( 
       /*in */ const BLACKBOARD_NAME_TYPE     BLACKBOARD_NAME, 
       /*out*/ BLACKBOARD_ID_TYPE       *BLACKBOARD_ID, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
extern void GET_BLACKBOARD_STATUS ( 
       /*in */ BLACKBOARD_ID_TYPE       BLACKBOARD_ID, 
       /*out*/ BLACKBOARD_STATUS_TYPE   *BLACKBOARD_STATUS, 
       /*out*/ RETURN_CODE_TYPE         *RETURN_CODE ); 
 
#endif 

#endif
