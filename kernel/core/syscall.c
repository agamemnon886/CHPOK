/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2013-2014, 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * This file also incorporates work covered by POK License.
 * Copyright (c) 2007-2009 POK team
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: removed the
 * POK_SYSCALL_GET_BSP_INFO syscall (PowerPC only).
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: dispatches the
 * POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE syscall.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-01: dispatches the
 * POK_SYSCALL_ERROR_HANDLER_CONFIGURE syscall.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: __user/__kuser
 * are renamed JET_USER/JET_KUSER; discarded results are cast to void;
 * deviations are documented where a guideline can't be followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the use of the
 * SYSCALL_ENTRY() parameter with ## is explained; the unused infos
 * parameter is cast to void; deviations are documented where a guideline
 * can't be followed. Long lines are wrapped at 80 columns.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the macro of the dispatcher is
 * moved to kernel/include/core/syscall.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_ret_e (was pok_ret_t), pok_syscall_id_e (was pok_syscall_id_t).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: the function headers follow
 * outb() and inb(), which call x86_outb() and x86_inb().
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: low level requirements of one
 * behaviour each; code that compiles to nothing (assertions, (void) x;,
 * empty branches) is left out of the design; the high level requirements
 * trace to the system requirements split into requirements of one "shall"
 * each.
 */

#include <config.h>

#include <asp/arch.h>
#include <types.h>
#include <libc.h>
#include <ioports.h>

#include <errno.h>
#include <core/debug.h>
#include <core/syscall.h>
#include <core/partition_arinc.h>
#include <core/thread.h>
#include <core/time.h>
#include <core/error.h>
#include <asp/memory.h>

#include <cons.h>
#include <core/port.h>

/**
 * @funcname `unprotected_syscall`
 *
 * @description
 * Runs a syscall implementation with preemption enabled, then returns to user
 * mode as the implementation leaves it.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `f` | `pok_ret_e (*)(const pok_syscall_args_t* args)` | [in] The syscall's wrapper. |
 * | `args` | `const pok_syscall_args_t *` | [in] The syscall's arguments. |
 *
 * @return The syscall's result.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `pok_ret_e` | The result |
 *
 * @hlr
 * - \hlrid{HLR_SYSCALL_UNPROTECTED_SYSCALL_01}: `unprotected_syscall()` shall
 *   run a system call with preemption enabled.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_006}
 * - \hlrid{HLR_SYSCALL_UNPROTECTED_SYSCALL_02}: `unprotected_syscall()` shall
 *   prepare the return to the partition when the call is done, so that its
 *   events are handled first.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_007}
 *
 * @llr
 * - \llrid{LLR_SYSCALL_UNPROTECTED_SYSCALL_01}: `unprotected_syscall()` shall
 *   call `ja_preempt_enable()` once and then the function `f` once with `args`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_UNPROTECTED_SYSCALL_01}
 * - \llrid{LLR_SYSCALL_UNPROTECTED_SYSCALL_02}: `unprotected_syscall()` shall
 *   then call `pok_partition_return_user()` once and return the result of `f`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_UNPROTECTED_SYSCALL_02}
 *
 * @design
 * 1. Call `ja_preempt_enable()`. \llrref{LLR_SYSCALL_UNPROTECTED_SYSCALL_01}
 * 2. Call the function `f` with the argument `args`, and store the result in
 *    `ret`. \llrref{LLR_SYSCALL_UNPROTECTED_SYSCALL_02}
 * 3. Call `pok_partition_return_user()`.
 *    \llrref{LLR_SYSCALL_UNPROTECTED_SYSCALL_02}
 * 4. Return `ret`. \llrref{LLR_SYSCALL_UNPROTECTED_SYSCALL_02}
 *
 * @calls
 * - `ja_preempt_enable()`
 * - `f()` (indirect)
 * - `pok_partition_return_user()`
 */
static pok_ret_e unprotected_syscall(
   pok_ret_e (*f)(const pok_syscall_args_t* args),
   const pok_syscall_args_t*    args)
{
   pok_ret_e ret;
   ja_preempt_enable();
   ret = f(args);
   pok_partition_return_user();
   return ret;
}

