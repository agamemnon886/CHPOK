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
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * libpok/arch/x86/syscall.c to x86_64 (64-bit).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#include <arch/syscall.h>
#include <types.h>

/*
 * Syscall convention (see kernel/arch/x86_64/syscalls.c):
 *
 *   int $0x80, %rax - syscall id, %rbx - address of the arguments.
 *   Result is returned in %rax, other registers are preserved.
 */
pok_ret_t lja_do_syscall (pok_syscall_id_t syscall_id, pok_syscall_args_t* args)
{
   uint64_t ret = (uint64_t)syscall_id;

   // The kernel reads arguments and may write to user memory.
   asm volatile ( "int $0x80"
                  : "+a" (ret)
                  : "b" (args)
                  : "memory");
   return (pok_ret_t)ret;
}
