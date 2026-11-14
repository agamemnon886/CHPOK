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
 * Created by julien on Wed Jan 21 23:07:15 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: includes
 * <core/kernel.h>, which declares the functions defined here.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * Restart and stop of the whole kernel (error handling); not implemented.
 */

#include <config.h>
#include <core/kernel.h>

#ifdef POK_NEEDS_ERROR_HANDLING

/**
 * @funcname `pok_kernel_restart`
 *
 * @description
 * Restarts the kernel, as an error handling action of the module. Not
 * implemented: it does nothing.
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
 * 1. Do nothing.
 *
 * @calls
 * None.
 */
void pok_kernel_restart (void)
{
   /*
    * Needs to be implemented
    */
}

/**
 * @funcname `pok_kernel_stop`
 *
 * @description
 * Stops the kernel, as an error handling action of the module. Not implemented:
 * it does nothing.
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
 * 1. Do nothing.
 *
 * @calls
 * None.
 */
void pok_kernel_stop (void)
{
   /*
    * Needs to be implemented
    */
}
#endif