/**
 * @file
 * The kernel's syscall entry and dispatch: each syscall ID goes to its
 * implementation (written by Julien Delange for POK).
 */

/**
 * @funcname `pok_core_syscall_internal`
 *
 * @description
 * Dispatches a syscall by its ID: the console, clock and time syscalls
 * directly, the others through their generated wrappers (SYSCALL_ENTRY), port
 * I/O within the partition's range, and the address translation syscalls. An
 * unknown ID is an illegal request.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `syscall_id` | `const pok_syscall_id_e` | [in] The syscall's ID. |
 * | `args` | `const pok_syscall_args_t *` | [in] The syscall's arguments. |
 * | `infos` | `const pok_syscall_info_t *` | [in] The calling partition (used by port I/O). |
 *
 * @return The syscall's result; POK_ERRNO_EINVAL for an unknown ID.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 * - \hlrid{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_01}:
 *   `pok_core_syscall_internal()` shall run the service that the identifier of
 *   a system call names, with its arguments, and give its result, for each
 *   system call that the kernel is built with.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_001}
 *   - \srqref{SRQ_JET_SYS_003}
 * - \hlrid{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_02}:
 *   `pok_core_syscall_internal()` shall treat an unknown identifier as an
 *   illegal request error of the calling partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_004}
 * - \hlrid{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_03}:
 *   `pok_core_syscall_internal()` shall handle the console write, clock and
 *   time calls itself.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_CONS_001}
 *   - \srqref{SRQ_JET_TIME_002}
 *   - \srqref{SRQ_JET_TIME_007}
 *   - \srqref{SRQ_JET_TIME_003}
 * - \hlrid{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}:
 *   `pok_core_syscall_internal()` shall let a partition read or write an I/O
 *   port only within the range that the configuration gives to the partition.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_008}
 * - \hlrid{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_05}:
 *   `pok_core_syscall_internal()` shall convert addresses of the calling
 *   partition between virtual and physical on request.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_MEM_011}
 *
 * @llr
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_01}: If `syscall_id` is
 *   `POK_SYSCALL_CONSWRITE` (built in with `POK_NEEDS_CONSOLE` or
 *   `POK_NEEDS_DEBUG`), `pok_core_syscall_internal()` shall return the result
 *   of `jet_console_write_user()` for `arg1` of `args` as a user address and
 *   `arg2` of `args`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_03}
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_02}: If `syscall_id` is
 *   `POK_SYSCALL_CLOCK_GETTIME` (built in with `POK_NEEDS_GETTICK`),
 *   `pok_core_syscall_internal()` shall return the result of
 *   `pok_clock_gettime()` for `arg1` of `args` as a clock and `arg2` of `args`
 *   as a user address.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_03}
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_03}: If `syscall_id` is
 *   `POK_SYSCALL_TIME`, `pok_core_syscall_internal()` shall return the result
 *   of `jet_time()` for `arg1` of `args` as an address.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_03}
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}: If `syscall_id` is
 *   `POK_SYSCALL_THREAD_CREATE`, `POK_SYSCALL_THREAD_SLEEP`,
 *   `POK_SYSCALL_THREAD_SLEEP_UNTIL`, `POK_SYSCALL_THREAD_PERIOD`,
 *   `POK_SYSCALL_THREAD_SUSPEND`, `POK_SYSCALL_THREAD_STATUS`,
 *   `POK_SYSCALL_THREAD_DELAYED_START`, `POK_SYSCALL_THREAD_SET_PRIORITY`,
 *   `POK_SYSCALL_THREAD_RESUME`, `POK_SYSCALL_THREAD_SUSPEND_TARGET`,
 *   `POK_SYSCALL_THREAD_YIELD`, `POK_SYSCALL_THREAD_REPLENISH`,
 *   `POK_SYSCALL_THREAD_STOP`, `POK_SYSCALL_THREAD_STOPSELF`,
 *   `POK_SYSCALL_THREAD_FIND`, `POK_SYSCALL_RESCHED`,
 *   `POK_SYSCALL_MSECTION_ENTER_HELPER`, `POK_SYSCALL_MSECTION_WAIT`,
 *   `POK_SYSCALL_MSECTION_NOTIFY`, `POK_SYSCALL_MSECTION_WQ_NOTIFY`,
 *   `POK_SYSCALL_MSECTION_WQ_SIZE`, `POK_SYSCALL_PARTITION_SET_MODE`,
 *   `POK_SYSCALL_PARTITION_GET_STATUS`, `POK_SYSCALL_PARTITION_INC_LOCK_LEVEL`,
 *   `POK_SYSCALL_PARTITION_DEC_LOCK_LEVEL`, `POK_SYSCALL_ERROR_HANDLER_CREATE`,
 *   `POK_SYSCALL_ERROR_HANDLER_CONFIGURE`,
 *   `POK_SYSCALL_ERROR_RAISE_APPLICATION_ERROR`, `POK_SYSCALL_ERROR_GET`,
 *   `POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE`,
 *   `POK_SYSCALL_ERROR_RAISE_OS_ERROR`,
 *   `POK_SYSCALL_MIDDLEWARE_SAMPLING_CREATE`,
 *   `POK_SYSCALL_MIDDLEWARE_SAMPLING_WRITE`,
 *   `POK_SYSCALL_MIDDLEWARE_SAMPLING_READ`,
 *   `POK_SYSCALL_MIDDLEWARE_SAMPLING_ID`,
 *   `POK_SYSCALL_MIDDLEWARE_SAMPLING_STATUS`,
 *   `POK_SYSCALL_MIDDLEWARE_SAMPLING_CHECK`,
 *   `POK_SYSCALL_MIDDLEWARE_QUEUEING_CREATE`,
 *   `POK_SYSCALL_MIDDLEWARE_QUEUEING_SEND`,
 *   `POK_SYSCALL_MIDDLEWARE_QUEUEING_RECEIVE`,
 *   `POK_SYSCALL_MIDDLEWARE_QUEUEING_ID`,
 *   `POK_SYSCALL_MIDDLEWARE_QUEUEING_STATUS`,
 *   `POK_SYSCALL_MIDDLEWARE_QUEUEING_CLEAR` or
 *   `POK_SYSCALL_MEMORY_BLOCK_GET_STATUS` (each built in as the configuration
 *   enables it), `pok_core_syscall_internal()` shall return the result of
 *   `unprotected_syscall()` for the wrapper of that identifier
 *   (`pok_syscall_wrapper_<id>`) and `args`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_01}
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_05}: If `syscall_id` is
 *   `POK_SYSCALL_INB` or `POK_SYSCALL_OUTB` (built in with `POK_NEEDS_IO`), and
 *   `arg1` of `args` is less than `io_min` or greater than `io_max` of the
 *   partition of `infos` (`pok_partitions[infos->partition]`),
 *   `pok_core_syscall_internal()` shall return `-POK_ERRNO_EPERM` without
 *   reading or writing the port.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_06}: If `syscall_id` is
 *   `POK_SYSCALL_INB` and `arg1` of `args` is from `io_min` to `io_max` of the
 *   partition of `infos`, `pok_core_syscall_internal()` shall return the byte
 *   that `inb()` reads from the port `arg1`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_07}: If `syscall_id` is
 *   `POK_SYSCALL_OUTB` and `arg1` of `args` is from `io_min` to `io_max` of the
 *   partition of `infos`, `pok_core_syscall_internal()` shall write `arg2` of
 *   `args` to the port `arg1` with `outb()` and return `POK_ERRNO_OK`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_08}: If `syscall_id` is
 *   `POK_SYSCALL_MEM_VIRT_TO_PHYS` or `POK_SYSCALL_MEM_PHYS_TO_VIRT`,
 *   `pok_core_syscall_internal()` shall return the result of
 *   `pok_virt_to_phys()` or `pok_phys_to_virt()`, in this order, for `arg1` of
 *   `args`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_05}
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_09}: If `syscall_id` is none
 *   of these identifiers, `pok_core_syscall_internal()` shall call `printf()`
 *   with `"Tried to use syscall %%d\n"` and `syscall_id` (with
 *   `POK_NEEDS_DEBUG`), then `pok_raise_error()` with
 *   `POK_ERROR_ID_ILLEGAL_REQUEST`, `TRUE` and NULL, and return
 *   `POK_ERRNO_EINVAL`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_02}
 *
 * @design
 * 1. Depending on `syscall_id`:
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_01},
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_02},
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_03},
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04},
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_05},
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_06},
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_07},
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_08},
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_09}
 *    1. Case `POK_SYSCALL_CONSWRITE`:
 *       \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_01}
 *       1. Call `jet_console_write_user()` with the arguments
 *          `(const char* JET_USER)args->arg1` and `args->arg2`, and return its
 *          result.
 *       2. Leave the switch.
 *    2. Case `POK_SYSCALL_CLOCK_GETTIME`:
 *       \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_02}
 *       1. Call `pok_clock_gettime()` with the arguments
 *          `(clockid_t)args->arg1` and `(pok_time_t* JET_USER)args->arg2`, and
 *          return its result.
 *       2. Leave the switch.
 *    3. Case `POK_SYSCALL_TIME`:
 *       \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_03}
 *       1. Call `jet_time()` with the argument `(time_t*)args->arg1`, and
 *          return its result.
 *    4. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_CREATE)`:
 *       \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *       1. Use the macro `SYSCALL_ENTRY()` with the argument
 *          `POK_SYSCALL_THREAD_CREATE`.
 *       2. Leave the switch.
 *    5. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_SLEEP)`:
 *       \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *       1. Use the macro `SYSCALL_ENTRY()` with the argument
 *          `POK_SYSCALL_THREAD_SLEEP`.
 *       2. Leave the switch.
 *    6. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_SLEEP_UNTIL)`:
 *       \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *       1. Use the macro `SYSCALL_ENTRY()` with the argument
 *          `POK_SYSCALL_THREAD_SLEEP_UNTIL`.
 *       2. Leave the switch.
 *    7. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_PERIOD)`:
 *       \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *       1. Use the macro `SYSCALL_ENTRY()` with the argument
 *          `POK_SYSCALL_THREAD_PERIOD`.
 *       2. Leave the switch.
 *    8. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_SUSPEND)`:
 *       \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *       1. Use the macro `SYSCALL_ENTRY()` with the argument
 *          `POK_SYSCALL_THREAD_SUSPEND`.
 *       2. Leave the switch.
 *    9. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_STATUS)`:
 *       \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *       1. Use the macro `SYSCALL_ENTRY()` with the argument
 *          `POK_SYSCALL_THREAD_STATUS`.
 *       2. Leave the switch.
 *    10. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_DELAYED_START)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_THREAD_DELAYED_START`.
 *        2. Leave the switch.
 *    11. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_SET_PRIORITY)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_THREAD_SET_PRIORITY`.
 *        2. Leave the switch.
 *    12. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_RESUME)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_THREAD_RESUME`.
 *        2. Leave the switch.
 *    13. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_SUSPEND_TARGET)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_THREAD_SUSPEND_TARGET`.
 *        2. Leave the switch.
 *    14. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_YIELD)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_THREAD_YIELD`.
 *        2. Leave the switch.
 *    15. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_REPLENISH)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_THREAD_REPLENISH`.
 *        2. Leave the switch.
 *    16. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_STOP)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_THREAD_STOP`.
 *        2. Leave the switch.
 *    17. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_STOPSELF)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_THREAD_STOPSELF`.
 *        2. Leave the switch.
 *    18. Case `SYSCALL_ENTRY(POK_SYSCALL_THREAD_FIND)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_THREAD_FIND`.
 *        2. Leave the switch.
 *    19. Case `SYSCALL_ENTRY(POK_SYSCALL_RESCHED)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_RESCHED`.
 *        2. Leave the switch.
 *    20. Case `SYSCALL_ENTRY(POK_SYSCALL_MSECTION_ENTER_HELPER)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MSECTION_ENTER_HELPER`.
 *        2. Leave the switch.
 *    21. Case `SYSCALL_ENTRY(POK_SYSCALL_MSECTION_WAIT)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MSECTION_WAIT`.
 *        2. Leave the switch.
 *    22. Case `SYSCALL_ENTRY(POK_SYSCALL_MSECTION_NOTIFY)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MSECTION_NOTIFY`.
 *        2. Leave the switch.
 *    23. Case `SYSCALL_ENTRY(POK_SYSCALL_MSECTION_WQ_NOTIFY)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MSECTION_WQ_NOTIFY`.
 *        2. Leave the switch.
 *    24. Case `SYSCALL_ENTRY(POK_SYSCALL_MSECTION_WQ_SIZE)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MSECTION_WQ_SIZE`.
 *        2. Leave the switch.
 *    25. Case `SYSCALL_ENTRY(POK_SYSCALL_PARTITION_SET_MODE)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_PARTITION_SET_MODE`.
 *        2. Leave the switch.
 *    26. Case `SYSCALL_ENTRY(POK_SYSCALL_PARTITION_GET_STATUS)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_PARTITION_GET_STATUS`.
 *        2. Leave the switch.
 *    27. Case `SYSCALL_ENTRY(POK_SYSCALL_PARTITION_INC_LOCK_LEVEL)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_PARTITION_INC_LOCK_LEVEL`.
 *        2. Leave the switch.
 *    28. Case `SYSCALL_ENTRY(POK_SYSCALL_PARTITION_DEC_LOCK_LEVEL)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_PARTITION_DEC_LOCK_LEVEL`.
 *        2. Leave the switch.
 *    29. Case `SYSCALL_ENTRY(POK_SYSCALL_ERROR_HANDLER_CREATE)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_ERROR_HANDLER_CREATE`.
 *        2. Leave the switch.
 *    30. Case `SYSCALL_ENTRY(POK_SYSCALL_ERROR_HANDLER_CONFIGURE)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_ERROR_HANDLER_CONFIGURE`.
 *        2. Leave the switch.
 *    31. Case `SYSCALL_ENTRY(POK_SYSCALL_ERROR_RAISE_APPLICATION_ERROR)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_ERROR_RAISE_APPLICATION_ERROR`.
 *        2. Leave the switch.
 *    32. Case `SYSCALL_ENTRY(POK_SYSCALL_ERROR_GET)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_ERROR_GET`.
 *        2. Leave the switch.
 *    33. Case `SYSCALL_ENTRY(POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE`.
 *        2. Leave the switch.
 *    34. Case `SYSCALL_ENTRY(POK_SYSCALL_ERROR_RAISE_OS_ERROR)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_ERROR_RAISE_OS_ERROR`.
 *        2. Leave the switch.
 *    35. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_CREATE)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_SAMPLING_CREATE`.
 *        2. Leave the switch.
 *    36. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_WRITE)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_SAMPLING_WRITE`.
 *        2. Leave the switch.
 *    37. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_READ)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_SAMPLING_READ`.
 *        2. Leave the switch.
 *    38. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_ID)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_SAMPLING_ID`.
 *        2. Leave the switch.
 *    39. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_STATUS)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_SAMPLING_STATUS`.
 *        2. Leave the switch.
 *    40. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_CHECK)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_SAMPLING_CHECK`.
 *        2. Leave the switch.
 *    41. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_CREATE)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_QUEUEING_CREATE`.
 *        2. Leave the switch.
 *    42. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_SEND)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_QUEUEING_SEND`.
 *        2. Leave the switch.
 *    43. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_RECEIVE)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_QUEUEING_RECEIVE`.
 *        2. Leave the switch.
 *    44. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_ID)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_QUEUEING_ID`.
 *        2. Leave the switch.
 *    45. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_STATUS)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_QUEUEING_STATUS`.
 *        2. Leave the switch.
 *    46. Case `SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_CLEAR)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MIDDLEWARE_QUEUEING_CLEAR`.
 *        2. Leave the switch.
 *    47. Case `POK_SYSCALL_INB`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_05},
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_06}
 *        1. If `(args->arg1 < pok_partitions[infos->partition].io_min) ||`
 *           `(args->arg1 > pok_partitions[infos->partition].io_max)` is true:
 *           \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_05}
 *           1. Return `-POK_ERRNO_EPERM`.
 *        2. Otherwise: \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_06}
 *           1. Return `inb((uint16_t) args->arg1)`.
 *        3. Leave the switch.
 *    48. Case `POK_SYSCALL_OUTB`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_05},
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_07}
 *        1. If `(args->arg1 < pok_partitions[infos->partition].io_min) ||`
 *           `(args->arg1 > pok_partitions[infos->partition].io_max)` is true:
 *           \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_05}
 *           1. Return `-POK_ERRNO_EPERM`.
 *        2. Otherwise: \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_07}
 *           1. Use the macro
 *              `outb((uint16_t) args->arg1, (uint8_t) args->arg2)`.
 *           2. Return `POK_ERRNO_OK`.
 *        3. Leave the switch.
 *    49. Case `POK_SYSCALL_MEM_VIRT_TO_PHYS`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_08}
 *        1. Call `pok_virt_to_phys()` with the argument `args->arg1`, and
 *           return its result.
 *        2. Leave the switch.
 *    50. Case `POK_SYSCALL_MEM_PHYS_TO_VIRT`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_08}
 *        1. Call `pok_phys_to_virt()` with the argument `args->arg1`, and
 *           return its result.
 *        2. Leave the switch.
 *    51. Case `SYSCALL_ENTRY(POK_SYSCALL_MEMORY_BLOCK_GET_STATUS)`:
 *        \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_04}
 *        1. Use the macro `SYSCALL_ENTRY()` with the argument
 *           `POK_SYSCALL_MEMORY_BLOCK_GET_STATUS`.
 *        2. Leave the switch.
 *    52. Otherwise (default): \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_09}
 *        1. Call `printf()` with the arguments `"Tried to use syscall %%d\n"`
 *           and `syscall_id`.
 *        2. Call `pok_raise_error()` with the arguments
 *           `POK_ERROR_ID_ILLEGAL_REQUEST`, `TRUE` and `NULL`.
 *        3. Leave the switch.
 * 2. Return `POK_ERRNO_EINVAL`.
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_INTERNAL_09}
 *
 * @calls
 * - `jet_console_write_user()`
 * - `pok_clock_gettime()`
 * - `jet_time()`
 * - `unprotected_syscall()`
 * - `x86_inb()`
 * - `x86_outb()`
 * - `pok_virt_to_phys()`
 * - `pok_phys_to_virt()`
 * - `printf()`
 * - `pok_raise_error()`
 */
