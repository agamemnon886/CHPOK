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
 * Modified by Kirill Brazhnikov, 2026-10-31: adapted from
 * kernel/arch/x86/syscalls.c for arm64.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <core/debug.h>
#include <core/syscall.h>
#include <core/uaccess.h>

#include "interrupt.h"

/*
 * Syscall convention (see libpok/arch/arm64/syscall.c):
 *
 *   svc #0, x0 - syscall id, x1 - address of pok_syscall_args_t.
 *   Result is returned in x0.
 */
void process_syscall(interrupt_frame* frame)
{
   pok_syscall_info_t   syscall_info;
   pok_ret_t            syscall_ret;
   pok_syscall_args_t*  syscall_args;
   pok_syscall_id_t     syscall_id;

   syscall_info.partition = 0; // Has no sence with new scheduler
   syscall_info.base_addr = 0; // Has no sence with new scheduler
   syscall_info.thread    = 0; // Has no sence with new scheduler

   syscall_id = (pok_syscall_id_t) frame->x[0];

   pok_syscall_args_t* __user syscall_args_user = (void*)frame->x[1];
   // Get kernel address of them.
   syscall_args = jet_user_to_kernel_typed(syscall_args_user);
   if(syscall_args == NULL)
   {
      syscall_ret = POK_ERRNO_EINVAL;
   }
   else
   {
      syscall_ret = pok_core_syscall (syscall_id, syscall_args, &syscall_info);
   }

   frame->x[0] = (uint64_t)(int64_t)syscall_ret;
}
