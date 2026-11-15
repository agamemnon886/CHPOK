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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t), pok_syscall_id_e (was pok_syscall_id_t).
 * Modified by Kirill Brazhnikov, 2026-11-15: the comments name kernel/csps (it
 * was kernel/arch).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/**
 * @file
 * Syscalls of libpok on x86_64.
 */

#include <arch/syscall.h>
#include <types.h>

/**
 * @funcname `lja_do_syscall`
 *
 * @description
 * Performs a syscall. The convention (see kernel/csps/x86_64/syscalls.c) is int
 * $0x80 with the syscall ID in %%rax and the address of the arguments in %%rbx;
 * the result is returned in %%rax, other registers are preserved.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `syscall_id` | `pok_syscall_id_e` | [in] The syscall ID. |
 * | `args` | `pok_syscall_args_t *` | [in] The arguments. |
 *
 * @return The result.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `uint64_t` | The ID, then the result |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `ret` to `(uint64_t)syscall_id`.
 * 2. Execute the assembly code `int $0x80`.
 * 3. Return `(pok_ret_e)ret`.
 *
 * @calls
 * None.
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation: the system call instruction
 * is GNU inline assembly, isolated in this function.
 */
pok_ret_e lja_do_syscall (pok_syscall_id_e syscall_id, pok_syscall_args_t* args)
{
   uint64_t ret = (uint64_t)syscall_id;

   // The kernel reads arguments and may write to user memory.
   asm volatile ( "int $0x80"
                  : "+a" (ret)
                  : "b" (args)
                  : "memory");
   return (pok_ret_e)ret;
}