/*
 * MISRA C:2025 Rule 11.4 and Rule 11.6 deviation: syscall arguments arrive in
 * registers as integers (pok_syscall_args_t) and are converted to the
 * pointer types of the implementations, here and in the generated
 * wrappers. The implementations check user pointers (see uaccess.h).
 */
static inline pok_ret_e pok_core_syscall_internal (
                            const pok_syscall_id_e       syscall_id,
                            const pok_syscall_args_t*    args,
                            const pok_syscall_info_t*    infos)
{
   (void) infos;

   switch (syscall_id)
   {
#if defined (POK_NEEDS_CONSOLE) || defined (POK_NEEDS_DEBUG)
      case POK_SYSCALL_CONSWRITE:
         return jet_console_write_user((const char* JET_USER)args->arg1,
                                       args->arg2);
         break;
#endif

#if defined POK_NEEDS_GETTICK
      case POK_SYSCALL_CLOCK_GETTIME:
         return pok_clock_gettime ((clockid_t)args->arg1,
                                   (pok_time_t* JET_USER)args->arg2);
         break;
#endif
      case POK_SYSCALL_TIME:
         return jet_time((time_t*)args->arg1);

      SYSCALL_ENTRY(POK_SYSCALL_THREAD_CREATE)

#ifdef POK_NEEDS_THREAD_SLEEP
      SYSCALL_ENTRY(POK_SYSCALL_THREAD_SLEEP)
#endif

#ifdef POK_NEEDS_THREAD_SLEEP_UNTIL
      SYSCALL_ENTRY(POK_SYSCALL_THREAD_SLEEP_UNTIL)
#endif

      SYSCALL_ENTRY(POK_SYSCALL_THREAD_PERIOD)

#if defined (POK_NEEDS_THREAD_SUSPEND) || defined (POK_NEEDS_ERROR_HANDLING)
      SYSCALL_ENTRY(POK_SYSCALL_THREAD_SUSPEND)
#endif

   SYSCALL_ENTRY(POK_SYSCALL_THREAD_STATUS)
   SYSCALL_ENTRY(POK_SYSCALL_THREAD_DELAYED_START)
   SYSCALL_ENTRY(POK_SYSCALL_THREAD_SET_PRIORITY)
   SYSCALL_ENTRY(POK_SYSCALL_THREAD_RESUME)
   SYSCALL_ENTRY(POK_SYSCALL_THREAD_SUSPEND_TARGET)
   SYSCALL_ENTRY(POK_SYSCALL_THREAD_YIELD)
   SYSCALL_ENTRY(POK_SYSCALL_THREAD_REPLENISH)
   SYSCALL_ENTRY(POK_SYSCALL_THREAD_STOP)
   SYSCALL_ENTRY(POK_SYSCALL_THREAD_STOPSELF)

   SYSCALL_ENTRY(POK_SYSCALL_THREAD_FIND)

   SYSCALL_ENTRY(POK_SYSCALL_RESCHED)
   SYSCALL_ENTRY(POK_SYSCALL_MSECTION_ENTER_HELPER)
   SYSCALL_ENTRY(POK_SYSCALL_MSECTION_WAIT)
   SYSCALL_ENTRY(POK_SYSCALL_MSECTION_NOTIFY)
   SYSCALL_ENTRY(POK_SYSCALL_MSECTION_WQ_NOTIFY)
   SYSCALL_ENTRY(POK_SYSCALL_MSECTION_WQ_SIZE)

#ifdef POK_NEEDS_PARTITIONS
   SYSCALL_ENTRY(POK_SYSCALL_PARTITION_SET_MODE)
   SYSCALL_ENTRY(POK_SYSCALL_PARTITION_GET_STATUS)
   SYSCALL_ENTRY(POK_SYSCALL_PARTITION_INC_LOCK_LEVEL)
   SYSCALL_ENTRY(POK_SYSCALL_PARTITION_DEC_LOCK_LEVEL)
#endif

#ifdef POK_NEEDS_ERROR_HANDLING
   SYSCALL_ENTRY(POK_SYSCALL_ERROR_HANDLER_CREATE)
   SYSCALL_ENTRY(POK_SYSCALL_ERROR_HANDLER_CONFIGURE)
   SYSCALL_ENTRY(POK_SYSCALL_ERROR_RAISE_APPLICATION_ERROR)
   SYSCALL_ENTRY(POK_SYSCALL_ERROR_GET)
   SYSCALL_ENTRY(POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE)
#endif

   SYSCALL_ENTRY(POK_SYSCALL_ERROR_RAISE_OS_ERROR)

   /* Middleware syscalls */
#ifdef POK_NEEDS_PORTS_SAMPLING
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_CREATE)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_WRITE)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_READ)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_ID)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_STATUS)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_SAMPLING_CHECK)
#endif /* POK_NEEDS_PORTS_SAMPLING */


