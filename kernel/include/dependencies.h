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
 * Created by julien on Sat May 16 14:40:46 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_KERNEL_DEPENDENCIES_H
#define POK_KERNEL_DEPENDENCIES_H

#include <config.h>

#ifdef POK_NEEDS_ERROR_HANDLING
   #ifndef POK_NEEDS_SCHED_STOP_SELF
      #define POK_NEEDS_SCHED_STOP_SELF 1
   #endif
#endif

#ifdef POK_NEEDS_THREAD_SUSPEND
   #ifndef POK_NEEDS_SCHED_STOP_SELF
      #define POK_NEEDS_SCHED_STOP_SELF 1
   #endif
#endif

#endif
