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
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <config.h>

#ifdef POK_NEEDS_ARINC653_TIME
#ifndef APEX_TIME
#define APEX_TIME

#include <arinc653/types.h>

/*----------------------------------------------------------------------*/
/* */
/* time constant definitions */
/* */
/*----------------------------------------------------------------------*/
/* implementation dependent */
/* these values are given as example */
/*----------------------------------------------------------------------*/
/* */
/* time type definitions */
/* */
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/* */
/* time management services */
/* */
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/** \cond */
extern void TIMED_WAIT (
      /*in */ SYSTEM_TIME_TYPE delay_time,
      /*out*/ RETURN_CODE_TYPE *return_code );
/*----------------------------------------------------------------------*/
extern void PERIODIC_WAIT (
      /*out*/ RETURN_CODE_TYPE *return_code );
/*----------------------------------------------------------------------*/
extern void GET_TIME (
      /*out*/ SYSTEM_TIME_TYPE *system_time,
      /*out*/ RETURN_CODE_TYPE *return_code );
/*----------------------------------------------------------------------*/
void REPLENISH (SYSTEM_TIME_TYPE budget_time, RETURN_CODE_TYPE *return_code);
/** \endcond */
/*----------------------------------------------------------------------*/
#endif
#endif
