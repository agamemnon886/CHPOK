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
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER.
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed.
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t), pok_syscall_id_e (was pok_syscall_id_t).
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/syscalls.c (the CPU support packages are in kernel/csps).
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs. A system call whose arguments the
 * partition may not access returns POK_ERRNO_EFAULT (was POK_ERRNO_EINVAL), as
 * for the other bad addresses.
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE). Long lines are wrapped at 80 columns.
 */

/**
 * @file
 * System calls on arm64: the handler of svc from user space.
 */

#include <core/debug.h>
#include <core/syscall.h>
#include <core/uaccess.h>

#include "interrupt.h"

/*
 * MISRA C:2025 Rule 11.5 deviation (this file): the jet_user_to_kernel*()
 * helpers return the kernel address of user memory as a pointer to void,
 * which is converted to the type of the object the user passed.
 */

/**
 * @funcname `process_syscall`
 *
 * @description
 * Performs a syscall (from ja_exception_sync_user()). The convention (see
 * libpok/arch/arm64/syscall.c) is svc \#0 with the syscall ID in x0 and the
 * address of its pok_syscall_args_t in x1; the result is returned in x0.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `frame` | `interrupt_frame *` | [in,out] The exception frame. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `syscall_args` | `pok_syscall_args_t *` | The arguments, at their kernel address |
 * | `syscall_args_user` | `pok_syscall_args_t *` | The arguments, at their user address |
 * | `syscall_id` | `pok_syscall_id_e` | The syscall ID |
 * | `syscall_info` | `pok_syscall_info_t` | Information about the caller (not used) |
 * | `syscall_ret` | `pok_ret_e` | The result |
 *
 * @hlr
 * - \hlrid{HLR_SYSCALLS_ARM64_PROCESS_SYSCALL_01}: `process_syscall()` shall
 *   take a system call from the supervisor call instruction: the identifier in
 *   x0 and the address of the arguments in x1, and give the result back in x0.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_002}
 *   - \srqref{SRQ_JET_SYS_009}
 *   - \srqref{SRQ_JET_SYS_003}
 * - \hlrid{HLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}: `process_syscall()` shall
 *   refuse arguments that the partition may not access with `POK_ERRNO_EFAULT`.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_005}
 *   - \srqref{SRQ_JET_MEM_006}
 *   - \srqref{SRQ_JET_MEM_016}
 *
 * @llr
 * - \llrid{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_01}: If
 *   `jet_user_to_kernel_typed()` gives NULL for the address in `x[1]` of
 *   `frame`, `process_syscall()` shall set `x[0]` of `frame` to
 *   `POK_ERRNO_EFAULT`, sign-extended to 64 bits, without calling
 *   `pok_core_syscall()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}
 * - \llrid{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}: If
 *   `jet_user_to_kernel_typed()` gives an address other than NULL for the
 *   address in `x[1]` of `frame`, `process_syscall()` shall call
 *   `pok_core_syscall()` once with the identifier in `x[0]` of `frame`, that
 *   address and the address of a `pok_syscall_info_t` whose `partition`,
 *   `base_addr` and `thread` are 0, and set `x[0]` of `frame` to its result,
 *   sign-extended to 64 bits.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALLS_ARM64_PROCESS_SYSCALL_01}
 *
 * @design
 * 1. Set `syscall_info.partition` to `0`.
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}
 * 2. Set `syscall_info.base_addr` to `0`.
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}
 * 3. Set `syscall_info.thread` to `0`.
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}
 * 4. Set `syscall_id` to `(pok_syscall_id_e) frame->x[0]`.
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}
 * 5. Set `syscall_args_user` to `(void*)frame->x[1]`.
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_01},
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}
 * 6. Set `syscall_args` to `jet_user_to_kernel_typed(syscall_args_user)`.
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_01},
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}
 * 7. If `syscall_args` is NULL: \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_01}
 *    1. Set `syscall_ret` to `POK_ERRNO_EFAULT`.
 * 8. Otherwise: \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}
 *    1. Call `pok_core_syscall()` with the arguments `syscall_id`,
 *       `syscall_args` and the address of `syscall_info`, and store the result
 *       in `syscall_ret`.
 * 9. Set `frame->x[0]` to `(uint64_t)(int64_t)syscall_ret`.
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_01},
 *    \llrref{LLR_SYSCALLS_ARM64_PROCESS_SYSCALL_02}
 *
 * @calls
 * - `jet_user_to_kernel()`
 * - `pok_core_syscall()`
 */
void process_syscall(interrupt_frame* frame)
{
   pok_syscall_info_t   syscall_info;
   pok_ret_e            syscall_ret;
   pok_syscall_args_t*  syscall_args;
   pok_syscall_id_e     syscall_id;

   syscall_info.partition = 0; // Has no sence with new scheduler
   syscall_info.base_addr = 0; // Has no sence with new scheduler
   syscall_info.thread    = 0; // Has no sence with new scheduler

   syscall_id = (pok_syscall_id_e) frame->x[0];

   /* MISRA C:2025 Rule 11.6 deviation: the user-space address of the arguments
    * is passed in a register. */
   pok_syscall_args_t* JET_USER syscall_args_user = (void*)frame->x[1];
   // Get kernel address of them.
   syscall_args = jet_user_to_kernel_typed(syscall_args_user);
   if(syscall_args == NULL)
   {
      syscall_ret = POK_ERRNO_EFAULT;
   }
   else
   {
      syscall_ret = pok_core_syscall (syscall_id, syscall_args, &syscall_info);
   }

   frame->x[0] = (uint64_t)(int64_t)syscall_ret;
}