#ifdef POK_NEEDS_PORTS_QUEUEING
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_CREATE)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_SEND)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_RECEIVE)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_ID)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_STATUS)
   SYSCALL_ENTRY(POK_SYSCALL_MIDDLEWARE_QUEUEING_CLEAR)
#endif /* POK_NEEDS_PORTS_QUEUEING */

#ifdef POK_NEEDS_IO
      case POK_SYSCALL_INB:
         if ((args->arg1 < pok_partitions[infos->partition].io_min) ||
             (args->arg1 > pok_partitions[infos->partition].io_max))
         {
            return -POK_ERRNO_EPERM;
         }
         else
         {
            return inb((uint16_t) args->arg1);
         }
         break;

      case POK_SYSCALL_OUTB:
         if ((args->arg1 < pok_partitions[infos->partition].io_min) ||
             (args->arg1 > pok_partitions[infos->partition].io_max))
         {
            return -POK_ERRNO_EPERM;
         }
         else
         {
            outb((uint16_t) args->arg1, (uint8_t) args->arg2);
            return POK_ERRNO_OK;
         }
       break;
#endif /* POK_NEEDS_IO */

      //TODO rewrite this! This two syscall needs to return pok_ret_e!
      case POK_SYSCALL_MEM_VIRT_TO_PHYS:
         return pok_virt_to_phys(args->arg1);
         break;
      case POK_SYSCALL_MEM_PHYS_TO_VIRT:
         return pok_phys_to_virt(args->arg1);
         break;

      SYSCALL_ENTRY(POK_SYSCALL_MEMORY_BLOCK_GET_STATUS)

      default:
       /*
        * Unrecognized system call ID.
        */
#ifdef POK_NEEDS_DEBUG
            (void) printf ("Tried to use syscall %d\n", syscall_id);
#endif
         pok_raise_error(POK_ERROR_ID_ILLEGAL_REQUEST, TRUE, NULL);
         break;
   }

   return POK_ERRNO_EINVAL; // TODO: Unreachable?
}

/**
 * @funcname `pok_core_syscall`
 *
 * @description
 * The kernel's syscall entry (called by the architecture's trap handler): with
 * the GDB server, records the user stack and that the CPU is in the kernel
 * around the dispatch.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `syscall_id` | `const pok_syscall_id_e` | [in] The syscall's ID. |
 * | `args` | `const pok_syscall_args_t *` | [in] The syscall's arguments. |
 * | `infos` | `const pok_syscall_info_t *` | [in] The calling partition. |
 *
 * @return The syscall's result.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `current_partition` | `pok_partition_t *` | The running partition |
 * | `global_thread_stack` | `interrupt_frame *` | The user stack pointer at the trap (GDB) |
 * | `pok_in_user_space` | `volatile pok_bool_t` | Whether the CPU runs user code (GDB) |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `ret` | `pok_ret_e` | The result |
 *
 * @hlr
 * - \hlrid{HLR_SYSCALL_POK_CORE_SYSCALL_01}: `pok_core_syscall()` shall be the
 *   entry of the kernel for a system call from the trap handler of the
 *   architecture, and give the result of the call.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_SYS_002}
 *   - \srqref{SRQ_JET_SYS_009}
 * - \hlrid{HLR_SYSCALL_POK_CORE_SYSCALL_02}: `pok_core_syscall()` shall let the
 *   GDB server see, while a call runs, the stack of the partition and that the
 *   processor is in the kernel.
 *   <br>Traces to:
 *   - \srqref{SRQ_JET_DBG_004}
 *
 * @llr
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_01}: `pok_core_syscall()` shall return
 *   the result of `pok_core_syscall_internal()` for `syscall_id`, `args` and
 *   `infos`.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_01}
 * - \llrid{LLR_SYSCALL_POK_CORE_SYSCALL_02}: With `POK_NEEDS_GDB`,
 *   `pok_core_syscall()` shall set `entry_sp_user` of `current_partition` to
 *   `global_thread_stack` and `pok_in_user_space` to `FALSE` before that call,
 *   and `pok_in_user_space` to `TRUE` after it.
 *   <br>Traces to:
 *   - \hlrref{HLR_SYSCALL_POK_CORE_SYSCALL_02}
 *
 * @design
 * 1. Set `current_partition->entry_sp_user` to `global_thread_stack`.
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_02}
 * 2. Set `pok_in_user_space` to `FALSE`.
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_02}
 * 3. Call `pok_core_syscall_internal()` with the arguments `syscall_id`, `args`
 *    and `infos`, and store the result in `ret`.
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_01}
 * 4. Set `pok_in_user_space` to `TRUE`.
 *    \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_02}
 * 5. Return `ret`. \llrref{LLR_SYSCALL_POK_CORE_SYSCALL_01}
 *
 * @calls
 * - `pok_core_syscall_internal()`
 */
pok_ret_e pok_core_syscall (const pok_syscall_id_e       syscall_id,
                            const pok_syscall_args_t*    args,
                            const pok_syscall_info_t*    infos)
{
    pok_ret_e ret;
#ifdef POK_NEEDS_GDB
    current_partition->entry_sp_user = global_thread_stack;
    pok_in_user_space = FALSE;
#endif

    ret = pok_core_syscall_internal(syscall_id, args, infos);

#ifdef POK_NEEDS_GDB
    pok_in_user_space = TRUE;
#endif

    return ret;
}
